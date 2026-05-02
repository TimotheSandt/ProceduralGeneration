"""
main.py - CLI entry point for the .cui precompiler.

Commands:
  scan    Scan C++ headers for @cui-* annotations → registry.json
  build   Compile .cui files → .gen.h + .gen.cpp  (reads registry.json)
  run     scan + build in one step
  check   Parse + validate a .cui file; output JSON diagnostics for VS Code

Usage:
  python main.py scan   --headers <dir> --output <registry.json>
  python main.py build  --registry <registry.json> --output <dir> <file.cui> ...
  python main.py run    --headers <dir> --output <dir> <file.cui> ...
  python main.py check  --registry <registry.json> [--stdin] <file.cui>
"""

from __future__ import annotations
import argparse
import json
import sys
from pathlib import Path

# Add the tools/cui directory to path so sibling imports work
sys.path.insert(0, str(Path(__file__).parent))

from scanner   import Scanner, registry_to_json, registry_from_json
from lexer     import Lexer, LexError
from parser    import parse, ParseError
from validator import validate
from generator import Generator


def cmd_scan(args):
    headers_dir = Path(args.headers)
    if not headers_dir.is_dir():
        print(f"Error: headers dir not found: {headers_dir}", file=sys.stderr)
        sys.exit(1)

    scanner  = Scanner()
    registry = scanner.scan_directory(headers_dir)

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(registry_to_json(registry), encoding="utf-8")

    n_comp = len(registry.components)
    n_func = len(registry.functions)
    n_enum = len(registry.enums)
    print(f"[cui scan] {n_comp} component(s), {n_func} function(s), {n_enum} enum(s) -> {out_path}")


def cmd_build(args):
    reg_path = Path(args.registry)
    if not reg_path.exists():
        print(f"Error: registry not found: {reg_path}", file=sys.stderr)
        sys.exit(1)

    registry = registry_from_json(reg_path.read_text(encoding="utf-8"))
    out_dir  = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    for cui_file in args.files:
        cui_path = Path(cui_file)
        if not cui_path.exists():
            print(f"Error: file not found: {cui_path}", file=sys.stderr)
            continue
        _compile_file(cui_path, out_dir, registry)


def cmd_run(args):
    """Convenience: scan + build in one pass."""
    headers_dir = Path(args.headers)
    if not headers_dir.is_dir():
        print(f"Error: headers dir not found: {headers_dir}", file=sys.stderr)
        sys.exit(1)

    scanner  = Scanner()
    registry = scanner.scan_directory(headers_dir)

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    for cui_file in args.files:
        cui_path = Path(cui_file)
        if not cui_path.exists():
            print(f"Error: file not found: {cui_path}", file=sys.stderr)
            continue
        _compile_file(cui_path, out_dir, registry)


def _compile_file(cui_path: Path, out_dir: Path, registry):
    source = cui_path.read_text(encoding="utf-8")
    stem   = cui_path.stem  # e.g. "PerformanceView"

    try:
        lexer  = Lexer(source, str(cui_path))
        tokens = lexer.tokenize()
        ast    = parse(tokens)
    except Exception as e:
        print(f"[cui] ERROR parsing {cui_path}: {e}", file=sys.stderr)
        return

    diagnostics = validate(ast, registry)
    for d in diagnostics:
        print(f"[cui] {cui_path.name}: {d}", file=sys.stderr)
    if any(d.fatal for d in diagnostics):
        return

    gen = Generator(registry, source_file=cui_path.name)
    try:
        header, impl = gen.generate(ast)
    except Exception as e:
        print(f"[cui] ERROR generating {cui_path}: {e}", file=sys.stderr)
        return

    h_path   = out_dir / (stem + ".gen.h")
    cpp_path = out_dir / (stem + ".gen.cpp")
    h_path.write_text(header, encoding="utf-8")
    cpp_path.write_text(impl,   encoding="utf-8")
    print(f"[cui] {cui_path.name} -> {h_path.name}, {cpp_path.name}")


def cmd_check(args):
    """Parse + validate one .cui file; print a JSON array of diagnostics to stdout."""
    cui_path = Path(args.file)

    if args.stdin:
        source = sys.stdin.buffer.read().decode("utf-8")
    elif not cui_path.exists():
        print(json.dumps([{
            "line": 1, "col": 1, "endLine": 1, "endCol": 1,
            "message": f"File not found: {cui_path}",
            "severity": "error",
        }]))
        return
    else:
        source = cui_path.read_text(encoding="utf-8")

    diags = []

    # ── Lex ──────────────────────────────────────────────────────────────────
    try:
        tokens = Lexer(source, str(cui_path)).tokenize()
    except LexError as e:
        diags.append({
            "line": e.line, "col": e.col,
            "endLine": e.line, "endCol": e.col + 1,
            "message": str(e),
            "severity": "error",
        })
        print(json.dumps(diags))
        return

    # ── Parse ─────────────────────────────────────────────────────────────────
    try:
        ast = parse(tokens)
    except ParseError as e:
        tok = e.token
        diags.append({
            "line": tok.line, "col": tok.col,
            "endLine": tok.line, "endCol": tok.col + max(1, len(str(tok.value or ""))),
            "message": str(e),
            "severity": "error",
        })
        print(json.dumps(diags))
        return

    # ── Validate ──────────────────────────────────────────────────────────────
    if args.registry and Path(args.registry).exists():
        registry = registry_from_json(Path(args.registry).read_text(encoding="utf-8"))
        for d in validate(ast, registry):
            line = max(1, d.line)
            col  = max(1, d.col)
            diags.append({
                "line": line, "col": col,
                "endLine": line, "endCol": col + 1,
                "message": d.message,
                "severity": "warning" if d.is_warning else "error",
            })

    print(json.dumps(diags))


def main():
    parser = argparse.ArgumentParser(prog="cui", description=".cui DSL precompiler")
    sub = parser.add_subparsers(dest="command", required=True)

    # scan
    p_scan = sub.add_parser("scan", help="Scan C++ headers → registry.json")
    p_scan.add_argument("--headers", required=True, help="Root directory of C++ headers")
    p_scan.add_argument("--output",  required=True, help="Output path for registry.json")

    # build
    p_build = sub.add_parser("build", help="Compile .cui files → .gen.h/.gen.cpp")
    p_build.add_argument("--registry", required=True, help="Path to registry.json from scan")
    p_build.add_argument("--output",   required=True, help="Output directory for generated files")
    p_build.add_argument("files",      nargs="+",     help=".cui source files")

    # run (scan + build)
    p_run = sub.add_parser("run", help="scan + build in one step")
    p_run.add_argument("--headers", required=True, help="Root directory of C++ headers")
    p_run.add_argument("--output",  required=True, help="Output directory for generated files")
    p_run.add_argument("files",     nargs="+",     help=".cui source files")

    # check (diagnostics for VS Code)
    p_check = sub.add_parser("check", help="Parse + validate → JSON diagnostics")
    p_check.add_argument("--registry", default="", help="Path to registry.json (optional)")
    p_check.add_argument("--stdin",    action="store_true", help="Read source from stdin")
    p_check.add_argument("file",       help=".cui source file path")

    args = parser.parse_args()
    {"scan": cmd_scan, "build": cmd_build, "run": cmd_run, "check": cmd_check}[args.command](args)


if __name__ == "__main__":
    main()
