"""
scanner.py - Scans C++ headers for @cui-* Doxygen annotations and
builds the component / function / enum registry.
"""

from __future__ import annotations
import re
import json
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional


# ── Registry data structures ──────────────────────────────────────────────────

@dataclass
class ModifierInfo:
    dsl_name:   str
    cpp_method: str     # e.g. "SetTextScale"
    param_type: str     # e.g. "float"
    param_name: str = "value"


@dataclass
class ComponentInfo:
    dsl_name:        str
    cpp_class:       str   # e.g. "UI::Text"
    factory:         str   # e.g. "UI::CreateText"
    accepts_children: bool
    content_model:   str   # "none" | "text_content"
    modifiers:       dict = field(default_factory=dict)  # dsl -> ModifierInfo
    include_path:    str = ""


@dataclass
class ParamInfo:
    name:  str
    type_: str


@dataclass
class FunctionInfo:
    dsl_name:     str
    cpp_qualified: str   # e.g. "Window::GetAverageFPS" or "Profiler::GetAverageTime"
    return_type:  str
    params:       list   # list of ParamInfo
    is_instance:  bool   # True = instance method on @Observed var
    is_getter:    bool   # True = 0-arg getter aliased as property
    is_volatile:  bool
    owner_class:  str    # e.g. "Window" (for instance methods)
    include_path: str = ""


@dataclass
class EnumValueInfo:
    dsl_name: str
    cpp_name: str


@dataclass
class EnumInfo:
    dsl_name:     str
    cpp_type:     str   # e.g. "UI::Anchor"
    values:       list  # list of EnumValueInfo
    include_path: str = ""


@dataclass
class Registry:
    components: dict = field(default_factory=dict)  # dsl_name -> ComponentInfo
    functions:  dict = field(default_factory=dict)  # dsl_qualified -> FunctionInfo
    enums:      dict = field(default_factory=dict)  # dsl_name -> EnumInfo


# ── Regex patterns ────────────────────────────────────────────────────────────

# A Doxygen block containing at least one @cui- tag (simplest approach)
CUI_BLOCK_RE = re.compile(r'/\*\*(.*?)\*/', re.DOTALL)

# Individual @cui-tag [argument] within a block.
# Lookahead skips optional leading ' * ' on next comment line.
CUI_TAG_RE = re.compile(
    r'@cui-([\w-]+)'
    r'(?:[ \t]+([^\n@*/]+?))?'
    r'(?=[ \t]*\n?[ \t]*\*?[ \t]*(?:@|\*/|\Z))',
    re.DOTALL
)

# Class / struct declaration
CLASS_DECL_RE = re.compile(
    r'(?:class|struct)\s+(\w+)\s*(?:final\s*)?'
    r'(?::\s*(?:public|protected|private)?\s*[\w:<>, \t]+)?'
    r'\s*[{;]'
)

# Method/function declaration — anchored to start of line for clarity.
# We look for: [specifiers] ReturnType MethodName(params) [const] [override] [{;]
# Strategy: match known method name prefixes (DoSet, Set, Get, Is, Has) to avoid
# ambiguity with the return type.
METHOD_DECL_RE = re.compile(
    r'([\w\s:<>*&,]+?)\s+'          # return type (non-greedy)
    r'((?:DoSet|Set|Get|Is|Has)\w+)'  # method name (known prefixes)
    r'\s*\(([^)]*)\)'                # params
    r'[^;{]*[;{]'                    # end
)

# enum class / enum struct
ENUM_DECL_RE = re.compile(r'enum\s+(?:class|struct)\s+(\w+)')

# namespace
NAMESPACE_RE = re.compile(r'^\s*namespace\s+(\w+)\s*\{', re.MULTILINE)

# class declaration (for context tracking)
CLASS_CONTEXT_RE = re.compile(r'^\s*class\s+(\w+)\b', re.MULTILINE)


# ── Scanner ───────────────────────────────────────────────────────────────────

class Scanner:
    def __init__(self):
        self._registry = Registry()
        self._pending_component: Optional[str] = None
        # Deferred modifiers: (ModifierInfo list, class_context)
        self._pending_modifiers: list = []

    def scan_directory(self, headers_dir: Path, pattern: str = "**/*.h") -> Registry:
        files = sorted(headers_dir.glob(pattern))
        # Pass 1: create component entries (so modifiers can attach to them in pass 2)
        for path in files:
            self._pending_component = None
            text = path.read_text(encoding="utf-8", errors="replace")
            rel  = path.relative_to(headers_dir).as_posix()
            self._scan_text(text, rel, components_only=True)
        # Pass 2: attach modifiers and expose functions
        for path in files:
            self._pending_component = None   # reset per file — prevents cross-file leakage
            text = path.read_text(encoding="utf-8", errors="replace")
            rel  = path.relative_to(headers_dir).as_posix()
            self._scan_text(text, rel, components_only=False)
        # Pass 3: resolve deferred modifier propagation (@cui-extends)
        self._resolve_pending_modifiers()
        return self._registry

    def scan_file(self, path: Path) -> Registry:
        """Single-file scan (for testing)."""
        text = path.read_text(encoding="utf-8", errors="replace")
        rel  = path.name
        self._pending_component = None
        self._scan_text(text, rel, components_only=True)
        self._pending_component = None
        self._scan_text(text, rel, components_only=False)
        self._resolve_pending_modifiers()
        return self._registry

    def _resolve_pending_modifiers(self):
        """Propagate modifiers from base components to derived ones via @cui-extends."""
        for entry in self._pending_modifiers:
            base_name, derived_name = entry
            base = self._registry.components.get(base_name)
            derived = self._registry.components.get(derived_name)
            if base and derived:
                for mod_name, mod_info in base.modifiers.items():
                    if mod_name not in derived.modifiers:
                        derived.modifiers[mod_name] = mod_info

    def _scan_text(self, text: str, include_path: str, components_only: bool = False):
        # Build position → name maps for namespace and class context
        ns_map  = {}  # start_pos -> namespace_name
        cls_map = {}  # start_pos -> class_name
        for m in NAMESPACE_RE.finditer(text):
            ns_map[m.start()] = m.group(1)
        for m in CLASS_CONTEXT_RE.finditer(text):
            cls_map[m.start()] = m.group(1)

        # Pre-scan (pass 2 only): find which component is defined in this file.
        # Modifiers on base-class methods attach to this component when _pending_component
        # hasn't been set yet (i.e. the @cui-component annotation comes later in the file).
        file_component_dsl: Optional[str] = None
        if not components_only:
            file_comps: list[str] = []
            for bm in CUI_BLOCK_RE.finditer(text):
                bc = bm.group(1)
                if "@cui-" not in bc:
                    continue
                pre_tags: dict = {}
                for tm in CUI_TAG_RE.finditer(bc):
                    k = tm.group(1).strip()
                    v = (tm.group(2) or "").strip()
                    pre_tags[k] = v
                if "component" in pre_tags or "generate-component" in pre_tags:
                    rest = text[bm.end():bm.end() + 200]
                    m = CLASS_DECL_RE.search(rest)
                    if m:
                        file_comps.append(pre_tags.get("dsl-name", m.group(1)))
            if len(file_comps) == 1:
                file_component_dsl = file_comps[0]

        # Main scan: walk all comment blocks
        for block_match in CUI_BLOCK_RE.finditer(text):
            block_content = block_match.group(1)

            if "@cui-" not in block_content:
                continue

            block_end = block_match.end()

            # Extract tags
            tags: dict = {}
            for tag_m in CUI_TAG_RE.finditer(block_content):
                key   = tag_m.group(1).strip()
                value = (tag_m.group(2) or "").strip()
                if key in tags:
                    if not isinstance(tags[key], list):
                        tags[key] = [tags[key]]
                    tags[key].append(value)
                else:
                    tags[key] = value

            if not tags:
                continue

            # Determine namespace context
            namespace = ""
            for pos, ns in sorted(ns_map.items()):
                if pos < block_match.start():
                    namespace = ns

            # Determine class context
            class_ctx = ""
            for pos, cls in sorted(cls_map.items()):
                if pos < block_match.start():
                    class_ctx = cls

            owner = class_ctx or namespace
            rest  = text[block_end:block_end + 500]
            self._process_block(tags, rest, owner, namespace, include_path,
                                components_only=components_only,
                                file_component_dsl=file_component_dsl)

    def _process_block(self, tags: dict, decl_snippet: str,
                        owner: str, namespace: str, include_path: str,
                        components_only: bool = False,
                        file_component_dsl: Optional[str] = None):
        """Process a single @cui-* comment block."""
        ns_prefix = (namespace + "::") if namespace else ""

        # Pass 1 (components_only): only handle structural declarations
        if components_only and not any(k in tags for k in ("component", "generate-component", "alias")):
            return

        # ── @cui-component ────────────────────────────────────────────────────
        if "component" in tags:
            m = CLASS_DECL_RE.search(decl_snippet)
            if not m:
                return
            class_name = m.group(1)
            dsl_name   = tags.get("dsl-name", class_name)
            if components_only:
                # Pass 1: create the entry
                info = ComponentInfo(
                    dsl_name=dsl_name,
                    cpp_class=ns_prefix + class_name,
                    factory=tags.get("factory", ns_prefix + "Create" + class_name),
                    accepts_children=tags.get("accepts-children", "false").lower() not in ("false", ""),
                    content_model=tags.get("content-model", "none"),
                    include_path=include_path,
                )
                self._registry.components[dsl_name] = info
                # @cui-extends: schedule modifier propagation from base component in pass 3
                if "extends" in tags:
                    self._pending_modifiers.append((tags["extends"].strip(), dsl_name))
            self._pending_component = dsl_name

        # ── @cui-generate-component ────────────────────────────────────────
        elif "generate-component" in tags:
            m = CLASS_DECL_RE.search(decl_snippet)
            if not m:
                return
            class_name = m.group(1)
            dsl_name   = class_name.removesuffix("Base")
            if components_only:
                info = ComponentInfo(
                    dsl_name=dsl_name,
                    cpp_class=ns_prefix + dsl_name,
                    factory=ns_prefix + "Create" + dsl_name,
                    accepts_children=tags.get("accepts-children", "false").lower() not in ("false", ""),
                    content_model=tags.get("content-model", "none"),
                    include_path=include_path,
                )
                self._registry.components[dsl_name] = info
            self._pending_component = dsl_name

        # ── @cui-alias ────────────────────────────────────────────────────────
        elif "alias" in tags:
            class_name = None
            m = CLASS_DECL_RE.search(decl_snippet)
            if m:
                class_name = m.group(1)
            else:
                mu = re.search(r'using\s+(\w+)\s*=', decl_snippet)
                if mu:
                    class_name = mu.group(1)
            if not class_name:
                return
            if components_only:
                alias_of    = tags["alias"]
                factory_val = tags.get("factory", ns_prefix + "Create" + class_name)
                if alias_of in self._registry.components:
                    base = self._registry.components[alias_of]
                    info = ComponentInfo(
                        dsl_name=class_name,
                        cpp_class=ns_prefix + class_name,
                        factory=factory_val,
                        accepts_children=base.accepts_children,
                        content_model=base.content_model,
                        include_path=include_path,
                    )
                else:
                    info = ComponentInfo(
                        dsl_name=class_name,
                        cpp_class=ns_prefix + class_name,
                        factory=factory_val,
                        accepts_children=False,
                        content_model="none",
                        include_path=include_path,
                    )
                self._registry.components[class_name] = info
                # Schedule modifier propagation from the aliased component in pass 3
                self._pending_modifiers.append((alias_of, class_name))
            self._pending_component = class_name

        # ── @cui-modifier ─────────────────────────────────────────────────────
        if "modifier" in tags:
            m = METHOD_DECL_RE.search(decl_snippet)
            if not m:
                return
            method_name = m.group(2)
            params_raw  = m.group(3).strip()

            explicit_dsl = tags["modifier"].strip()
            dsl_names = [n.strip() for n in explicit_dsl.split(",")] if explicit_dsl else \
                        [_method_to_dsl_name(method_name)]

            cpp_setter = ("Set" + method_name[5:]) if method_name.startswith("DoSet") else method_name
            param_type = _parse_first_param_type(params_raw)

            if self._pending_component and self._pending_component in self._registry.components:
                # Modifier follows @cui-component in the same file → attach to it
                comp = self._registry.components[self._pending_component]
                for name in dsl_names:
                    comp.modifiers[name] = ModifierInfo(name, cpp_setter, param_type)
            elif file_component_dsl and file_component_dsl in self._registry.components:
                # Modifier on a base class, but this file defines exactly one component
                # (e.g. TextWidgetBase::DoSetTextScale in Text.h → attach to Text)
                comp = self._registry.components[file_component_dsl]
                for name in dsl_names:
                    if name not in comp.modifiers:
                        comp.modifiers[name] = ModifierInfo(name, cpp_setter, param_type)
            else:
                # Truly shared base class — apply to all existing components
                for comp in self._registry.components.values():
                    for name in dsl_names:
                        if name not in comp.modifiers:
                            comp.modifiers[name] = ModifierInfo(name, cpp_setter, param_type)

        # ── @cui-expose ───────────────────────────────────────────────────────
        if "expose" in tags:
            m = METHOD_DECL_RE.search(decl_snippet)
            if not m:
                return
            ret_type    = m.group(1).strip()
            method_name = m.group(2)
            params_raw  = m.group(3).strip()
            is_volatile = "volatile" in tags

            owner_class   = owner  # e.g. "Window", "Profiler"
            cpp_qualified = (owner_class + "::" if owner_class else "") + method_name
            params        = _parse_params(params_raw)
            is_getter     = (method_name.startswith("Get") and len(params) == 0)

            explicit_dsl = tags.get("dsl-name", "").strip()
            dsl_name     = explicit_dsl if explicit_dsl else _method_to_dsl_name(method_name)

            # Determine if instance method: window is @Observed, so Window methods are instance
            # Profiler static methods and free functions are NOT instance
            # Heuristic: check if the declaration snippet contains 'static'
            is_static  = bool(re.search(r'\bstatic\b', decl_snippet[:100]))
            is_instance = bool(owner_class) and not is_static

            qualified_key = (owner_class + "." + dsl_name) if owner_class else dsl_name

            self._registry.functions[qualified_key] = FunctionInfo(
                dsl_name=dsl_name,
                cpp_qualified=cpp_qualified,
                return_type=ret_type,
                params=params,
                is_instance=is_instance,
                is_getter=is_getter,
                is_volatile=is_volatile,
                owner_class=owner_class,
                include_path=include_path,
            )

        # ── @cui-enum ─────────────────────────────────────────────────────────
        if "enum" in tags:
            m = ENUM_DECL_RE.search(decl_snippet)
            if not m:
                return
            enum_name = m.group(1)
            cpp_type  = (ns_prefix or "UI::") + enum_name

            values = []
            brace_start = decl_snippet.find("{")
            brace_end   = decl_snippet.find("}")
            if brace_start != -1 and brace_end != -1:
                body = decl_snippet[brace_start+1:brace_end]
                for val in re.split(r"[,\n]", body):
                    val = val.strip().split("=")[0].strip().split("//")[0].strip()
                    if val and re.match(r"^\w+$", val):
                        values.append(EnumValueInfo(_to_lower_camel(val), val))

            self._registry.enums[enum_name] = EnumInfo(
                dsl_name=enum_name,
                cpp_type=cpp_type,
                values=values,
                include_path=include_path,
            )


# ── Helpers ───────────────────────────────────────────────────────────────────

def _method_to_dsl_name(method_name: str) -> str:
    """DoSetTextScale -> textScale, GetAverageFPS -> averageFPS, SetPadding -> padding"""
    if method_name.startswith("DoSet"):
        name = method_name[5:]
    elif method_name.startswith("Get"):
        name = method_name[3:]
    elif method_name.startswith("Set"):
        name = method_name[3:]
    else:
        name = method_name
    if name:
        return name[0].lower() + name[1:]
    return name


def _to_lower_camel(s: str) -> str:
    """TOP_LEFT -> topLeft, WRAP -> wrap"""
    parts = s.split("_")
    return parts[0].lower() + "".join(p.capitalize() for p in parts[1:])


def _parse_first_param_type(params_raw: str) -> str:
    if not params_raw.strip():
        return "void"
    first = params_raw.split(",")[0].strip()
    # Remove parameter name (last word) to get type
    parts = first.split()
    if len(parts) > 1:
        return " ".join(parts[:-1])
    return first


def _parse_params(params_raw: str) -> list[ParamInfo]:
    if not params_raw.strip():
        return []
    result = []
    for p in params_raw.split(","):
        p = p.strip()
        if not p:
            continue
        parts = p.split()
        if len(parts) >= 2:
            name = parts[-1].lstrip("*&")
            type_ = " ".join(parts[:-1])
        else:
            name = p
            type_ = p
        result.append(ParamInfo(name, type_))
    return result


# ── Serialization ─────────────────────────────────────────────────────────────

def registry_to_json(registry: Registry) -> str:
    def default(o):
        if hasattr(o, "__dataclass_fields__"):
            return asdict(o)
        return str(o)
    return json.dumps(asdict(registry), indent=2, default=default)


def registry_from_json(text: str) -> Registry:
    data = json.loads(text)
    reg = Registry()
    for dsl_name, comp in data.get("components", {}).items():
        mods = {k: ModifierInfo(**v) for k, v in comp.get("modifiers", {}).items()}
        reg.components[dsl_name] = ComponentInfo(
            dsl_name=comp["dsl_name"],
            cpp_class=comp["cpp_class"],
            factory=comp["factory"],
            accepts_children=comp["accepts_children"],
            content_model=comp["content_model"],
            modifiers=mods,
            include_path=comp.get("include_path", ""),
        )
    for key, fn in data.get("functions", {}).items():
        params = [ParamInfo(**p) for p in fn.get("params", [])]
        reg.functions[key] = FunctionInfo(
            dsl_name=fn["dsl_name"],
            cpp_qualified=fn["cpp_qualified"],
            return_type=fn["return_type"],
            params=params,
            is_instance=fn["is_instance"],
            is_getter=fn["is_getter"],
            is_volatile=fn["is_volatile"],
            owner_class=fn["owner_class"],
            include_path=fn.get("include_path", ""),
        )
    for name, en in data.get("enums", {}).items():
        vals = [EnumValueInfo(**v) for v in en.get("values", [])]
        reg.enums[name] = EnumInfo(
            dsl_name=en["dsl_name"],
            cpp_type=en["cpp_type"],
            values=vals,
            include_path=en.get("include_path", ""),
        )
    return reg
