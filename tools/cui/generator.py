"""
generator.py - Converts a parsed .cui AST into C++ source files.
"""

from __future__ import annotations
from pathlib import Path
from typing import Optional
from scanner import Registry, ComponentInfo, FunctionInfo
from ast_nodes import (
    File, ViewDecl, WrappedField, LetDecl, FuncDecl, Param, CppBlock,
    Element, Modifier,
    StringLit, StringSegment, InterpSegment,
    NumberLit, BoolLit, Identifier, EnumCase,
    PropertyAccess, CallExpr, BinaryExpr, CaptureExpr,
)

# DSL type → C++ type
DSL_TYPES = {
    "int":          "int",
    "float":        "float",
    "double":       "double",
    "bool":         "bool",
    "string":       "std::string",
    "nanoseconds":  "std::chrono::nanoseconds",
    "seconds":      "std::chrono::duration<double>",
    "Window":       "Window",
    "Profiler":     "Profiler",
}

# DSL modifier names that take a Bounds frame
FRAME_MODIFIER = "frame"


class GenError(Exception):
    pass


class Generator:
    def __init__(self, registry: Registry, source_file: str = "<cui>"):
        self._reg   = registry
        self._src   = source_file
        self._lines: list[str] = []
        self._indent = 0

    # ── public API ────────────────────────────────────────────────────────────

    def generate(self, ast: File) -> tuple[str, str]:
        """Returns (header_source, impl_source)."""
        header_lines: list[str] = []
        impl_lines:   list[str] = []

        for view in ast.views:
            h, cpp = self._generate_view(view, ast.imports)
            header_lines.extend(h)
            impl_lines.extend(cpp)

        header = "\n".join(header_lines)
        impl   = "\n".join(impl_lines)
        return header, impl

    # ── view ──────────────────────────────────────────────────────────────────

    def _generate_view(self, view: ViewDecl, imports: list) -> tuple[list[str], list[str]]:
        observed = [f for f in view.fields if isinstance(f, WrappedField) and f.wrapper == "@Observed"]
        mutable_ = [f for f in view.fields if isinstance(f, WrappedField) and f.wrapper == "@Mutable"]
        snapshot = [f for f in view.fields if isinstance(f, WrappedField) and f.wrapper == "@Snapshot"]
        states   = [f for f in view.fields if isinstance(f, WrappedField) and f.wrapper == "@State"]
        lets     = [f for f in view.fields if isinstance(f, LetDecl)]
        funcs    = [f for f in view.fields if isinstance(f, FuncDecl)]
        cpp_blks = [f for f in view.fields if isinstance(f, CppBlock)]

        # Collect includes
        includes = set([
            "UI/View.h",
            "UI/Utils/TextContent.h",
        ])
        for f in observed + mutable_ + snapshot:
            includes |= self._includes_for_type(f.type_)
        for f in funcs:
            includes |= self._includes_for_type(f.return_type)
        includes |= self._includes_for_element(view.root)
        includes |= self._includes_for_imports(imports)

        # ── header (.gen.h) ───────────────────────────────────────────────────
        h: list[str] = []
        h.append(f"// AUTO-GENERATED — do not edit. Source: {self._src}")
        h.append("#pragma once")
        h.append("")
        for inc in sorted(includes):
            h.append(f'#include "{inc}"')
        h.append("")
        h.append("namespace ui::generated")
        h.append("{")
        h.append("")
        h.append(f"class {view.name} : public UI::View")
        h.append("{")
        h.append("public:")
        ctor_params = self._ctor_params(observed, mutable_, snapshot)
        h.append(f"    explicit {view.name}({ctor_params});")
        h.append("")
        h.append("protected:")
        h.append("    void Build() override;")
        h.append("")
        h.append("private:")
        for f in observed:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            h.append(f"    const {cpp_type}* {f.name} = nullptr;")   # @Observed → const
        for f in mutable_:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            h.append(f"    {cpp_type}* {f.name} = nullptr;")           # @Mutable → non-const
        for f in snapshot:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            h.append(f"    {cpp_type} {f.name}{{}};")
        for f in states:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            default = self._gen_expr(f.default) if f.default else ""
            init = f" = {default}" if default else ""
            h.append(f"    {cpp_type} {f.name}{init};")
            setter = "Set" + f.name[0].upper() + f.name[1:]
            h.append(f"    void {setter}({cpp_type} v) {{ {f.name} = v; MarkFullDirty(); }}")
        for fn in funcs:
            h.append(f"    {self._gen_func_sig(fn, static=True)};")
        for blk in cpp_blks:
            h.append(f"    // @cui-cpp block")
            h.append(f"    {blk.content}")
        h.append("};")
        h.append("")
        # Factory declaration
        h.append(f"std::shared_ptr<{view.name}> Create{view.name}(")
        h.append(f"    {ctor_params});")
        h.append("")
        h.append("} // namespace ui::generated")

        # ── implementation (.gen.cpp) ─────────────────────────────────────────
        cpp: list[str] = []
        gen_h_name = view.name + ".gen.h"
        cpp.append(f"// AUTO-GENERATED — do not edit. Source: {self._src}")
        cpp.append(f'#include "{gen_h_name}"')
        cpp.append("")
        cpp.append("namespace ui::generated")
        cpp.append("{")
        cpp.append("")
        # Constructor (no default values in definition)
        ctor_params_nodef = self._ctor_params(observed, mutable_, snapshot, with_defaults=False)
        ctor_init = self._ctor_init(observed, mutable_, snapshot)
        cpp.append(f"{view.name}::{view.name}({ctor_params_nodef})")
        cpp.append(f"    : UI::View(bounds, false){ctor_init}")
        cpp.append("{")
        cpp.append("    DoSetIdentifierKind(UI::IdentifierKind::TRANSPARENT);")
        cpp.append("}")
        cpp.append("")
        # Build()
        cpp.append(f"void {view.name}::Build()")
        cpp.append("{")

        # let constants
        for let in lets:
            cpp_type = self._dsl_to_cpp_type(let.type_) if let.type_ else "auto"
            if cpp_type == "auto":
                cpp_type = "constexpr auto"
            elif cpp_type in ("float", "double", "int"):
                cpp_type = f"constexpr {cpp_type}"
            val = self._gen_expr(let.value)
            suffix = "f" if cpp_type.endswith("float") and "." in str(val) else ""
            cpp.append(f"    {cpp_type} {let.name} = {val}{suffix};")

        if lets:
            cpp.append("")

        # Root element
        root_code = self._gen_element(view, observed, mutable_, states)
        for line in root_code.splitlines():
            cpp.append("    " + line)
        cpp.append("}")
        cpp.append("")
        # func definitions
        for fn in funcs:
            sig = self._gen_func_sig(fn, static=True, qualified=view.name + "::")
            body = self._gen_expr(fn.body)
            cpp.append(f"{sig}")
            cpp.append("{")
            cpp.append(f"    return {body};")
            cpp.append("}")
            cpp.append("")
        # Factory (no default values in definition)
        factory_sig = f"std::shared_ptr<{view.name}> Create{view.name}(\n    {ctor_params_nodef})"
        factory_init = self._factory_args(observed, mutable_, snapshot)
        cpp.append(factory_sig)
        cpp.append("{")
        cpp.append(f"    return std::make_shared<{view.name}>({factory_init});")
        cpp.append("}")
        cpp.append("")
        cpp.append("} // namespace ui::generated")

        return h, cpp

    # ── element code generation ───────────────────────────────────────────────

    def _gen_element(self, view: ViewDecl, observed: list[WrappedField],
                     mutable_: list[WrappedField], states: list[WrappedField] = None) -> str:
        """Returns C++ code for the root element, as AddChild call (Build is void)."""
        ctx = _GenContext(view, observed, mutable_, states or [], self._reg)
        code = ctx.gen_element(view.root, is_root=True)
        return f"AddChild({code});"

    # ── expression code generation ────────────────────────────────────────────

    def _gen_expr(self, node) -> str:
        return _GenContext(None, [], [], [], self._reg).gen_expr(node)

    # ── helpers ───────────────────────────────────────────────────────────────

    def _ctor_params(self, observed: list[WrappedField], mutable_: list[WrappedField],
                     snapshot: list[WrappedField], with_defaults: bool = True) -> str:
        if with_defaults:
            params = ["UI::Bounds bounds = UI::Bounds()"]
        else:
            params = ["UI::Bounds bounds"]
        for f in observed:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            params.append(f"const {cpp_type}* {f.name}" + (" = nullptr" if with_defaults else ""))
        for f in mutable_:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            params.append(f"{cpp_type}* {f.name}" + (" = nullptr" if with_defaults else ""))
        for f in snapshot:
            cpp_type = self._dsl_to_cpp_type(f.type_)
            params.append(f"{cpp_type} {f.name}" + (" = {{}}" if with_defaults else ""))
        return ", ".join(params)

    def _ctor_init(self, observed: list[WrappedField], mutable_: list[WrappedField],
                   snapshot: list[WrappedField]) -> str:
        inits = []
        for f in observed + mutable_ + snapshot:
            inits.append(f"{f.name}({f.name})")
        return (", " + ", ".join(inits)) if inits else ""

    def _factory_args(self, observed: list[WrappedField], mutable_: list[WrappedField],
                      snapshot: list[WrappedField]) -> str:
        args = ["bounds"]
        for f in observed + mutable_ + snapshot:
            args.append(f.name)
        return ", ".join(args)

    def _gen_func_sig(self, fn: FuncDecl, static: bool = False, qualified: str = "") -> str:
        ret = self._dsl_to_cpp_type(fn.return_type)
        params = ", ".join(f"{self._dsl_to_cpp_type(p.type_)} {p.name}" for p in fn.params)
        prefix = "static " if static and not qualified else ""
        return f"{prefix}{ret} {qualified}{fn.name}({params})"

    @staticmethod
    def _dsl_to_cpp_type(dsl_type: str) -> str:
        return DSL_TYPES.get(dsl_type, dsl_type)

    @staticmethod
    def _includes_for_type(type_: str) -> set[str]:
        incs = set()
        if "nanoseconds" in type_ or "chrono" in type_:
            incs.add("chrono")
        if "Window" in type_:
            incs.add("Graphics/Window.h")
        if "Profiler" in type_:
            incs.add("Profiler/Profiler.h")
        return incs

    def _includes_for_element(self, el: Element) -> set[str]:
        incs = set()
        name = el.name
        if name in self._reg.components:
            inc = self._reg.components[name].include_path
            if inc:
                incs.add(inc)
        for child in el.children:
            incs |= self._includes_for_element(child)
        return incs

    @staticmethod
    def _includes_for_imports(imports) -> set[str]:
        return {i.path for i in imports}


# ── Generation context (per-view) ─────────────────────────────────────────────

class _GenContext:
    """Stateful helper for generating code within a single view."""

    def __init__(self, view, observed: list[WrappedField], mutable_: list[WrappedField],
                 states: list[WrappedField], registry: Registry):
        self._view     = view
        self._observed = {f.name for f in (observed or [])}
        self._mutable  = {f.name for f in (mutable_ or [])}
        self._states   = {f.name for f in (states or [])}
        self._reg      = registry
        # Map field name → WrappedField for type inference
        self._fields: dict[str, WrappedField] = {}
        if view is not None:
            for f in view.fields:
                if isinstance(f, WrappedField):
                    self._fields[f.name] = f

    def _get_field_cpp_type(self, field_name: str) -> str:
        f = self._fields.get(field_name)
        if f is None:
            return ""
        return Generator._dsl_to_cpp_type(f.type_)

    @staticmethod
    def _infer_getter(prop: str) -> str:
        """DSL property name → C++ getter: averageFPS → GetAverageFPS"""
        return "Get" + prop[0].upper() + prop[1:]

    def gen_element(self, el: Element, is_root: bool = False) -> str:
        comp = self._reg.components.get(el.name)
        factory = comp.factory if comp else f"UI::Create{el.name}"

        # Check for .frame() modifier to extract Bounds
        bounds = self._extract_frame_bounds(el.modifiers)
        bounds_arg = bounds if bounds else "UI::Bounds()"

        if comp and comp.content_model == "text_content":
            return self._gen_text_element(el, factory, bounds_arg)
        else:
            return self._gen_container_element(el, factory, bounds_arg, comp)

    def _gen_text_element(self, el: Element, factory: str, bounds_arg: str) -> str:
        # First positional arg is a string literal → TextContent
        if el.args:
            tc = self._gen_text_content(el.args[0])
        else:
            tc = 'UI::TextContent()'

        scale_arg = ""
        non_frame_mods = [m for m in el.modifiers if m.name != FRAME_MODIFIER]
        lines = [f"{factory}({bounds_arg}, {tc})"]
        for mod in non_frame_mods:
            setter = self._resolve_modifier(el.name, mod)
            args   = self._gen_mod_args(mod)
            lines[-1] = lines[-1] + f"\n    ->{setter}({args})"
        return "\n".join(lines)

    def _gen_container_element(self, el: Element, factory: str, bounds_arg: str,
                                comp) -> str:
        has_children = bool(el.children)

        if has_children:
            # CreateVBox(bounds)->SetPadding(x)->...->AddChild(...);
            non_frame_mods = [m for m in el.modifiers if m.name != FRAME_MODIFIER]
            lines = [f"[&]() {{"]
            lines.append(f"    auto _root = {factory}({bounds_arg});")
            for mod in non_frame_mods:
                setter = self._resolve_modifier(el.name, mod)
                args   = self._gen_mod_args(mod)
                lines.append(f"    _root->{setter}({args});")
            for child in el.children:
                child_code = self.gen_element(child)
                lines.append(f"    _root->AddChild({child_code});")
            lines.append("    return _root;")
            lines.append("}()")
            return "\n".join(lines)
        else:
            non_frame_mods = [m for m in el.modifiers if m.name != FRAME_MODIFIER]
            line = f"{factory}({bounds_arg})"
            for mod in non_frame_mods:
                setter = self._resolve_modifier(el.name, mod)
                args   = self._gen_mod_args(mod)
                line += f"\n    ->{setter}({args})"
            return line

    def _gen_text_content(self, node) -> str:
        """Generate UI::TextContent(...) from a StringLit or plain expression."""
        if not isinstance(node, StringLit):
            return f"UI::TextContent({self.gen_expr(node)})"

        parts = []
        for seg in node.segments:
            if isinstance(seg, StringSegment):
                if seg.text:
                    parts.append(f'"{_escape(seg.text)}"')
            elif isinstance(seg, InterpSegment):
                capture = seg.capture
                if capture == "once":
                    # Evaluate once: use a static copy
                    parts.append(f"UI::BindStatic({self.gen_expr(seg.expr)})")
                elif capture == "always":
                    # Always re-evaluate
                    parts.append(f"UI::BindAlways({self.gen_expr_raw(seg.expr)})")
                else:
                    # Reactive binding
                    parts.append(self._gen_binding(seg.expr))

        if not parts:
            return 'UI::TextContent("")'
        return f"UI::TextContent({', '.join(parts)})"

    def _gen_binding(self, node) -> str:
        """Generate the reactive binding expression for an interpolated expression."""
        if isinstance(node, PropertyAccess):
            obj, prop = node.obj, node.prop
            fn_key = f"{obj}.{prop}"
            fn_info = self._reg.functions.get(fn_key)
            if fn_info and fn_info.is_getter:
                if obj in self._observed or obj in self._mutable:
                    return f"UI::Bind({obj}, &{fn_info.cpp_qualified})"
                return f"UI::Call(&{fn_info.cpp_qualified})"
            # Inference fallback: obj is a tracked pointer field
            if obj in self._observed or obj in self._mutable:
                cpp_type = self._get_field_cpp_type(obj)
                getter = self._infer_getter(prop)
                if cpp_type:
                    return f"UI::Bind({obj}, &{cpp_type}::{getter})"
                return f"UI::Bind({obj}, &decltype(*{obj})::{getter})"
            return self.gen_expr(node)

        if isinstance(node, CallExpr):
            callee = node.callee
            # Local func calls: toMs(...)
            if "." not in callee:
                inner_args = [self._gen_binding(a) for a in node.args]
                return f"UI::Call(&{callee}, {', '.join(inner_args)})"
            # Qualified: Profiler.averageTime(...)
            owner, method = callee.rsplit(".", 1)
            fn_info = self._reg.functions.get(callee)
            if fn_info:
                cpp_args = [self.gen_expr(a) for a in node.args]
                if fn_info.is_volatile:
                    inner = self.gen_expr(node)
                    return f"UI::BindAlways([&]() {{ return {inner}; }})"
                if fn_info.is_instance and (owner in self._observed or owner in self._mutable):
                    return f"UI::Bind({owner}, &{fn_info.cpp_qualified}{', ' + ', '.join(cpp_args) if cpp_args else ''})"
                return f"UI::Call(&{fn_info.cpp_qualified}{', ' + ', '.join(cpp_args) if cpp_args else ''})"
            # Inference fallback: apply GetXxx naming convention
            cpp_args = [self.gen_expr(a) for a in node.args]
            getter = self._infer_getter(method)
            if owner in self._observed or owner in self._mutable:
                cpp_type = self._get_field_cpp_type(owner)
                if cpp_type:
                    return f"UI::Bind({owner}, &{cpp_type}::{getter}{', ' + ', '.join(cpp_args) if cpp_args else ''})"
            if owner and owner[0].isupper():
                return f"UI::Call(&{owner}::{getter}{', ' + ', '.join(cpp_args) if cpp_args else ''})"
            # Local variable — plain method call (not reactive)
            suffix = f"({', '.join(cpp_args)})"
            return f"{owner}.{method}{suffix}"

        # Identifier (let, param, @Observed / @Mutable / @State field)
        if isinstance(node, Identifier):
            if node.name in self._observed or node.name in self._mutable:
                return f"UI::Bind({node.name})"
            if node.name in self._states:
                return f"UI::BindAlways([this]() {{ return {node.name}; }})"
            return node.name

        return self.gen_expr(node)

    def gen_expr_raw(self, node) -> str:
        """Generate a lambda-wrapped expression for BindAlways."""
        inner = self.gen_expr(node)
        return f"[&]() {{ return {inner}; }}"

    def gen_expr(self, node) -> str:
        """Generate a plain C++ expression (no Bind wrapping)."""
        if node is None:
            return ""
        if isinstance(node, NumberLit):
            v = node.value
            if v == int(v):
                return str(int(v))
            return str(v)
        if isinstance(node, BoolLit):
            return "true" if node.value else "false"
        if isinstance(node, Identifier):
            return node.name
        if isinstance(node, EnumCase):
            return self._resolve_enum_case(node.name)
        if isinstance(node, PropertyAccess):
            obj, prop = node.obj, node.prop
            fn_key = f"{obj}.{prop}"
            fn_info = self._reg.functions.get(fn_key)
            if fn_info:
                return f"{fn_info.cpp_qualified}()"
            # Inference: pointer field → obj->GetProp()
            if obj in self._observed or obj in self._mutable:
                return f"{obj}->{self._infer_getter(prop)}()"
            return f"{obj}.{prop}"
        if isinstance(node, CallExpr):
            return self._gen_call(node)
        if isinstance(node, BinaryExpr):
            l = self.gen_expr(node.left)
            r = self.gen_expr(node.right)
            return f"{l} {node.op} {r}"
        if isinstance(node, CaptureExpr):
            # In an expression context, once/always are advisory; just emit the expr
            return self.gen_expr(node.expr)
        if isinstance(node, StringLit):
            # Plain string in expression context: build as TextContent or quoted string
            static_only = all(isinstance(s, StringSegment) for s in node.segments)
            if static_only:
                text = "".join(s.text for s in node.segments)
                return f'"{_escape(text)}"'
            return self._gen_text_content_inline(node)
        return str(node)

    def _gen_call(self, node: CallExpr) -> str:
        callee = node.callee
        args   = [self.gen_expr(a) for a in node.args]
        named  = [f"/* {k}= */{self.gen_expr(v)}" for k, v in node.named_args.items()]
        all_args = args + named

        # Resolve callee through registry
        if "." in callee:
            owner, method = callee.rsplit(".", 1)
            fn_info = self._reg.functions.get(callee)
            if fn_info:
                cpp_q = fn_info.cpp_qualified
                if fn_info.is_instance:
                    if all_args:
                        return f"{owner}.{method.split('::')[-1]}({', '.join(all_args)})"
                    return f"{cpp_q}()"
                else:
                    return f"{cpp_q}({', '.join(all_args)})"
            # Inference based on context:
            getter = self._infer_getter(method)
            if owner in self._observed or owner in self._mutable:
                # Known pointer field → obj->GetMethod(args)
                return f"{owner}->{getter}({', '.join(all_args)})"
            if owner and owner[0].isupper():
                # Class or namespace name (PascalCase) → Owner::GetMethod(args)
                return f"{owner}::{getter}({', '.join(all_args)})"
            # Local variable or parameter → plain obj.method(args)
            return f"{owner}.{method}({', '.join(all_args)})"

        return f"{callee}({', '.join(all_args)})"

    def _gen_text_content_inline(self, node: StringLit) -> str:
        """Inline TextContent(...) for expression context."""
        parts = []
        for seg in node.segments:
            if isinstance(seg, StringSegment) and seg.text:
                parts.append(f'"{_escape(seg.text)}"')
            elif isinstance(seg, InterpSegment):
                parts.append(self._gen_binding(seg.expr))
        return f"UI::TextContent({', '.join(parts)})"

    def _extract_frame_bounds(self, modifiers: list[Modifier]) -> str:
        for mod in modifiers:
            if mod.name == FRAME_MODIFIER:
                return self._gen_frame_bounds(mod)
        return ""

    def _gen_frame_bounds(self, mod: Modifier) -> str:
        width  = mod.named_args.get("width")
        height = mod.named_args.get("height")
        anchor = mod.named_args.get("anchor")
        w_str  = self._gen_pixel_value(width) if width else "UI::Value{0.0, UI::ValueType::PIXEL}"
        h_str  = self._gen_pixel_value(height) if height else "UI::Value{0.0, UI::ValueType::PIXEL}"
        a_str  = self.gen_expr(anchor) if anchor else "UI::Anchor::TOP_LEFT"
        return f"UI::Bounds({w_str}, {h_str}, {a_str})"

    def _gen_pixel_value(self, node) -> str:
        return f"UI::Value{{static_cast<double>({self.gen_expr(node)}), UI::ValueType::PIXEL}}"

    def _gen_mod_args(self, mod: Modifier) -> str:
        # Special modifiers have their own arg-wrapping rules
        special = self._gen_mod_args_special(mod)
        if special is not None:
            return special
        args = [self.gen_expr(a) for a in mod.args]
        # Add 'f' suffix for float args passed as integers
        if len(args) == 1:
            v = mod.args[0]
            if isinstance(v, NumberLit) and args[0].isdigit():
                args[0] = args[0] + ".0f"
        named = [f"/* {k}= */{self.gen_expr(v)}" for k, v in mod.named_args.items()]
        return ", ".join(args + named)

    # DSL modifiers that need special C++ call shapes
    _SPECIAL_MODIFIERS = {
        "throttle": ("SetThrottlePeriod", "std::chrono::duration<double>({})"),
    }

    def _resolve_modifier(self, component_name: str, mod: Modifier) -> str:
        comp = self._reg.components.get(component_name)
        if comp and mod.name in comp.modifiers:
            return comp.modifiers[mod.name].cpp_method
        if mod.name in self._SPECIAL_MODIFIERS:
            return self._SPECIAL_MODIFIERS[mod.name][0]
        # Common fallback: camelCase → Set + Title
        return "Set" + mod.name[0].upper() + mod.name[1:]

    def _gen_mod_args_special(self, mod: Modifier) -> Optional[str]:
        """Returns a fully-formed arg string for special modifiers, or None."""
        spec = self._SPECIAL_MODIFIERS.get(mod.name)
        if spec:
            inner = self.gen_expr(mod.args[0]) if mod.args else "0"
            return spec[1].format(inner)
        return None

    def _resolve_enum_case(self, case_name: str) -> str:
        for enum_info in self._reg.enums.values():
            for val in enum_info.values:
                if val.dsl_name == case_name:
                    return f"{enum_info.cpp_type}::{val.cpp_name}"
        # Fallback: try common UI enums
        KNOWN = {
            "topLeft":    "UI::Anchor::TOP_LEFT",
            "topCenter":  "UI::Anchor::TOP_CENTER",
            "topRight":   "UI::Anchor::TOP_RIGHT",
            "left":       "UI::HAlign::LEFT",
            "center":     "UI::HAlign::CENTER",
            "right":      "UI::HAlign::RIGHT",
            "scroll":     "UI::OverflowMode::SCROLL",
            "hidden":     "UI::OverflowMode::HIDDEN",
            "wrap":       "UI::OverflowMode::WRAP",
        }
        return KNOWN.get(case_name, case_name)


# ── Utilities ─────────────────────────────────────────────────────────────────

def _escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
