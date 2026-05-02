"""
validator.py - Semantic validation of a parsed .cui AST against the registry.

Checks performed:
  - Each Element name must be a known component in the registry.
  - Each Modifier on a known component must be a known modifier for that component
    (or a known special modifier).
  - A Modifier must have at least one argument if the component's modifier expects one.
"""

from __future__ import annotations
from ast_nodes import (
    File, ViewDecl, Element, Modifier, WrappedField, LetDecl, FuncDecl,
)
from scanner import Registry

# Special modifiers handled by the generator regardless of registry
_SPECIAL_MODIFIERS = {"throttle", "frame"}

# Wrappers whose fields become constructor params (not state)
_PARAM_WRAPPERS = {"@Observed", "@Mutable", "@Snapshot"}


class ValidationError:
    def __init__(self, view: str, message: str):
        self.view    = view
        self.message = message

    def __str__(self) -> str:
        return f"[{self.view}] {self.message}"


def validate(ast: File, registry: Registry) -> list[ValidationError]:
    errors: list[ValidationError] = []
    for view in ast.views:
        _validate_view(view, registry, errors)
    return errors


def _validate_view(view: ViewDecl, registry: Registry, errors: list[ValidationError]):
    _validate_element(view.root, view.name, registry, errors)


def _validate_element(el: Element, view_name: str, registry: Registry,
                      errors: list[ValidationError]):
    comp = registry.components.get(el.name)
    if comp is None:
        errors.append(ValidationError(
            view_name,
            f"Unknown component '{el.name}'. "
            f"Known: {', '.join(sorted(registry.components))}."
        ))
    else:
        for mod in el.modifiers:
            if mod.name in _SPECIAL_MODIFIERS:
                continue
            if mod.name not in comp.modifiers:
                errors.append(ValidationError(
                    view_name,
                    f"Component '{el.name}' has no modifier '{mod.name}'. "
                    f"Known modifiers: {', '.join(sorted(comp.modifiers))}."
                ))
            else:
                mod_info = comp.modifiers[mod.name]
                if mod_info.param_type != "void" and not mod.args and not mod.named_args:
                    errors.append(ValidationError(
                        view_name,
                        f"Modifier '{el.name}.{mod.name}' expects an argument "
                        f"({mod_info.param_type}) but got none."
                    ))

    for child in el.children:
        _validate_element(child, view_name, registry, errors)
