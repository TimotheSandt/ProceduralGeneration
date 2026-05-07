#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TokenType {
    KwView,
    KwLet,
    KwFunc,
    KwOnce,
    KwAlways,
    KwVar,
    KwImport,
    KwTrue,
    KwFalse,
    KwReturn,
    AtObserved,
    AtMutable,
    AtSnapshot,
    AtState,
    AtBinding,
    AtCuiCpp,
    Ident,
    IntLit,
    FloatLit,
    String,
    Arrow,
    Eq,
    Colon,
    Comma,
    Dot,
    DColon,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Question,
    Plus,
    Minus,
    Star,
    Slash,
    Semicolon,
    Eof,
}

#[derive(Clone, Debug)]
pub enum StringTokenPart {
    Text(String),
    Interp {
        capture: Option<String>,
        tokens: Vec<Token>,
    },
}

#[derive(Clone, Debug)]
pub enum TokenValue {
    None,
    Text(String),
    Int(i64),
    Float(f64),
    Bool(bool),
    String(Vec<StringTokenPart>),
}

#[derive(Clone, Debug)]
pub struct Token {
    pub ty: TokenType,
    pub value: TokenValue,
    pub line: usize,
    pub col: usize,
}

impl Token {
    pub fn new(ty: TokenType, value: TokenValue, line: usize, col: usize) -> Self {
        Self {
            ty,
            value,
            line,
            col,
        }
    }

    pub fn text(&self) -> String {
        match &self.value {
            TokenValue::Text(value) => value.clone(),
            TokenValue::Int(value) => value.to_string(),
            TokenValue::Float(value) => format_float(*value),
            TokenValue::Bool(value) => value.to_string(),
            TokenValue::String(_) | TokenValue::None => String::new(),
        }
    }
}

#[derive(Clone, Debug)]
pub struct LexError {
    pub message: String,
    pub line: usize,
    pub col: usize,
}

impl std::fmt::Display for LexError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Lex error at {}:{}: {}",
            self.line, self.col, self.message
        )
    }
}

pub struct Lexer {
    chars: Vec<char>,
    pos: usize,
    line: usize,
    col: usize,
}

impl Lexer {
    pub fn new(source: &str) -> Self {
        Self {
            chars: source.chars().collect(),
            pos: 0,
            line: 1,
            col: 1,
        }
    }

    pub fn tokenize(&mut self) -> Result<Vec<Token>, LexError> {
        let mut tokens = Vec::new();
        loop {
            let token = self.next_token()?;
            let done = token.ty == TokenType::Eof;
            tokens.push(token);
            if done {
                break;
            }
        }
        Ok(tokens)
    }

    fn next_token(&mut self) -> Result<Token, LexError> {
        self.skip_whitespace_and_comments();
        let line = self.line;
        let col = self.col;

        if self.pos >= self.chars.len() {
            return Ok(Token::new(TokenType::Eof, TokenValue::None, line, col));
        }

        let ch = self.peek(0);
        if ch == '@' {
            return self.lex_at(line, col);
        }
        if ch == '"' {
            return self.lex_string(line, col);
        }
        if ch.is_ascii_digit() || (ch == '.' && self.peek(1).is_ascii_digit()) {
            return self.lex_number(line, col);
        }
        if is_ident_start(ch) {
            return Ok(self.lex_ident(line, col));
        }
        if ch == '-' && self.peek(1) == '>' {
            self.advance();
            self.advance();
            return Ok(Token::new(
                TokenType::Arrow,
                TokenValue::Text("->".to_string()),
                line,
                col,
            ));
        }
        if ch == ':' && self.peek(1) == ':' {
            self.advance();
            self.advance();
            return Ok(Token::new(
                TokenType::DColon,
                TokenValue::Text("::".to_string()),
                line,
                col,
            ));
        }

        let ty = match ch {
            '=' => TokenType::Eq,
            ':' => TokenType::Colon,
            ',' => TokenType::Comma,
            '.' => TokenType::Dot,
            '(' => TokenType::LParen,
            ')' => TokenType::RParen,
            '{' => TokenType::LBrace,
            '}' => TokenType::RBrace,
            '?' => TokenType::Question,
            '+' => TokenType::Plus,
            '-' => TokenType::Minus,
            '*' => TokenType::Star,
            '/' => TokenType::Slash,
            ';' => TokenType::Semicolon,
            other => {
                return Err(self.error_at(format!("Unexpected character {other:?}"), line, col));
            }
        };
        self.advance();
        Ok(Token::new(ty, TokenValue::Text(ch.to_string()), line, col))
    }

    fn lex_at(&mut self, line: usize, col: usize) -> Result<Token, LexError> {
        let start = self.pos;
        self.advance();
        while is_ident_continue(self.peek(0)) || self.peek(0) == '-' {
            self.advance();
        }
        let word: String = self.chars[start..self.pos].iter().collect();
        let ty = match word.as_str() {
            "@Observed" => TokenType::AtObserved,
            "@Mutable" => TokenType::AtMutable,
            "@Snapshot" => TokenType::AtSnapshot,
            "@State" => TokenType::AtState,
            "@Binding" => TokenType::AtBinding,
            "@cui-cpp" => TokenType::AtCuiCpp,
            _ => {
                return Err(self.error_at(format!("Unknown @ token: {word:?}"), line, col));
            }
        };
        Ok(Token::new(ty, TokenValue::Text(word), line, col))
    }

    fn lex_ident(&mut self, line: usize, col: usize) -> Token {
        let start = self.pos;
        while is_ident_continue(self.peek(0)) {
            self.advance();
        }
        let word: String = self.chars[start..self.pos].iter().collect();
        let (ty, value) = match word.as_str() {
            "view" => (TokenType::KwView, TokenValue::Text(word)),
            "let" => (TokenType::KwLet, TokenValue::Text(word)),
            "func" => (TokenType::KwFunc, TokenValue::Text(word)),
            "once" => (TokenType::KwOnce, TokenValue::Text(word)),
            "always" => (TokenType::KwAlways, TokenValue::Text(word)),
            "var" => (TokenType::KwVar, TokenValue::Text(word)),
            "import" => (TokenType::KwImport, TokenValue::Text(word)),
            "true" => (TokenType::KwTrue, TokenValue::Bool(true)),
            "false" => (TokenType::KwFalse, TokenValue::Bool(false)),
            "return" => (TokenType::KwReturn, TokenValue::Text(word)),
            _ => (TokenType::Ident, TokenValue::Text(word)),
        };
        Token::new(ty, value, line, col)
    }

    fn lex_number(&mut self, line: usize, col: usize) -> Result<Token, LexError> {
        let start = self.pos;
        let mut is_float = false;
        while self.peek(0).is_ascii_digit() {
            self.advance();
        }
        if self.peek(0) == '.' && self.peek(1).is_ascii_digit() {
            is_float = true;
            self.advance();
            while self.peek(0).is_ascii_digit() {
                self.advance();
            }
        }
        if matches!(self.peek(0), 'e' | 'E') {
            let next = self.peek(1);
            if next.is_ascii_digit() || matches!(next, '+' | '-') {
                is_float = true;
                self.advance();
                if matches!(self.peek(0), '+' | '-') {
                    self.advance();
                }
                while self.peek(0).is_ascii_digit() {
                    self.advance();
                }
            }
        }
        if self.peek(0) == '.' {
            let suffix_start = self.pos;
            self.advance();
            let mut suffix = String::new();
            while self.peek(0).is_ascii_alphabetic() {
                suffix.push(self.advance());
            }
            if matches!(suffix.as_str(), "s" | "ms" | "us" | "ns") {
                let raw: String = self.chars[start..suffix_start].iter().collect();
                let value = raw.parse::<f64>().map_err(|err| {
                    self.error_at(format!("Invalid number {raw}: {err}"), line, col)
                })?;
                return Ok(Token::new(
                    TokenType::FloatLit,
                    TokenValue::Float(to_seconds(value, &suffix)),
                    line,
                    col,
                ));
            }
            self.pos = suffix_start;
        }

        let text: String = self.chars[start..self.pos].iter().collect();
        if is_float || text.contains('.') {
            let value = text
                .parse::<f64>()
                .map_err(|err| self.error_at(format!("Invalid float {text}: {err}"), line, col))?;
            Ok(Token::new(
                TokenType::FloatLit,
                TokenValue::Float(value),
                line,
                col,
            ))
        } else {
            let value = text
                .parse::<i64>()
                .map_err(|err| self.error_at(format!("Invalid int {text}: {err}"), line, col))?;
            Ok(Token::new(
                TokenType::IntLit,
                TokenValue::Int(value),
                line,
                col,
            ))
        }
    }

    fn lex_string(&mut self, line: usize, col: usize) -> Result<Token, LexError> {
        self.advance();
        let mut segments = Vec::new();
        let mut buffer = String::new();

        while self.pos < self.chars.len() {
            let ch = self.peek(0);
            if ch == '"' {
                self.advance();
                if !buffer.is_empty() {
                    segments.push(StringTokenPart::Text(std::mem::take(&mut buffer)));
                }
                return Ok(Token::new(
                    TokenType::String,
                    TokenValue::String(segments),
                    line,
                    col,
                ));
            }
            if ch == '\\' {
                if self.peek(1) == '(' {
                    if !buffer.is_empty() {
                        segments.push(StringTokenPart::Text(std::mem::take(&mut buffer)));
                    }
                    self.advance();
                    self.advance();
                    let (capture, tokens) = self.lex_interp_expr()?;
                    segments.push(StringTokenPart::Interp { capture, tokens });
                    continue;
                }
                self.advance();
                let escaped = self.advance();
                let value = match escaped {
                    'n' => '\n',
                    't' => '\t',
                    'r' => '\r',
                    '"' => '"',
                    '\\' => '\\',
                    other => other,
                };
                buffer.push(value);
                continue;
            }
            buffer.push(self.advance());
        }

        Err(self.error_at("Unterminated string literal".to_string(), line, col))
    }

    fn lex_interp_expr(&mut self) -> Result<(Option<String>, Vec<Token>), LexError> {
        let mut capture = None;
        let mut tokens = Vec::new();
        let mut depth = 1usize;

        let saved_pos = self.pos;
        let saved_line = self.line;
        let saved_col = self.col;
        self.skip_whitespace_and_comments();
        if is_ident_start(self.peek(0)) {
            let start = self.pos;
            while is_ident_continue(self.peek(0)) {
                self.advance();
            }
            let word: String = self.chars[start..self.pos].iter().collect();
            if word == "once" || word == "always" {
                capture = Some(word);
            } else {
                self.pos = saved_pos;
                self.line = saved_line;
                self.col = saved_col;
            }
        } else {
            self.pos = saved_pos;
            self.line = saved_line;
            self.col = saved_col;
        }

        while self.pos < self.chars.len() {
            self.skip_whitespace_and_comments();
            if self.pos >= self.chars.len() {
                break;
            }
            let ch = self.peek(0);
            if ch == '(' {
                depth += 1;
                tokens.push(self.next_token()?);
            } else if ch == ')' {
                depth -= 1;
                if depth == 0 {
                    self.advance();
                    break;
                }
                tokens.push(self.next_token()?);
            } else {
                tokens.push(self.next_token()?);
            }
        }

        Ok((capture, tokens))
    }

    fn skip_whitespace_and_comments(&mut self) {
        loop {
            if self.pos >= self.chars.len() {
                break;
            }
            let ch = self.peek(0);
            if matches!(ch, ' ' | '\t' | '\r' | '\n') {
                self.advance();
            } else if ch == '/' && self.peek(1) == '/' {
                while self.pos < self.chars.len() && self.peek(0) != '\n' {
                    self.advance();
                }
            } else if ch == '/' && self.peek(1) == '*' {
                self.advance();
                self.advance();
                while self.pos < self.chars.len() {
                    if self.peek(0) == '*' && self.peek(1) == '/' {
                        self.advance();
                        self.advance();
                        break;
                    }
                    self.advance();
                }
            } else {
                break;
            }
        }
    }

    fn peek(&self, offset: usize) -> char {
        self.chars.get(self.pos + offset).copied().unwrap_or('\0')
    }

    fn advance(&mut self) -> char {
        let ch = self.peek(0);
        self.pos += 1;
        if ch == '\n' {
            self.line += 1;
            self.col = 1;
        } else {
            self.col += 1;
        }
        ch
    }

    fn error_at(&self, message: String, line: usize, col: usize) -> LexError {
        LexError { message, line, col }
    }
}

fn is_ident_start(ch: char) -> bool {
    ch.is_ascii_alphabetic() || ch == '_'
}

fn is_ident_continue(ch: char) -> bool {
    ch.is_ascii_alphanumeric() || ch == '_'
}

fn to_seconds(value: f64, suffix: &str) -> f64 {
    match suffix {
        "s" => value,
        "ms" => value * 1e-3,
        "us" => value * 1e-6,
        "ns" => value * 1e-9,
        _ => value,
    }
}

fn format_float(value: f64) -> String {
    let mut text = value.to_string();
    if text.contains('e') || text.contains('E') {
        return text;
    }
    if text.contains('.') {
        while text.ends_with('0') {
            text.pop();
        }
        if text.ends_with('.') {
            text.push('0');
        }
    }
    text
}
