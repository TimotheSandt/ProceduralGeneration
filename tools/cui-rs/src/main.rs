mod ast;
mod generator;
mod json;
mod lexer;
mod parser;
mod registry;
mod scanner;
mod validator;

use crate::generator::Generator;
use crate::json::json_escape;
use crate::lexer::{Lexer, TokenType};
use crate::registry::{Registry, registry_from_json, registry_to_json};
use crate::scanner::Scanner;
use crate::validator::validate;
use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::io::{self, Read};
use std::path::{Path, PathBuf};

const HASH_TAG: &str = "// @cui-hash:";

fn main() {
    if let Err(err) = run_cli() {
        eprintln!("{err}");
        std::process::exit(1);
    }
}

fn run_cli() -> Result<(), String> {
    let mut args = env::args().skip(1).collect::<Vec<_>>();
    if args.is_empty() {
        print_usage();
        return Err("missing command".to_string());
    }
    let command = args.remove(0);
    match command.as_str() {
        "scan" => cmd_scan(&args),
        "build" => cmd_build(&args),
        "run" => cmd_run(&args),
        "redirect" => cmd_redirect(&args),
        "check" => cmd_check(&args),
        "-h" | "--help" | "help" => {
            print_usage();
            Ok(())
        }
        _ => {
            print_usage();
            Err(format!("unknown command: {command}"))
        }
    }
}

fn cmd_scan(args: &[String]) -> Result<(), String> {
    let parsed = parse_args(args);
    let headers = required_opt(&parsed.options, "--headers")?;
    let output = required_opt(&parsed.options, "--output")?;
    let headers_dir = PathBuf::from(headers);
    if !headers_dir.is_dir() {
        return Err(format!(
            "Error: headers dir not found: {}",
            headers_dir.display()
        ));
    }
    let registry = Scanner::new().scan_directory(&headers_dir)?;
    let out_path = PathBuf::from(output);
    if let Some(parent) = out_path.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("failed to create {}: {err}", parent.display()))?;
    }
    fs::write(&out_path, registry_to_json(&registry))
        .map_err(|err| format!("failed to write {}: {err}", out_path.display()))?;
    println!(
        "[cui scan] {} component(s), {} function(s), {} enum(s) -> {}",
        registry.components.len(),
        registry.functions.len(),
        registry.enums.len(),
        out_path.display()
    );
    Ok(())
}

fn cmd_build(args: &[String]) -> Result<(), String> {
    let parsed = parse_args(args);
    let registry_path = required_opt(&parsed.options, "--registry")?;
    let output = required_opt(&parsed.options, "--output")?;
    let registry = read_registry(Path::new(registry_path))?;
    let out_dir = PathBuf::from(output);
    fs::create_dir_all(&out_dir)
        .map_err(|err| format!("failed to create {}: {err}", out_dir.display()))?;
    if parsed.positionals.is_empty() {
        return Err("build requires at least one .cui file".to_string());
    }
    for file in &parsed.positionals {
        let cui_path = PathBuf::from(file);
        if !cui_path.exists() {
            eprintln!("Error: file not found: {}", cui_path.display());
            continue;
        }
        compile_file(&cui_path, &out_dir, &registry)?;
    }
    Ok(())
}

fn cmd_run(args: &[String]) -> Result<(), String> {
    let parsed = parse_args(args);
    let headers = required_opt(&parsed.options, "--headers")?;
    let output = required_opt(&parsed.options, "--output")?;
    let headers_dir = PathBuf::from(headers);
    if !headers_dir.is_dir() {
        return Err(format!(
            "Error: headers dir not found: {}",
            headers_dir.display()
        ));
    }
    let registry = Scanner::new().scan_directory(&headers_dir)?;
    let out_dir = PathBuf::from(output);
    fs::create_dir_all(&out_dir)
        .map_err(|err| format!("failed to create {}: {err}", out_dir.display()))?;
    if parsed.positionals.is_empty() {
        return Err("run requires at least one .cui file".to_string());
    }
    for file in &parsed.positionals {
        let cui_path = PathBuf::from(file);
        if !cui_path.exists() {
            eprintln!("Error: file not found: {}", cui_path.display());
            continue;
        }
        compile_file(&cui_path, &out_dir, &registry)?;
    }
    Ok(())
}

fn cmd_redirect(args: &[String]) -> Result<(), String> {
    let parsed = parse_args(args);
    let output = required_opt(&parsed.options, "--output")?;
    let out_dir = PathBuf::from(output);
    fs::create_dir_all(&out_dir)
        .map_err(|err| format!("failed to create {}: {err}", out_dir.display()))?;
    if parsed.positionals.is_empty() {
        return Err("redirect requires at least one .cui file".to_string());
    }
    for file in &parsed.positionals {
        let cui_path = PathBuf::from(file);
        write_redirect(&out_dir, &cui_path)?;
        println!(
            "[cui] redirect: {}",
            out_dir.join(file_name(&cui_path)).display()
        );
    }
    Ok(())
}

fn cmd_check(args: &[String]) -> Result<(), String> {
    let parsed = parse_args(args);
    let read_stdin = parsed.flags.iter().any(|flag| flag == "--stdin");
    let file = parsed
        .positionals
        .first()
        .cloned()
        .unwrap_or_else(|| "<stdin>".to_string());
    let source = if read_stdin {
        let mut source = String::new();
        io::stdin()
            .read_to_string(&mut source)
            .map_err(|err| format!("failed to read stdin: {err}"))?;
        source
    } else {
        let path = Path::new(&file);
        if !path.exists() {
            println!(
                "[{}]",
                diagnostic_json(
                    1,
                    1,
                    1,
                    1,
                    &format!("File not found: {}", path.display()),
                    "error"
                )
            );
            return Ok(());
        }
        fs::read_to_string(path)
            .map_err(|err| format!("failed to read {}: {err}", path.display()))?
    };

    let tokens = match Lexer::new(&source).tokenize() {
        Ok(tokens) => tokens,
        Err(err) => {
            println!(
                "[{}]",
                diagnostic_json(
                    err.line,
                    err.col,
                    err.line,
                    err.col + 1,
                    &err.to_string(),
                    "error"
                )
            );
            return Ok(());
        }
    };

    let ast = match parser::parse(tokens) {
        Ok(ast) => ast,
        Err(err) => {
            let end_col = err.token.col + token_len(&err.token).max(1);
            println!(
                "[{}]",
                diagnostic_json(
                    err.token.line.max(1),
                    err.token.col.max(1),
                    err.token.line.max(1),
                    end_col,
                    &err.to_string(),
                    "error"
                )
            );
            return Ok(());
        }
    };

    let mut diagnostics = Vec::new();
    if let Some(registry_path) = parsed.options.get("--registry") {
        let path = Path::new(registry_path);
        if path.exists() {
            let registry = read_registry(path)?;
            for diagnostic in validate(&ast, &registry) {
                let line = diagnostic.line.max(1);
                let col = diagnostic.col.max(1);
                diagnostics.push(diagnostic_json(
                    line,
                    col,
                    line,
                    col + 1,
                    &diagnostic.message,
                    if diagnostic.is_warning {
                        "warning"
                    } else {
                        "error"
                    },
                ));
            }
        }
    }
    println!("[{}]", diagnostics.join(","));
    Ok(())
}

fn compile_file(cui_path: &Path, out_dir: &Path, registry: &Registry) -> Result<(), String> {
    let source = fs::read_to_string(cui_path)
        .map_err(|err| format!("failed to read {}: {err}", cui_path.display()))?;
    let stem = cui_path
        .file_stem()
        .and_then(|stem| stem.to_str())
        .ok_or_else(|| format!("invalid .cui filename: {}", cui_path.display()))?;
    let header_path = out_dir.join(format!("{stem}.gen.h"));
    let cpp_path = out_dir.join(format!("{stem}.gen.cpp"));

    write_redirect(out_dir, cui_path)?;

    let current_hash = source_hash(&source);
    if read_stored_hash(&header_path).as_deref() == Some(current_hash.as_str()) {
        println!("[cui] {} unchanged, skipping", file_name(cui_path));
        return Ok(());
    }

    let tokens = Lexer::new(&source)
        .tokenize()
        .map_err(|err| format!("[cui] ERROR parsing {}: {}", cui_path.display(), err))?;
    let ast = parser::parse(tokens)
        .map_err(|err| format!("[cui] ERROR parsing {}: {}", cui_path.display(), err))?;
    let diagnostics = validate(&ast, registry);
    for diagnostic in &diagnostics {
        eprintln!("[cui] {}: {}", file_name(cui_path), diagnostic);
    }
    if diagnostics.iter().any(|diagnostic| diagnostic.fatal()) {
        return Err(format!("[cui] ERROR validating {}", cui_path.display()));
    }

    let (header, implementation) = Generator::new(registry, file_name(cui_path)).generate(&ast)?;
    fs::write(&header_path, format!("{HASH_TAG} {current_hash}\n{header}"))
        .map_err(|err| format!("failed to write {}: {err}", header_path.display()))?;
    fs::write(&cpp_path, implementation)
        .map_err(|err| format!("failed to write {}: {err}", cpp_path.display()))?;
    println!(
        "[cui] {} -> {}, {}",
        file_name(cui_path),
        file_name(&header_path),
        file_name(&cpp_path)
    );
    Ok(())
}

fn write_redirect(out_dir: &Path, cui_path: &Path) -> Result<(), String> {
    fs::create_dir_all(out_dir)
        .map_err(|err| format!("failed to create {}: {err}", out_dir.display()))?;
    let cui_name = file_name(cui_path);
    let stem = cui_path
        .file_stem()
        .and_then(|stem| stem.to_str())
        .ok_or_else(|| format!("invalid .cui filename: {}", cui_path.display()))?;
    let redirect_path = out_dir.join(&cui_name);
    fs::write(
        &redirect_path,
        format!(
            "// AUTO-GENERATED - do not edit. Include redirect for {cui_name}\n#pragma once\n#include \"{stem}.gen.h\"\n"
        ),
    )
    .map_err(|err| format!("failed to write {}: {err}", redirect_path.display()))?;
    Ok(())
}

fn read_registry(path: &Path) -> Result<Registry, String> {
    if !path.exists() {
        return Err(format!("Error: registry not found: {}", path.display()));
    }
    let text = fs::read_to_string(path)
        .map_err(|err| format!("failed to read {}: {err}", path.display()))?;
    registry_from_json(&text)
}

fn source_hash(source: &str) -> String {
    let mut hash = 0xcbf29ce484222325u64;
    for byte in source.as_bytes() {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
}

fn read_stored_hash(header_path: &Path) -> Option<String> {
    let text = fs::read_to_string(header_path).ok()?;
    let first = text.lines().next()?;
    first
        .strip_prefix(HASH_TAG)
        .map(|hash| hash.trim().to_string())
}

#[derive(Default)]
struct ParsedArgs {
    options: BTreeMap<String, String>,
    flags: Vec<String>,
    positionals: Vec<String>,
}

fn parse_args(args: &[String]) -> ParsedArgs {
    let mut parsed = ParsedArgs::default();
    let mut index = 0usize;
    while index < args.len() {
        let arg = &args[index];
        match arg.as_str() {
            "--headers" | "--output" | "--registry" => {
                if let Some(value) = args.get(index + 1) {
                    parsed.options.insert(arg.clone(), value.clone());
                    index += 2;
                } else {
                    parsed.options.insert(arg.clone(), String::new());
                    index += 1;
                }
            }
            "--stdin" => {
                parsed.flags.push(arg.clone());
                index += 1;
            }
            _ => {
                parsed.positionals.push(arg.clone());
                index += 1;
            }
        }
    }
    parsed
}

fn required_opt<'a>(options: &'a BTreeMap<String, String>, name: &str) -> Result<&'a str, String> {
    options
        .get(name)
        .filter(|value| !value.is_empty())
        .map(String::as_str)
        .ok_or_else(|| format!("{name} is required"))
}

fn diagnostic_json(
    line: usize,
    col: usize,
    end_line: usize,
    end_col: usize,
    message: &str,
    severity: &str,
) -> String {
    format!(
        "{{\"line\":{},\"col\":{},\"endLine\":{},\"endCol\":{},\"message\":\"{}\",\"severity\":\"{}\"}}",
        line,
        col,
        end_line,
        end_col,
        json_escape(message),
        severity
    )
}

fn token_len(token: &crate::lexer::Token) -> usize {
    match token.ty {
        TokenType::Eof => 1,
        _ => token.text().chars().count(),
    }
}

fn file_name(path: &Path) -> String {
    path.file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("")
        .to_string()
}

fn print_usage() {
    eprintln!(
        "Usage:\n  cui scan --headers <dir> --output <registry.json>\n  cui build --registry <registry.json> --output <dir> <file.cui>...\n  cui run --headers <dir> --output <dir> <file.cui>...\n  cui redirect --output <dir> <file.cui>...\n  cui check [--registry <registry.json>] [--stdin] <file.cui>"
    );
}
