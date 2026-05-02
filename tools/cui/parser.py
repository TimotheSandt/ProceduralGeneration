"""
parser.py - Recursive-descent parser for the .cui DSL.
"""

from __future__ import annotations
from typing import Optional
from lexer import Token, TT
from ast_nodes import (
    File, ImportDecl, ViewDecl, WrappedField, LetDecl, FuncDecl, Param,
    CppBlock, Element, Modifier,
    StringLit, StringSegment, InterpSegment,
    NumberLit, BoolLit, Identifier, EnumCase, PropertyAccess,
    CallExpr, BinaryExpr, CaptureExpr,
)


class ParseError(Exception):
    def __init__(self, msg, token: Token):
        loc = f"{token.line}:{token.col}"
        super().__init__(f"Parse error at {loc}: {msg} (got {token.type.name} {token.value!r})")
        self.token = token


class Parser:
    def __init__(self, tokens: list[Token]):
        self._tokens = [t for t in tokens if t.type is not TT.EOF] + [Token(TT.EOF, None)]
        self._pos = 0

    # ── primitives ────────────────────────────────────────────────────────────

    def _peek(self, offset: int = 0) -> Token:
        idx = self._pos + offset
        return self._tokens[idx] if idx < len(self._tokens) else self._tokens[-1]

    def _advance(self) -> Token:
        tok = self._tokens[self._pos]
        if self._pos < len(self._tokens) - 1:
            self._pos += 1
        return tok

    def _expect(self, tt: TT) -> Token:
        tok = self._peek()
        if tok.type is not tt:
            raise ParseError(f"expected {tt.name}", tok)
        return self._advance()

    def _match(self, *types: TT) -> Optional[Token]:
        if self._peek().type in types:
            return self._advance()
        return None

    def _at(self, *types: TT) -> bool:
        return self._peek().type in types

    # ── top level ─────────────────────────────────────────────────────────────

    def parse_file(self) -> File:
        imports = []
        while self._at(TT.KW_IMPORT):
            imports.append(self._parse_import())
        views = []
        while not self._at(TT.EOF):
            views.append(self._parse_view())
        return File(imports, views)

    def _parse_import(self) -> ImportDecl:
        self._expect(TT.KW_IMPORT)
        path_tok = self._expect(TT.STRING)
        # The STRING token carries a StringLit; flatten it for import paths
        lit: StringLit = path_tok.value
        path = "".join(s.text for s in lit.segments if isinstance(s, StringSegment))
        return ImportDecl(path)

    # ── view ──────────────────────────────────────────────────────────────────

    def _parse_view(self) -> ViewDecl:
        self._expect(TT.KW_VIEW)
        name_tok = self._expect(TT.IDENT)
        name = name_tok.value
        self._expect(TT.LBRACE)
        fields = []
        root = None
        while not self._at(TT.RBRACE) and not self._at(TT.EOF):
            stmt = self._parse_view_stmt()
            if isinstance(stmt, Element):
                if root is not None:
                    raise ParseError("A view may only have one root element", self._peek())
                root = stmt
            else:
                fields.append(stmt)
        self._expect(TT.RBRACE)
        if root is None:
            raise ParseError("View has no root element", self._peek())
        return ViewDecl(name, fields, root, line=name_tok.line, col=name_tok.col)

    def _parse_view_stmt(self):
        tok = self._peek()
        if tok.type in (TT.AT_OBSERVED, TT.AT_MUTABLE, TT.AT_SNAPSHOT, TT.AT_STATE, TT.AT_BINDING):
            return self._parse_wrapped_field()
        if tok.type is TT.KW_LET:
            return self._parse_let()
        if tok.type is TT.KW_FUNC:
            return self._parse_func()
        if tok.type is TT.AT_CUI_CPP:
            return self._parse_cpp_block()
        # Otherwise it must be an element (root component or nested)
        return self._parse_element()

    # ── wrapped field ─────────────────────────────────────────────────────────

    def _parse_wrapped_field(self) -> WrappedField:
        wrapper_tok = self._advance()
        wrapper = wrapper_tok.value
        self._expect(TT.KW_VAR)
        name = self._expect(TT.IDENT).value
        self._expect(TT.COLON)
        type_, optional = self._parse_type()
        default = None
        if self._match(TT.EQ):
            default = self._parse_expr()
        return WrappedField(wrapper, name, type_, optional, default,
                            line=wrapper_tok.line, col=wrapper_tok.col)

    def _parse_type(self) -> tuple[str, bool]:
        """Returns (type_name, is_optional). Handles 'Foo?', 'Foo<Bar>', etc."""
        parts = [self._expect(TT.IDENT).value]
        while self._at(TT.DCOLON):
            self._advance()
            parts.append(self._expect(TT.IDENT).value)
        type_name = "::".join(parts)
        optional = bool(self._match(TT.QUESTION))
        return type_name, optional

    # ── let ───────────────────────────────────────────────────────────────────

    def _parse_let(self) -> LetDecl:
        let_tok = self._expect(TT.KW_LET)
        name = self._expect(TT.IDENT).value
        type_ = None
        if self._match(TT.COLON):
            type_, _ = self._parse_type()
        self._expect(TT.EQ)
        value = self._parse_expr()
        return LetDecl(name, type_, value, line=let_tok.line, col=let_tok.col)

    # ── func ──────────────────────────────────────────────────────────────────

    def _parse_func(self) -> FuncDecl:
        func_tok = self._expect(TT.KW_FUNC)
        name = self._expect(TT.IDENT).value
        self._expect(TT.LPAREN)
        params = self._parse_param_list()
        self._expect(TT.RPAREN)
        self._expect(TT.ARROW)
        ret_type, _ = self._parse_type()

        if self._match(TT.EQ):
            # Single-expression form: func f(...) -> T = expr
            body = self._parse_expr()
        else:
            # Block form: func f(...) -> T { return expr; }
            self._expect(TT.LBRACE)
            self._expect(TT.KW_RETURN)
            body = self._parse_expr()
            self._match(TT.SEMICOLON)   # trailing semicolon is optional in DSL
            self._expect(TT.RBRACE)

        return FuncDecl(name, params, ret_type, body, line=func_tok.line, col=func_tok.col)

    def _parse_param_list(self) -> list[Param]:
        params = []
        if self._at(TT.RPAREN):
            return params
        params.append(self._parse_param())
        while self._match(TT.COMMA):
            params.append(self._parse_param())
        return params

    def _parse_param(self) -> Param:
        name = self._expect(TT.IDENT).value
        self._expect(TT.COLON)
        type_, _ = self._parse_type()
        default = None
        if self._match(TT.EQ):
            default = self._parse_expr()
        return Param(name, type_, default)

    # ── @cui-cpp block ────────────────────────────────────────────────────────

    def _parse_cpp_block(self) -> CppBlock:
        cpp_tok = self._expect(TT.AT_CUI_CPP)
        self._expect(TT.LBRACE)
        # Everything until the matching } is verbatim C++
        # The lexer has already tokenized it, but we need the raw text.
        # Collect tokens (they're already lexed) until the matching RBRACE.
        # For now, we rebuild a crude representation from the token values.
        depth = 1
        parts = []
        while not self._at(TT.EOF):
            tok = self._advance()
            if tok.type is TT.LBRACE:
                depth += 1
                parts.append("{")
            elif tok.type is TT.RBRACE:
                depth -= 1
                if depth == 0:
                    break
                parts.append("}")
            else:
                parts.append(str(tok.value))
        return CppBlock(" ".join(parts), line=cpp_tok.line, col=cpp_tok.col)

    # ── element ───────────────────────────────────────────────────────────────

    def _parse_element(self) -> Element:
        name_tok = self._expect(TT.IDENT)
        name = name_tok.value

        args, named_args = [], {}
        if self._at(TT.LPAREN):
            args, named_args = self._parse_call_args()

        children = []
        if self._at(TT.LBRACE):
            self._advance()
            while not self._at(TT.RBRACE) and not self._at(TT.EOF):
                children.append(self._parse_element())
            self._expect(TT.RBRACE)

        modifiers = []
        while self._at(TT.DOT):
            modifiers.append(self._parse_modifier())

        return Element(name, args, named_args, children, modifiers,
                       line=name_tok.line, col=name_tok.col)

    def _parse_modifier(self) -> Modifier:
        dot_tok = self._expect(TT.DOT)
        name = self._expect(TT.IDENT).value
        args, named_args = [], {}
        if self._at(TT.LPAREN):
            args, named_args = self._parse_call_args()
        return Modifier(name, args, named_args, line=dot_tok.line, col=dot_tok.col)

    # ── call arguments ────────────────────────────────────────────────────────

    def _parse_call_args(self) -> tuple[list, dict]:
        self._expect(TT.LPAREN)
        args, named_args = [], {}
        if not self._at(TT.RPAREN):
            self._parse_arg_list(args, named_args)
        self._expect(TT.RPAREN)
        return args, named_args

    def _parse_arg_list(self, args: list, named_args: dict):
        # First arg
        self._parse_one_arg(args, named_args)
        while self._match(TT.COMMA):
            self._parse_one_arg(args, named_args)

    def _parse_one_arg(self, args: list, named_args: dict):
        # Named arg: label: expr
        if self._at(TT.IDENT) and self._peek(1).type is TT.COLON:
            label = self._advance().value
            self._advance()  # ':'
            named_args[label] = self._parse_expr()
        else:
            args.append(self._parse_expr())

    # ── expressions ───────────────────────────────────────────────────────────

    def _parse_expr(self) -> object:
        return self._parse_binary()

    def _parse_binary(self) -> object:
        left = self._parse_unary()
        while self._at(TT.PLUS, TT.MINUS, TT.STAR, TT.SLASH):
            op = self._advance().value
            right = self._parse_unary()
            left = BinaryExpr(op, left, right)
        return left

    def _parse_unary(self) -> object:
        # once / always prefix
        if self._at(TT.KW_ONCE, TT.KW_ALWAYS):
            mode_tok = self._advance()
            mode = "once" if mode_tok.type is TT.KW_ONCE else "always"
            expr = self._parse_primary()
            return CaptureExpr(mode, expr)
        return self._parse_postfix()

    def _parse_postfix(self) -> object:
        node = self._parse_primary()
        # Property accesses and calls chained with '.'
        while self._at(TT.DOT):
            # Peek ahead: DOT IDENT [ LPAREN ]
            self._advance()  # consume '.'
            prop_name = self._expect(TT.IDENT).value

            if self._at(TT.LPAREN):
                # method call on the base
                call_args, call_named = self._parse_call_args()
                callee = _expr_to_str(node) + "." + prop_name
                node = CallExpr(callee, call_args, call_named)
            else:
                # property access
                node = PropertyAccess(_expr_to_str(node), prop_name)
        return node

    def _parse_primary(self) -> object:
        tok = self._peek()

        # Literals
        if tok.type is TT.INT_LIT:
            self._advance()
            return NumberLit(float(tok.value))
        if tok.type is TT.FLOAT_LIT:
            self._advance()
            return NumberLit(tok.value)
        if tok.type in (TT.KW_TRUE, TT.KW_FALSE):
            self._advance()
            return BoolLit(tok.value)
        if tok.type is TT.STRING:
            self._advance()
            return self._process_string(tok.value)

        # Enum case: .topLeft
        if tok.type is TT.DOT:
            self._advance()
            name = self._expect(TT.IDENT).value
            return EnumCase(name)

        # Identifier, qualified names, calls
        if tok.type is TT.IDENT:
            return self._parse_ident_or_call()

        # Parenthesized
        if tok.type is TT.LPAREN:
            self._advance()
            expr = self._parse_expr()
            self._expect(TT.RPAREN)
            return expr

        raise ParseError("Expected expression", tok)

    def _parse_ident_or_call(self) -> object:
        """Parse identifier, qualified name (::), or function call."""
        parts = [self._expect(TT.IDENT).value]
        # Handle :: qualified names (e.g. std::chrono)
        while self._at(TT.DCOLON):
            self._advance()
            parts.append(self._expect(TT.IDENT).value)
        name = "::".join(parts)

        if self._at(TT.LPAREN):
            args, named = self._parse_call_args()
            return CallExpr(name, args, named)

        if len(parts) == 1:
            return Identifier(name)
        # Qualified without call — treat as Identifier
        return Identifier(name)

    # ── string processing ─────────────────────────────────────────────────────

    def _process_string(self, lit: StringLit) -> StringLit:
        """Parse expressions inside InterpSegment token lists."""
        processed = []
        for seg in lit.segments:
            if isinstance(seg, StringSegment):
                processed.append(seg)
            elif isinstance(seg, InterpSegment):
                sub_parser = Parser(seg.expr + [Token(TT.EOF, None)])
                expr = sub_parser._parse_expr()
                processed.append(InterpSegment(seg.capture, expr))
        return StringLit(processed)


# ── Helper ────────────────────────────────────────────────────────────────────

def _expr_to_str(node) -> str:
    """Convert a simple expression node to a dotted string for building callee names."""
    if isinstance(node, Identifier):
        return node.name
    if isinstance(node, PropertyAccess):
        return node.obj + "." + node.prop
    return "?"


def parse(tokens: list[Token]) -> File:
    return Parser(tokens).parse_file()
