# UI DSL - Requirements and Implementation Plan

**Target Version:** 0.2
**Status:** 🚧 In Progress

## 0. Changelog

### 0.2 (In Progress) 🚧

- Language surface refactored into a SwiftUI-inspired style.
- `body { ... }` is no longer mandatory: the view body is the direct content of the `view` block.
- Removed `let / ref / state` qualifiers in parameter lists. Replaced by property wrappers: `@Observed`, `@Snapshot`, `@State`, and `@Binding`.
- String interpolation `"\(expr)"` added. Concatenation `+` still tolerated.
- Property access for getters: `window.averageFPS` resolves `Get` + lower-camelCase to `Window::GetAverageFPS()`.
- Modifier order: children first, chained modifiers after.
- `func name(args) -> T = expression` for mono-expression helpers. `@cui-cpp` remains as an escape hatch.
- Namespace calls in dot notation: `Profiler.averageTime(...)` (`::` is still accepted).
- Numerical literals: pixels by default, `.px` suffix no longer needed.
- Dotted enum cases: `.topLeft` when the type is known from context.
- **Explicit capture modes for reactive expressions**: `once expr` captures the value only once at `Build()` time, `always expr` forces re-evaluation on every `Update()`, default is reactive (re-evaluates only when a dependency changes). Use `always` when calling external functions (e.g. `Profiler`) that read global state and are not annotated `@cui-volatile`.
- **Tag `@cui-volatile`** to mark C++ functions/methods **inside the UI core system** that read time, a RNG, or global state (non-pure). The precompiler forces their re-evaluation on every tick. Never add `@cui-*` annotations to external code outside the UI system.
- **Modifier `.throttle(period)`**: caps the re-evaluation rate of a component and **all its descendants**. An internal expression can never update more often than the `.throttle` of its most restrictive ancestor. Period in seconds (e.g., `0.5` or explicit suffix `.s`, `.ms`).
- **`@Mutable` property wrapper**: non-const writable external reference (`T*`). Equivalent to `@Observed` but allows calling non-const methods. `@Observed` compiles to `const T*` (read-only, only const methods may be called).

Runtime/pipeline sections (§2, §7, §9, §10, §12, §13, §14, §16-§19, §21, §23, §24, §26) remain as is: only the surface language changes.

### 0.1 (Released) ✅

Initial version. Surface inspired by SwiftUI / Kotlin DSL with `let / ref / state` qualifiers and `body { }` block.

---

## 1. Objective

The goal is to define a declarative DSL to build the UI more simply than in C++, while maintaining:
- The project's current rendering structure.
- The existing dirty tracking system.
- Existing components.
- The ability to add new components without rewriting the language.

The DSL is not intended to replace the current UI engine but to compile down to it.

## 2. Constraints of Existing C++ Code

The DSL must leverage existing elements:
- `UI::View` creates its tree once via `Build()`.
- `UI::ComponentBase` already manages multiple levels of dirty state.
- `UI::TextContent` knows how to compose static text, values, functions, and methods.
- `UI::BindValue`, `UI::BindFunc`, and `UI::BindMethod` know how to detect changes and cache the last result.
- `UI::CreateText`, `UI::CreateLabel`, `UI::CreateComponent`, and existing factories must remain usable.

The DSL must use this logic, not bypass it.

## 3. Design Principles

1. Declarative first.
2. A view is described as a function of its data.
3. A display only changes if a dependency has changed.
4. Static values are snapshotted once.
5. Dynamic values are tracked via comparison.
6. Objects bound in methods are part of the dependencies.
7. Future components must be addable without changing the base grammar.
8. Generated code must remain readable and debuggable.
9. Language surface should resemble SwiftUI: a view is a struct whose body is the direct UI tree, reactive dependencies are declared via property wrappers (`@Observed`, `@Mutable`, `@Snapshot`, `@State`), modifiers are chained after the children block, text is composed via interpolation `"\(expr)"`.

## 4. Target Format

The DSL must be able to describe:
- A `view` with its property wrappers (reactive dependencies, snapshots, local state).
- Nested child components.
- Static values (literals, local `let`, `@Snapshot`).
- Dynamic values (`@Observed`, `@Mutable`, `@Binding`, `@State`).
- Function calls (free or via namespace).
- Object method calls (property form or explicit call).
- Chained modifiers after the children block.
- String interpolation `"\(expr)"`.
- Mono-expression helpers `func name(args) -> T = expr`.
- Conditional blocks and lists/loops (planned for later).

The target format follows SwiftUI conventions:
- No mandatory `body` block: the view body is the direct content of `view { }`.
- `@Observed`, `@Mutable`, `@Snapshot`, `@State`, `@Binding` property wrappers declare dependencies at the top of the view.
- Nested components contain their children between `{ }`, followed by modifiers chained via `.modifier(...)`.
- External data is never guessed: it arrives via an explicit wrapper.

## 5. Minimal Grammar

Grammar 0.2 covers at least:

```txt
file        := import* view_decl+

import      := "import" string_literal

view_decl   := "view" IDENT view_block
view_block  := "{" view_stmt* "}"
view_stmt   := wrapped_field | let_decl | var_decl | func_decl | cpp_block | element

wrapped_field := wrapper "var" IDENT ":" type [ "=" expr ]
wrapper       := "@Observed" | "@Mutable" | "@Snapshot" | "@State" | "@Binding"

let_decl    := "let" IDENT ("=" expr | ":" type "=" expr)
var_decl    := "var" IDENT ("=" expr | ":" type "=" expr)

func_decl   := "func" IDENT "(" param_list? ")" "->" type func_body
func_body   := "=" expr                          // single-expression (simple calculations)
             | "{" "return" expr ";"? "}"         // block form (C++ style, for clarity)
param_list  := param ("," param)*
param       := IDENT ":" type [ "=" expr ]

cpp_block   := "@cui-cpp" "{" /* C++ verbatim */ "}"

element     := IDENT call_args? children_block? modifier_chain?
call_args   := "(" arg_list? ")"
arg_list    := arg ("," arg)*
arg         := [ IDENT ":" ] expr
children_block := "{" element* "}"
modifier_chain := ("." IDENT call_args)*

expr        := capture_expr
            |  interp_string
            |  number
            |  duration_literal
            |  bool_literal
            |  enum_case
            |  identifier
            |  property_access
            |  call_expr
            |  binary_expr
            |  paren_expr

capture_expr := ("once" | "always") expr

interp_string := '"' (text_seg | "\(" expr ")")* '"'
enum_case     := "." IDENT
property_access := expr "." IDENT
call_expr   := (expr | namespace_path) call_args
namespace_path := IDENT ("." IDENT | "::" IDENT)*
binary_expr := expr ("+" | "-" | "*" | "/") expr

duration_literal := number ("." ("s" | "ms" | "us" | "ns"))?
```

*Detailed notes omitted for brevity but preserved in implementation.*

---

## 6. Data Models

### 6.1 `@Snapshot` - Static Value ✅

A `@Snapshot` value is captured once during view construction. Used for literal text passed as a parameter or fixed values.

### 6.2 `@Observed` - Read-Only External Reference ✅

An `@Observed` value is an external pointer tracked in read-only mode (`const T*`). If `==` indicates a change, the view becomes dirty. Only `const` methods may be called on an `@Observed` field; the view cannot mutate the object.

### 6.3 `@Mutable` - Writable External Reference ✅

An `@Mutable` value is a non-const external pointer (`T*`). Semantics mirror `@Observed` for dirty tracking, but the view may also call non-const methods on the object. Use `@Mutable` only when the view genuinely needs to write back to the external object.

### 6.4 `@State` - Local View State 🚧

A `@State` field belongs to the view. It is initialized inline and persists between updates. Writing to it marks the subtree as dirty.

### 6.5 `@Binding` - Mutable External Reference 📅 (v0.3)

A `@Binding` is an external reference that is both tracked and modifiable. Primarily used to pass parent `@State` to children.

---

## 15. Implementation Plan

### Phase 0 - Precompilation System ✅
- Define detection of components, modifiers, and exposed functions.
- Choose minimal C++ annotations.
- Define generated files.

### Phase 1 - Spec and Registry ✅
- Finalize 1.0 syntax.
- Describe generated component registry.

### Phase 2 - AST ✅
- Define DSL nodes for views, components, expressions, and modifiers.

### Phase 3 - Lexer / Parser ✅
- Build the parser and provide readable error messages.

### Phase 4 - Semantic Validation 🚧
- Type checking, argument verification, and volatile flag propagation.

### Phase 5 - C++ Generation 🚧
- Convert AST to C++ code calling the existing UI API.

### Phase 6 - Build Integration 🚧
- Integrate into Makefile to regenerate only modified files.

---

## 16. Acceptance Criteria

The project is considered correct if:
- A DSL view compiles to C++.
- It can use all existing components.
- A modified `@Observed` or `@Mutable` value updates the display.
- Static values cause no unnecessary re-evaluations.
- Chained modifiers work correctly.
