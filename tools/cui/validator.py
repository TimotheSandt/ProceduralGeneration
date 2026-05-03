"""
validator.py - Semantic validation of a parsed .cui AST against the registry.

Checks:
  - Duplicate field / let / func names within a view.
  - Each Element name must be a known component.
  - A component that does not accept children must have none.
  - Each Modifier on a known component must exist in its modifier table
    (or be a known special modifier).
  - A non-void modifier must receive at least one argument.
  - @State fields must carry an explicit type annotation.
  - @cui-cpp annotation is for the UI system — warn if used in .cui source.
"""

from __future__ import annotations
from ast_nodes import (
    File, ViewDecl, Element, Modifier,
    WrappedField, LetDecl, FuncDecl, CppBlock,
)
from scanner import Registry

_SPECIAL_MODIFIERS = {"throttle", "frame"}


class ValidationError:
    def __init__(self, view: str, message: str, is_warning: bool = False,
                 line: int = 0, col: int = 0):
        self.view       = view
        self.message    = message
        self.is_warning = is_warning
        self.line       = line
        self.col        = col

    def __str__(self) -> str:
        tag = "WARNING" if self.is_warning else "ERROR"
        loc = f":{self.line}:{self.col}" if self.line else ""
        return f"[{self.view}{loc}] {tag}: {self.message}"

    @property
    def fatal(self) -> bool:
        return not self.is_warning


def validate(ast: File, registry: Registry) -> list[ValidationError]:
    errors: list[ValidationError] = []
    for view in ast.views:
        _validate_view(view, registry, errors)
    return errors


def _validate_view(view: ViewDecl, registry: Registry, errors: list[ValidationError]):
    # ── Duplicate names ───────────────────────────────────────────────────────
    seen: dict[str, str] = {}
    for field in view.fields:
        name = None
        kind = ""
        if isinstance(field, WrappedField):
            name, kind = field.name, field.wrapper
        elif isinstance(field, LetDecl):
            name, kind = field.name, "let"
        elif isinstance(field, FuncDecl):
            name, kind = field.name, "func"
        if name:
            if name in seen:
                errors.append(ValidationError(
                    view.name,
                    f"Duplicate name '{name}' (already declared as {seen[name]}).",
                    line=getattr(field, "line", 0), col=getattr(field, "col", 0),
                ))
            else:
                seen[name] = kind

    # ── @State must have a type ───────────────────────────────────────────────
    for field in view.fields:
        if isinstance(field, WrappedField) and field.wrapper == "@State":
            if not field.type_:
                errors.append(ValidationError(
                    view.name,
                    f"@State field '{field.name}' must have an explicit type annotation "
                    f"(e.g. @State var {field.name}: int = 0).",
                    line=field.line, col=field.col,
                ))

    # ── @cui-cpp warning ──────────────────────────────────────────────────────
    for field in view.fields:
        if isinstance(field, CppBlock):
            errors.append(ValidationError(
                view.name,
                "@cui-cpp is an escape hatch for UI system internals. "
                "Prefer func declarations for view-local helpers.",
                is_warning=True,
                line=field.line, col=field.col,
            ))

    # ── Root element ──────────────────────────────────────────────────────────
    _validate_element(view.root, view.name, registry, errors)


def _validate_element(el: Element, view_name: str, registry: Registry,
                      errors: list[ValidationError]):
    comp = registry.components.get(el.name)

    if comp is None:
        known = ", ".join(sorted(registry.components))
        errors.append(ValidationError(
            view_name,
            f"Unknown component '{el.name}'. Known components: {known}.",
            line=el.line, col=el.col,
        ))
        # Still recurse so we catch more errors in one pass
    else:
        # Children on a non-container
        if el.children and not comp.accepts_children:
            errors.append(ValidationError(
                view_name,
                f"'{el.name}' does not accept children (accepts_children=false).",
                line=el.line, col=el.col,
            ))

        # Modifiers
        for mod in el.modifiers:
            _validate_modifier(mod, el.name, comp, view_name, errors)

    for child in el.children:
        _validate_element(child, view_name, registry, errors)


def _validate_modifier(mod: Modifier, comp_name: str, comp,
                       view_name: str, errors: list[ValidationError]):
    if mod.name in _SPECIAL_MODIFIERS:
        # throttle must have exactly one arg
        if mod.name == "throttle" and not mod.args:
            errors.append(ValidationError(
                view_name,
                f"Modifier '{comp_name}.throttle' requires a period argument (e.g. .throttle(0.25)).",
                line=mod.line, col=mod.col,
            ))
        return

    if mod.name not in comp.modifiers:
        known = ", ".join(sorted(comp.modifiers))
        errors.append(ValidationError(
            view_name,
            f"'{comp_name}' has no modifier '{mod.name}'. "
            f"Known modifiers: {known}.",
            line=mod.line, col=mod.col,
        ))
        return

    mod_info = comp.modifiers[mod.name]
    if mod_info.param_type != "void" and not mod.args and not mod.named_args:
        errors.append(ValidationError(
            view_name,
            f"Modifier '{comp_name}.{mod.name}' expects a {mod_info.param_type} argument "
            f"but none was provided.",
            line=mod.line, col=mod.col,
        ))
