"""
main.py - CLI entry point for the .cui precompiler.

Commands:
  scan    Scan C++ headers for @cui-* annotations → registry.json
  build   Compile .cui files → .gen.h + .gen.cpp  (reads registry.json)
  run     scan + build in one step

Usage:
  python main.py scan   --headers <dir> --output <registry.json>
  python main.py build  --registry <registry.json> --output <dir> <file.cui> ...
  python main.py run    --headers <dir> --output <dir> <file.cui> ...
"""

from __future__ import annotations
import argparse
import sys
from pathlib import Path

# Add the tools/cui directory to path so sibling imports work
sys.path.insert(0, str(Path(__file__).parent))

from scanner   import Scanner, registry_to_json, registry_from_json
from lexer     import Lexer
from parser    import parse
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

    errors = validate(ast, registry)
    if errors:
        for err in errors:
            print(f"[cui] {cui_path.name}: {err}", file=sys.stderr)
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

    args = parser.parse_args()
    {"scan": cmd_scan, "build": cmd_build, "run": cmd_run}[args.command](args)


if __name__ == "__main__":
    main()
