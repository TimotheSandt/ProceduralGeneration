use crate::ast::{
    CppBlock, Element, Expr, FileAst, FuncDecl, ImportDecl, LetDecl, Modifier, Param, StringPart,
    ViewDecl, ViewField, WrappedField,
};
use crate::lexer::{StringTokenPart, Token, TokenType, TokenValue};
use std::collections::BTreeMap;

#[derive(Clone, Debug)]
pub struct ParseError {
    pub message: String,
    pub token: Token,
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Parse error at {}:{}: {} (got {:?})",
            self.token.line, self.token.col, self.message, self.token.ty
        )
    }
}

pub fn parse(tokens: Vec<Token>) -> Result<FileAst, ParseError> {
    Parser::new(tokens).parse_file()
}

struct Parser {
    tokens: Vec<Token>,
    pos: usize,
}

impl Parser {
    fn new(mut tokens: Vec<Token>) -> Self {
        tokens.retain(|token| token.ty != TokenType::Eof);
        tokens.push(Token::new(TokenType::Eof, TokenValue::None, 0, 0));
        Self { tokens, pos: 0 }
    }

    fn parse_file(&mut self) -> Result<FileAst, ParseError> {
        let mut imports = Vec::new();
        while self.at(TokenType::KwImport) {
            imports.push(self.parse_import()?);
        }
        let mut views = Vec::new();
        while !self.at(TokenType::Eof) {
            views.push(self.parse_view()?);
        }
        Ok(FileAst { imports, views })
    }

    fn parse_import(&mut self) -> Result<ImportDecl, ParseError> {
        self.expect(TokenType::KwImport)?;
        let token = self.expect(TokenType::String)?;
        let mut path = String::new();
        if let TokenValue::String(parts) = token.value {
            for part in parts {
                if let StringTokenPart::Text(text) = part {
                    path.push_str(&text);
                }
            }
        }
        Ok(ImportDecl { path })
    }

    fn parse_view(&mut self) -> Result<ViewDecl, ParseError> {
        self.expect(TokenType::KwView)?;
        let name_token = self.expect(TokenType::Ident)?;
        let name = name_token.text();
        self.expect(TokenType::LBrace)?;
        let mut fields = Vec::new();
        let mut root = None;
        while !self.at(TokenType::RBrace) && !self.at(TokenType::Eof) {
            match self.parse_view_stmt()? {
                ViewStmt::Field(field) => fields.push(field),
                ViewStmt::Element(element) => {
                    if root.is_some() {
                        return Err(self.error("A view may only have one root element"));
                    }
                    root = Some(element);
                }
            }
        }
        self.expect(TokenType::RBrace)?;
        let root = root.ok_or_else(|| self.error("View has no root element"))?;
        Ok(ViewDecl {
            name,
            fields,
            root,
            line: name_token.line,
            col: name_token.col,
        })
    }

    fn parse_view_stmt(&mut self) -> Result<ViewStmt, ParseError> {
        match self.peek().ty {
            TokenType::AtObserved
            | TokenType::AtMutable
            | TokenType::AtSnapshot
            | TokenType::AtState
            | TokenType::AtBinding => Ok(ViewStmt::Field(ViewField::Wrapped(
                self.parse_wrapped_field()?,
            ))),
            TokenType::KwLet => Ok(ViewStmt::Field(ViewField::Let(self.parse_let()?))),
            TokenType::KwFunc => Ok(ViewStmt::Field(ViewField::Func(self.parse_func()?))),
            TokenType::AtCuiCpp => Ok(ViewStmt::Field(ViewField::Cpp(self.parse_cpp_block()?))),
            _ => Ok(ViewStmt::Element(self.parse_element()?)),
        }
    }

    fn parse_wrapped_field(&mut self) -> Result<WrappedField, ParseError> {
        let wrapper_token = self.advance().clone();
        let wrapper = wrapper_token.text();
        self.expect(TokenType::KwVar)?;
        let name = self.expect(TokenType::Ident)?.text();
        self.expect(TokenType::Colon)?;
        let (type_name, optional) = self.parse_type()?;
        let default = if self.match_ty(TokenType::Eq).is_some() {
            Some(self.parse_expr()?)
        } else {
            None
        };
        Ok(WrappedField {
            wrapper,
            name,
            type_name,
            optional,
            default,
            line: wrapper_token.line,
            col: wrapper_token.col,
        })
    }

    fn parse_type(&mut self) -> Result<(String, bool), ParseError> {
        let mut parts = vec![self.expect(TokenType::Ident)?.text()];
        while self.at(TokenType::DColon) {
            self.advance();
            parts.push(self.expect(TokenType::Ident)?.text());
        }
        let optional = self.match_ty(TokenType::Question).is_some();
        Ok((parts.join("::"), optional))
    }

    fn parse_let(&mut self) -> Result<LetDecl, ParseError> {
        let let_token = self.expect(TokenType::KwLet)?;
        let name = self.expect(TokenType::Ident)?.text();
        let type_name = if self.match_ty(TokenType::Colon).is_some() {
            Some(self.parse_type()?.0)
        } else {
            None
        };
        self.expect(TokenType::Eq)?;
        let value = self.parse_expr()?;
        Ok(LetDecl {
            name,
            type_name,
            value,
            line: let_token.line,
            col: let_token.col,
        })
    }

    fn parse_func(&mut self) -> Result<FuncDecl, ParseError> {
        let func_token = self.expect(TokenType::KwFunc)?;
        let name = self.expect(TokenType::Ident)?.text();
        self.expect(TokenType::LParen)?;
        let params = self.parse_param_list()?;
        self.expect(TokenType::RParen)?;
        self.expect(TokenType::Arrow)?;
        let return_type = self.parse_type()?.0;
        let body = if self.match_ty(TokenType::Eq).is_some() {
            self.parse_expr()?
        } else {
            self.expect(TokenType::LBrace)?;
            self.expect(TokenType::KwReturn)?;
            let expr = self.parse_expr()?;
            self.match_ty(TokenType::Semicolon);
            self.expect(TokenType::RBrace)?;
            expr
        };
        Ok(FuncDecl {
            name,
            params,
            return_type,
            body,
            line: func_token.line,
            col: func_token.col,
        })
    }

    fn parse_param_list(&mut self) -> Result<Vec<Param>, ParseError> {
        let mut params = Vec::new();
        if self.at(TokenType::RParen) {
            return Ok(params);
        }
        params.push(self.parse_param()?);
        while self.match_ty(TokenType::Comma).is_some() {
            params.push(self.parse_param()?);
        }
        Ok(params)
    }

    fn parse_param(&mut self) -> Result<Param, ParseError> {
        let name = self.expect(TokenType::Ident)?.text();
        self.expect(TokenType::Colon)?;
        let type_name = self.parse_type()?.0;
        let default = if self.match_ty(TokenType::Eq).is_some() {
            Some(self.parse_expr()?)
        } else {
            None
        };
        Ok(Param {
            name,
            type_name,
            default,
        })
    }

    fn parse_cpp_block(&mut self) -> Result<CppBlock, ParseError> {
        let cpp_token = self.expect(TokenType::AtCuiCpp)?;
        self.expect(TokenType::LBrace)?;
        let mut depth = 1usize;
        let mut parts = Vec::new();
        while !self.at(TokenType::Eof) {
            let token = self.advance().clone();
            match token.ty {
                TokenType::LBrace => {
                    depth += 1;
                    parts.push("{".to_string());
                }
                TokenType::RBrace => {
                    depth -= 1;
                    if depth == 0 {
                        break;
                    }
                    parts.push("}".to_string());
                }
                _ => parts.push(token.text()),
            }
        }
        Ok(CppBlock {
            content: parts.join(" "),
            line: cpp_token.line,
            col: cpp_token.col,
        })
    }

    fn parse_element(&mut self) -> Result<Element, ParseError> {
        let name_token = self.expect(TokenType::Ident)?;
        let name = name_token.text();
        let (args, named_args) = if self.at(TokenType::LParen) {
            self.parse_call_args()?
        } else {
            (Vec::new(), BTreeMap::new())
        };
        let mut children = Vec::new();
        if self.at(TokenType::LBrace) {
            self.advance();
            while !self.at(TokenType::RBrace) && !self.at(TokenType::Eof) {
                children.push(self.parse_element()?);
            }
            self.expect(TokenType::RBrace)?;
        }
        let mut modifiers = Vec::new();
        while self.at(TokenType::Dot) {
            modifiers.push(self.parse_modifier()?);
        }
        Ok(Element {
            name,
            args,
            named_args,
            children,
            modifiers,
            line: name_token.line,
            col: name_token.col,
        })
    }

    fn parse_modifier(&mut self) -> Result<Modifier, ParseError> {
        let dot = self.expect(TokenType::Dot)?;
        let name = self.expect(TokenType::Ident)?.text();
        let (args, named_args) = if self.at(TokenType::LParen) {
            self.parse_call_args()?
        } else {
            (Vec::new(), BTreeMap::new())
        };
        Ok(Modifier {
            name,
            args,
            named_args,
            line: dot.line,
            col: dot.col,
        })
    }

    fn parse_call_args(&mut self) -> Result<(Vec<Expr>, BTreeMap<String, Expr>), ParseError> {
        self.expect(TokenType::LParen)?;
        let mut args = Vec::new();
        let mut named_args = BTreeMap::new();
        if !self.at(TokenType::RParen) {
            self.parse_one_arg(&mut args, &mut named_args)?;
            while self.match_ty(TokenType::Comma).is_some() {
                self.parse_one_arg(&mut args, &mut named_args)?;
            }
        }
        self.expect(TokenType::RParen)?;
        Ok((args, named_args))
    }

    fn parse_one_arg(
        &mut self,
        args: &mut Vec<Expr>,
        named_args: &mut BTreeMap<String, Expr>,
    ) -> Result<(), ParseError> {
        if self.at(TokenType::Ident) && self.peek_n(1).ty == TokenType::Colon {
            let label = self.advance().text();
            self.advance();
            named_args.insert(label, self.parse_expr()?);
        } else {
            args.push(self.parse_expr()?);
        }
        Ok(())
    }

    fn parse_expr(&mut self) -> Result<Expr, ParseError> {
        self.parse_binary()
    }

    fn parse_binary(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_unary()?;
        while matches!(
            self.peek().ty,
            TokenType::Plus | TokenType::Minus | TokenType::Star | TokenType::Slash
        ) {
            let op = self.advance().text();
            let right = self.parse_unary()?;
            left = Expr::Binary {
                op,
                left: Box::new(left),
                right: Box::new(right),
            };
        }
        Ok(left)
    }

    fn parse_unary(&mut self) -> Result<Expr, ParseError> {
        if self.at(TokenType::KwOnce) || self.at(TokenType::KwAlways) {
            let mode = if self.advance().ty == TokenType::KwOnce {
                "once"
            } else {
                "always"
            };
            let expr = self.parse_primary()?;
            return Ok(Expr::Capture {
                mode: mode.to_string(),
                expr: Box::new(expr),
            });
        }
        self.parse_postfix()
    }

    fn parse_postfix(&mut self) -> Result<Expr, ParseError> {
        let mut node = self.parse_primary()?;
        while self.at(TokenType::Dot) {
            self.advance();
            let prop = self.expect(TokenType::Ident)?.text();
            if self.at(TokenType::LParen) {
                let (args, named_args) = self.parse_call_args()?;
                node = Expr::Call {
                    callee: format!("{}.{}", expr_to_str(&node), prop),
                    args,
                    named_args,
                };
            } else {
                node = Expr::PropertyAccess {
                    obj: expr_to_str(&node),
                    prop,
                };
            }
        }
        Ok(node)
    }

    fn parse_primary(&mut self) -> Result<Expr, ParseError> {
        let token = self.peek().clone();
        match token.ty {
            TokenType::IntLit => {
                self.advance();
                if let TokenValue::Int(value) = token.value {
                    Ok(Expr::Number(value as f64))
                } else {
                    unreachable!()
                }
            }
            TokenType::FloatLit => {
                self.advance();
                if let TokenValue::Float(value) = token.value {
                    Ok(Expr::Number(value))
                } else {
                    unreachable!()
                }
            }
            TokenType::KwTrue | TokenType::KwFalse => {
                self.advance();
                if let TokenValue::Bool(value) = token.value {
                    Ok(Expr::Bool(value))
                } else {
                    unreachable!()
                }
            }
            TokenType::String => {
                self.advance();
                if let TokenValue::String(parts) = token.value {
                    self.process_string(parts)
                } else {
                    unreachable!()
                }
            }
            TokenType::Dot => {
                self.advance();
                let name = self.expect(TokenType::Ident)?.text();
                Ok(Expr::EnumCase(name))
            }
            TokenType::Ident => self.parse_ident_or_call(),
            TokenType::LParen => {
                self.advance();
                let expr = self.parse_expr()?;
                self.expect(TokenType::RParen)?;
                Ok(expr)
            }
            _ => Err(self.error("Expected expression")),
        }
    }

    fn parse_ident_or_call(&mut self) -> Result<Expr, ParseError> {
        let mut parts = vec![self.expect(TokenType::Ident)?.text()];
        while self.at(TokenType::DColon) {
            self.advance();
            parts.push(self.expect(TokenType::Ident)?.text());
        }
        let name = parts.join("::");
        if self.at(TokenType::LParen) {
            let (args, named_args) = self.parse_call_args()?;
            return Ok(Expr::Call {
                callee: name,
                args,
                named_args,
            });
        }
        Ok(Expr::Identifier(name))
    }

    fn process_string(&self, parts: Vec<StringTokenPart>) -> Result<Expr, ParseError> {
        let mut processed = Vec::new();
        for part in parts {
            match part {
                StringTokenPart::Text(text) => processed.push(StringPart::Text(text)),
                StringTokenPart::Interp {
                    capture,
                    mut tokens,
                } => {
                    tokens.push(Token::new(TokenType::Eof, TokenValue::None, 0, 0));
                    let mut parser = Parser::new(tokens);
                    let expr = parser.parse_expr()?;
                    processed.push(StringPart::Interp { capture, expr });
                }
            }
        }
        Ok(Expr::StringLit(processed))
    }

    fn expect(&mut self, ty: TokenType) -> Result<Token, ParseError> {
        if self.peek().ty != ty {
            return Err(self.error(&format!("expected {ty:?}")));
        }
        Ok(self.advance().clone())
    }

    fn match_ty(&mut self, ty: TokenType) -> Option<Token> {
        if self.at(ty) {
            Some(self.advance().clone())
        } else {
            None
        }
    }

    fn at(&self, ty: TokenType) -> bool {
        self.peek().ty == ty
    }

    fn advance(&mut self) -> &Token {
        let token = &self.tokens[self.pos];
        if self.pos + 1 < self.tokens.len() {
            self.pos += 1;
        }
        token
    }

    fn peek(&self) -> &Token {
        self.peek_n(0)
    }

    fn peek_n(&self, offset: usize) -> &Token {
        self.tokens
            .get(self.pos + offset)
            .unwrap_or_else(|| self.tokens.last().expect("parser always has EOF"))
    }

    fn error(&self, message: &str) -> ParseError {
        ParseError {
            message: message.to_string(),
            token: self.peek().clone(),
        }
    }
}

enum ViewStmt {
    Field(ViewField),
    Element(Element),
}

fn expr_to_str(node: &Expr) -> String {
    match node {
        Expr::Identifier(name) => name.clone(),
        Expr::PropertyAccess { obj, prop } => format!("{obj}.{prop}"),
        _ => "?".to_string(),
    }
}
