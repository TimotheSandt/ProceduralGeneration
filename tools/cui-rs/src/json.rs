#![allow(dead_code)]

use std::collections::BTreeMap;

#[derive(Clone, Debug)]
pub enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<JsonValue>),
    Object(BTreeMap<String, JsonValue>),
}

impl JsonValue {
    pub fn as_object(&self) -> Option<&BTreeMap<String, JsonValue>> {
        match self {
            JsonValue::Object(value) => Some(value),
            _ => None,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            JsonValue::String(value) => Some(value),
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            JsonValue::Bool(value) => Some(*value),
            _ => None,
        }
    }
}

pub fn parse_json(text: &str) -> Result<JsonValue, String> {
    let mut parser = JsonParser::new(text);
    let value = parser.parse_value()?;
    parser.skip_ws();
    if parser.peek().is_some() {
        return Err("trailing data after JSON value".to_string());
    }
    Ok(value)
}

pub fn json_escape(value: &str) -> String {
    let mut out = String::new();
    for ch in value.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c.is_control() => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

struct JsonParser<'a> {
    chars: Vec<char>,
    pos: usize,
    _source: &'a str,
}

impl<'a> JsonParser<'a> {
    fn new(source: &'a str) -> Self {
        Self {
            chars: source.chars().collect(),
            pos: 0,
            _source: source,
        }
    }

    fn parse_value(&mut self) -> Result<JsonValue, String> {
        self.skip_ws();
        match self.peek() {
            Some('"') => Ok(JsonValue::String(self.parse_string()?)),
            Some('{') => self.parse_object(),
            Some('[') => self.parse_array(),
            Some('t') => {
                self.expect_literal("true")?;
                Ok(JsonValue::Bool(true))
            }
            Some('f') => {
                self.expect_literal("false")?;
                Ok(JsonValue::Bool(false))
            }
            Some('n') => {
                self.expect_literal("null")?;
                Ok(JsonValue::Null)
            }
            Some('-' | '0'..='9') => self.parse_number(),
            Some(ch) => Err(format!("unexpected JSON character '{ch}'")),
            None => Err("unexpected end of JSON input".to_string()),
        }
    }

    fn parse_object(&mut self) -> Result<JsonValue, String> {
        self.expect_char('{')?;
        let mut object = BTreeMap::new();
        self.skip_ws();
        if self.consume_if('}') {
            return Ok(JsonValue::Object(object));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            self.expect_char(':')?;
            let value = self.parse_value()?;
            object.insert(key, value);
            self.skip_ws();
            if self.consume_if('}') {
                break;
            }
            self.expect_char(',')?;
        }
        Ok(JsonValue::Object(object))
    }

    fn parse_array(&mut self) -> Result<JsonValue, String> {
        self.expect_char('[')?;
        let mut array = Vec::new();
        self.skip_ws();
        if self.consume_if(']') {
            return Ok(JsonValue::Array(array));
        }
        loop {
            array.push(self.parse_value()?);
            self.skip_ws();
            if self.consume_if(']') {
                break;
            }
            self.expect_char(',')?;
        }
        Ok(JsonValue::Array(array))
    }

    fn parse_number(&mut self) -> Result<JsonValue, String> {
        let start = self.pos;
        self.consume_if('-');
        self.consume_digits();
        if self.consume_if('.') {
            self.consume_digits();
        }
        if matches!(self.peek(), Some('e' | 'E')) {
            self.pos += 1;
            if matches!(self.peek(), Some('+' | '-')) {
                self.pos += 1;
            }
            self.consume_digits();
        }
        let text: String = self.chars[start..self.pos].iter().collect();
        let value = text
            .parse::<f64>()
            .map_err(|err| format!("invalid JSON number {text}: {err}"))?;
        Ok(JsonValue::Number(value))
    }

    fn parse_string(&mut self) -> Result<String, String> {
        self.expect_char('"')?;
        let mut out = String::new();
        while let Some(ch) = self.next() {
            match ch {
                '"' => return Ok(out),
                '\\' => {
                    let escaped = self
                        .next()
                        .ok_or_else(|| "unterminated JSON escape".to_string())?;
                    match escaped {
                        '"' => out.push('"'),
                        '\\' => out.push('\\'),
                        '/' => out.push('/'),
                        'b' => out.push('\u{0008}'),
                        'f' => out.push('\u{000c}'),
                        'n' => out.push('\n'),
                        'r' => out.push('\r'),
                        't' => out.push('\t'),
                        'u' => {
                            let code = self.parse_hex4()?;
                            if let Some(c) = char::from_u32(code) {
                                out.push(c);
                            }
                        }
                        other => return Err(format!("unsupported JSON escape \\{other}")),
                    }
                }
                other => out.push(other),
            }
        }
        Err("unterminated JSON string".to_string())
    }

    fn parse_hex4(&mut self) -> Result<u32, String> {
        let mut value = 0u32;
        for _ in 0..4 {
            let ch = self
                .next()
                .ok_or_else(|| "unterminated JSON unicode escape".to_string())?;
            value = value * 16
                + ch.to_digit(16)
                    .ok_or_else(|| format!("invalid JSON unicode escape digit '{ch}'"))?;
        }
        Ok(value)
    }

    fn expect_literal(&mut self, literal: &str) -> Result<(), String> {
        for expected in literal.chars() {
            self.expect_char(expected)?;
        }
        Ok(())
    }

    fn expect_char(&mut self, expected: char) -> Result<(), String> {
        match self.next() {
            Some(ch) if ch == expected => Ok(()),
            Some(ch) => Err(format!("expected '{expected}', got '{ch}'")),
            None => Err(format!("expected '{expected}', got end of input")),
        }
    }

    fn consume_digits(&mut self) {
        while matches!(self.peek(), Some('0'..='9')) {
            self.pos += 1;
        }
    }

    fn consume_if(&mut self, expected: char) -> bool {
        if self.peek() == Some(expected) {
            self.pos += 1;
            true
        } else {
            false
        }
    }

    fn skip_ws(&mut self) {
        while matches!(self.peek(), Some(' ' | '\t' | '\r' | '\n')) {
            self.pos += 1;
        }
    }

    fn peek(&self) -> Option<char> {
        self.chars.get(self.pos).copied()
    }

    fn next(&mut self) -> Option<char> {
        let ch = self.peek()?;
        self.pos += 1;
        Some(ch)
    }
}
