#![allow(dead_code)]

use std::collections::BTreeMap;

#[derive(Clone, Debug)]
pub enum Expr {
    Number(f64),
    Bool(bool),
    Identifier(String),
    EnumCase(String),
    PropertyAccess {
        obj: String,
        prop: String,
    },
    Call {
        callee: String,
        args: Vec<Expr>,
        named_args: BTreeMap<String, Expr>,
    },
    Binary {
        op: String,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    Capture {
        mode: String,
        expr: Box<Expr>,
    },
    StringLit(Vec<StringPart>),
}

#[derive(Clone, Debug)]
pub enum StringPart {
    Text(String),
    Interp { capture: Option<String>, expr: Expr },
}

#[derive(Clone, Debug)]
pub struct LetDecl {
    pub name: String,
    pub type_name: Option<String>,
    pub value: Expr,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct Param {
    pub name: String,
    pub type_name: String,
    pub default: Option<Expr>,
}

#[derive(Clone, Debug)]
pub struct FuncDecl {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: String,
    pub body: Expr,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct WrappedField {
    pub wrapper: String,
    pub name: String,
    pub type_name: String,
    pub optional: bool,
    pub default: Option<Expr>,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct CppBlock {
    pub content: String,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct Modifier {
    pub name: String,
    pub args: Vec<Expr>,
    pub named_args: BTreeMap<String, Expr>,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct Element {
    pub name: String,
    pub args: Vec<Expr>,
    pub named_args: BTreeMap<String, Expr>,
    pub children: Vec<Element>,
    pub modifiers: Vec<Modifier>,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub enum ViewField {
    Wrapped(WrappedField),
    Let(LetDecl),
    Func(FuncDecl),
    Cpp(CppBlock),
}

#[derive(Clone, Debug)]
pub struct ViewDecl {
    pub name: String,
    pub fields: Vec<ViewField>,
    pub root: Element,
    pub line: usize,
    pub col: usize,
}

#[derive(Clone, Debug)]
pub struct ImportDecl {
    pub path: String,
}

#[derive(Clone, Debug)]
pub struct FileAst {
    pub imports: Vec<ImportDecl>,
    pub views: Vec<ViewDecl>,
}
