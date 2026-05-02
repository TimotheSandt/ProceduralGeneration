"""
ast_nodes.py - AST node definitions for the .cui DSL.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Optional


# ── Expressions ──────────────────────────────────────────────────────────────

@dataclass
class NumberLit:
    """A numeric literal. `value` is always a float."""
    value: float


@dataclass
class BoolLit:
    value: bool


@dataclass
class Identifier:
    name: str


@dataclass
class EnumCase:
    """Leading-dot enum: `.topLeft`"""
    name: str


@dataclass
class PropertyAccess:
    """obj.prop or Ns.func"""
    obj: str
    prop: str


@dataclass
class CallExpr:
    """func(args) or Ns.method(args)"""
    # callee is a string: plain "toMs", qualified "Profiler.averageTime", or "window.someMethod"
    callee: str
    args: list              # positional Expr list
    named_args: dict        # label -> Expr

    def __post_init__(self):
        if self.named_args is None:
            self.named_args = {}


@dataclass
class BinaryExpr:
    op: str
    left: object
    right: object


@dataclass
class CaptureExpr:
    """once expr / always expr"""
    mode: str           # "once" | "always"
    expr: object        # any Expr


@dataclass
class StringSegment:
    text: str


@dataclass
class InterpSegment:
    capture: Optional[str]  # "once" | "always" | None
    expr: object            # any Expr


@dataclass
class StringLit:
    """String literal with optional interpolation segments."""
    segments: list  # list of StringSegment | InterpSegment


# ── Declarations ─────────────────────────────────────────────────────────────

@dataclass
class LetDecl:
    name: str
    type_: Optional[str]
    value: object           # Expr


@dataclass
class Param:
    name: str
    type_: str
    default: Optional[object]  # Expr | None


@dataclass
class FuncDecl:
    name: str
    params: list            # list of Param
    return_type: str
    body: object            # Expr


@dataclass
class WrappedField:
    wrapper: str            # "@Observed" | "@Snapshot" | "@State" | "@Binding"
    name: str
    type_: str
    optional: bool          # True if type ends with "?"
    default: Optional[object]  # Expr | None


@dataclass
class CppBlock:
    content: str            # verbatim C++ code


# ── Modifier ─────────────────────────────────────────────────────────────────

@dataclass
class Modifier:
    name: str
    args: list              # positional Expr list
    named_args: dict        # label -> Expr

    def __post_init__(self):
        if self.named_args is None:
            self.named_args = {}


# ── Elements ─────────────────────────────────────────────────────────────────

@dataclass
class Element:
    name: str
    args: list              # positional Expr list
    named_args: dict        # label -> Expr
    children: list          # list of Element
    modifiers: list         # list of Modifier

    def __post_init__(self):
        if self.named_args is None:
            self.named_args = {}


# ── Top-level ─────────────────────────────────────────────────────────────────

@dataclass
class ViewDecl:
    name: str
    fields: list            # list of WrappedField | LetDecl | FuncDecl | CppBlock
    root: Element           # single root element


@dataclass
class ImportDecl:
    path: str


@dataclass
class File:
    imports: list           # list of ImportDecl
    views: list             # list of ViewDecl
