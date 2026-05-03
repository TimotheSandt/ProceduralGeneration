"""
lexer.py - Tokenizer for .cui DSL files.

String interpolation is handled by the lexer: a STRING token carries a
pre-parsed list of segments (StringSegment / InterpSegment).
"""

from __future__ import annotations
from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional
from ast_nodes import StringLit, StringSegment, InterpSegment


class TT(Enum):
    # Keywords
    KW_VIEW    = auto()
    KW_LET     = auto()
    KW_FUNC    = auto()
    KW_ONCE    = auto()
    KW_ALWAYS  = auto()
    KW_VAR     = auto()
    KW_IMPORT  = auto()
    KW_TRUE    = auto()
    KW_FALSE   = auto()
    KW_RETURN  = auto()
    # Property wrappers
    AT_OBSERVED = auto()
    AT_MUTABLE  = auto()   # non-const writable reference
    AT_SNAPSHOT = auto()
    AT_STATE    = auto()
    AT_BINDING  = auto()
    AT_CUI_CPP  = auto()
    # Literals
    IDENT      = auto()
    INT_LIT    = auto()
    FLOAT_LIT  = auto()
    STRING     = auto()  # value is a StringLit
    # Operators / punctuation
    ARROW      = auto()  # ->
    EQ         = auto()  # =
    COLON      = auto()  # :
    COMMA      = auto()  # ,
    DOT        = auto()  # .
    DCOLON     = auto()  # ::
    LPAREN     = auto()  # (
    RPAREN     = auto()  # )
    LBRACE     = auto()  # {
    RBRACE     = auto()  # }
    QUESTION   = auto()  # ?
    PLUS       = auto()
    MINUS      = auto()
    STAR       = auto()
    SLASH      = auto()
    SEMICOLON  = auto()  # ; (block-form func)
    EOF        = auto()


KEYWORDS = {
    "view":    TT.KW_VIEW,
    "let":     TT.KW_LET,
    "func":    TT.KW_FUNC,
    "once":    TT.KW_ONCE,
    "always":  TT.KW_ALWAYS,
    "var":     TT.KW_VAR,
    "import":  TT.KW_IMPORT,
    "true":    TT.KW_TRUE,
    "false":   TT.KW_FALSE,
    "return":  TT.KW_RETURN,
}

WRAPPERS = {
    "@Observed": TT.AT_OBSERVED,
    "@Mutable":  TT.AT_MUTABLE,
    "@Snapshot": TT.AT_SNAPSHOT,
    "@State":    TT.AT_STATE,
    "@Binding":  TT.AT_BINDING,
    "@cui-cpp":  TT.AT_CUI_CPP,
}


@dataclass
class Token:
    type:  TT
    value: object   # str | float | int | StringLit | None
    line:  int = 0
    col:   int = 0

    def __repr__(self):
        return f"Token({self.type.name}, {self.value!r}, {self.line}:{self.col})"


class LexError(Exception):
    def __init__(self, msg, line, col):
        super().__init__(f"Lex error at {line}:{col}: {msg}")
        self.line = line
        self.col  = col


class Lexer:
    def __init__(self, source: str, filename: str = "<cui>"):
        self._src      = source
        self._pos      = 0
        self._line     = 1
        self._col      = 1
        self._filename = filename

    # ── public API ────────────────────────────────────────────────────────────

    def tokenize(self) -> list[Token]:
        tokens = []
        while True:
            tok = self._next_token()
            tokens.append(tok)
            if tok.type is TT.EOF:
                break
        return tokens

    # ── internals ─────────────────────────────────────────────────────────────

    def _peek(self, offset: int = 0) -> str:
        idx = self._pos + offset
        return self._src[idx] if idx < len(self._src) else "\0"

    def _advance(self) -> str:
        ch = self._src[self._pos]
        self._pos += 1
        if ch == "\n":
            self._line += 1
            self._col = 1
        else:
            self._col += 1
        return ch

    def _skip_whitespace_and_comments(self):
        while self._pos < len(self._src):
            ch = self._peek()
            if ch in " \t\r\n":
                self._advance()
            elif ch == "/" and self._peek(1) == "/":
                # Line comment
                while self._pos < len(self._src) and self._peek() != "\n":
                    self._advance()
            elif ch == "/" and self._peek(1) == "*":
                # Block comment
                self._advance(); self._advance()
                while self._pos < len(self._src):
                    if self._peek() == "*" and self._peek(1) == "/":
                        self._advance(); self._advance()
                        break
                    self._advance()
            else:
                break

    def _next_token(self) -> Token:
        self._skip_whitespace_and_comments()
        line, col = self._line, self._col

        if self._pos >= len(self._src):
            return Token(TT.EOF, None, line, col)

        ch = self._peek()

        # ── @ prefixed (wrappers / @cui-cpp) ──────────────────────────────
        if ch == "@":
            return self._lex_at(line, col)

        # ── String literal ─────────────────────────────────────────────────
        if ch == '"':
            return self._lex_string(line, col)

        # ── Numbers ────────────────────────────────────────────────────────
        if ch.isdigit() or (ch == "." and self._peek(1).isdigit()):
            return self._lex_number(line, col)

        # ── Identifiers / keywords ─────────────────────────────────────────
        if ch.isalpha() or ch == "_":
            return self._lex_ident(line, col)

        # ── Two-char operators ─────────────────────────────────────────────
        if ch == "-" and self._peek(1) == ">":
            self._advance(); self._advance()
            return Token(TT.ARROW, "->", line, col)

        if ch == ":" and self._peek(1) == ":":
            self._advance(); self._advance()
            return Token(TT.DCOLON, "::", line, col)

        # ── Single-char operators ──────────────────────────────────────────
        SINGLES = {
            "=": TT.EQ,    ":": TT.COLON,  ",": TT.COMMA,
            ".": TT.DOT,   "(": TT.LPAREN, ")": TT.RPAREN,
            "{": TT.LBRACE, "}": TT.RBRACE, "?": TT.QUESTION,
            "+": TT.PLUS,  "-": TT.MINUS,  "*": TT.STAR,
            "/": TT.SLASH, ";": TT.SEMICOLON,
        }
        if ch in SINGLES:
            self._advance()
            return Token(SINGLES[ch], ch, line, col)

        raise LexError(f"Unexpected character {ch!r}", line, col)

    # ── @ tokens ──────────────────────────────────────────────────────────────

    def _lex_at(self, line, col) -> Token:
        start = self._pos
        self._advance()  # consume '@'
        while self._pos < len(self._src) and (self._peek().isalnum() or self._peek() in "-_"):
            self._advance()
        word = self._src[start:self._pos]
        if word in WRAPPERS:
            return Token(WRAPPERS[word], word, line, col)
        raise LexError(f"Unknown @ token: {word!r}", line, col)

    # ── Identifiers ───────────────────────────────────────────────────────────

    def _lex_ident(self, line, col) -> Token:
        start = self._pos
        while self._pos < len(self._src) and (self._peek().isalnum() or self._peek() == "_"):
            self._advance()
        word = self._src[start:self._pos]
        tt = KEYWORDS.get(word, TT.IDENT)
        value = True if tt is TT.KW_TRUE else (False if tt is TT.KW_FALSE else word)
        return Token(tt, value, line, col)

    # ── Numbers ───────────────────────────────────────────────────────────────

    def _lex_number(self, line, col) -> Token:
        start = self._pos
        is_float = False
        while self._pos < len(self._src) and self._peek().isdigit():
            self._advance()
        if self._peek() == "." and self._peek(1).isdigit():
            is_float = True
            self._advance()
            while self._pos < len(self._src) and self._peek().isdigit():
                self._advance()
        # Scientific notation: 1e-6, 1.5E+3, etc.
        if self._peek() in ("e", "E"):
            next1 = self._peek(1)
            if next1.isdigit() or next1 in ("+", "-"):
                is_float = True
                self._advance()  # consume e/E
                if self._peek() in ("+", "-"):
                    self._advance()
                while self._pos < len(self._src) and self._peek().isdigit():
                    self._advance()
        # Optional suffix: .s .ms .us .ns (duration literals)
        if self._peek() == ".":
            suffix_start = self._pos
            self._advance()
            suffix = ""
            while self._pos < len(self._src) and self._peek().isalpha():
                suffix += self._advance()
            if suffix in ("s", "ms", "us", "ns"):
                raw = self._src[start:suffix_start]
                return Token(TT.FLOAT_LIT, _to_seconds(float(raw), suffix), line, col)
            else:
                # Not a duration suffix — put back the '.' and letters
                self._pos = suffix_start
        text = self._src[start:self._pos]
        if is_float or "." in text:
            return Token(TT.FLOAT_LIT, float(text), line, col)
        return Token(TT.INT_LIT, int(text), line, col)

    # ── String literals with interpolation ───────────────────────────────────

    def _lex_string(self, line, col) -> Token:
        self._advance()  # consume opening "
        segments = []
        buf = ""

        while self._pos < len(self._src):
            ch = self._peek()

            if ch == '"':
                self._advance()
                if buf:
                    segments.append(StringSegment(buf))
                return Token(TT.STRING, StringLit(segments), line, col)

            if ch == "\\":
                if self._peek(1) == "(":
                    # Interpolation
                    if buf:
                        segments.append(StringSegment(buf))
                        buf = ""
                    self._advance()  # '\'
                    self._advance()  # '('
                    capture, expr_tokens = self._lex_interp_expr()
                    segments.append(InterpSegment(capture, expr_tokens))
                    continue
                else:
                    # Simple escape
                    self._advance()
                    esc = self._advance()
                    ESC = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}
                    buf += ESC.get(esc, esc)
                    continue

            buf += self._advance()

        raise LexError("Unterminated string literal", line, col)

    def _lex_interp_expr(self) -> tuple[Optional[str], list[Token]]:
        """Lex the expression inside \\(...). Returns (capture_mode, tokens)."""
        capture = None
        tokens = []
        depth = 1  # we already consumed the opening '('

        # Peek for once/always
        saved_pos  = self._pos
        saved_line = self._line
        saved_col  = self._col
        self._skip_whitespace_and_comments()
        if self._pos < len(self._src) and (self._peek().isalpha() or self._peek() == "_"):
            word_start = self._pos
            while self._pos < len(self._src) and (self._peek().isalnum() or self._peek() == "_"):
                self._advance()
            word = self._src[word_start:self._pos]
            if word in ("once", "always"):
                capture = word
            else:
                # Not a capture keyword — restore
                self._pos  = saved_pos
                self._line = saved_line
                self._col  = saved_col
        else:
            self._pos  = saved_pos
            self._line = saved_line
            self._col  = saved_col

        # Lex tokens until the matching ')'
        while self._pos < len(self._src):
            self._skip_whitespace_and_comments()
            if self._pos >= len(self._src):
                break
            ch = self._peek()
            if ch == "(" :
                depth += 1
                tok = self._next_token()
                tokens.append(tok)
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    self._advance()  # consume ')'
                    break
                tok = self._next_token()
                tokens.append(tok)
            else:
                tok = self._next_token()
                tokens.append(tok)

        return capture, tokens


# ── Helpers ───────────────────────────────────────────────────────────────────

def _to_seconds(value: float, suffix: str) -> float:
    factors = {"s": 1.0, "ms": 1e-3, "us": 1e-6, "ns": 1e-9}
    return value * factors[suffix]
