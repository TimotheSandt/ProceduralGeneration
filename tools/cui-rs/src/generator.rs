use crate::ast::{
    Element, Expr, FileAst, FuncDecl, ImportDecl, Modifier, StringPart, ViewDecl, ViewField,
    WrappedField,
};
use crate::registry::Registry;
use std::collections::{BTreeMap, BTreeSet};

pub struct Generator<'a> {
    registry: &'a Registry,
    source_file: String,
}

impl<'a> Generator<'a> {
    pub fn new(registry: &'a Registry, source_file: impl Into<String>) -> Self {
        Self {
            registry,
            source_file: source_file.into(),
        }
    }

    pub fn generate(&self, ast: &FileAst) -> Result<(String, String), String> {
        let mut header_lines = Vec::new();
        let mut impl_lines = Vec::new();
        for view in &ast.views {
            let (header, implementation) = self.generate_view(view, &ast.imports)?;
            header_lines.extend(header);
            impl_lines.extend(implementation);
        }
        Ok((header_lines.join("\n"), impl_lines.join("\n")))
    }

    fn generate_view(
        &self,
        view: &ViewDecl,
        imports: &[ImportDecl],
    ) -> Result<(Vec<String>, Vec<String>), String> {
        let observed = wrapped_fields(view, "@Observed");
        let mutable_fields = wrapped_fields(view, "@Mutable");
        let snapshot = wrapped_fields(view, "@Snapshot");
        let states = wrapped_fields(view, "@State");
        let lets = let_fields(view);
        let funcs = func_fields(view);
        let cpp_blocks = cpp_fields(view);

        let mut includes = BTreeSet::from([
            "UI/View.h".to_string(),
            "UI/Utils/TextContent.h".to_string(),
        ]);
        for field in observed
            .iter()
            .chain(mutable_fields.iter())
            .chain(snapshot.iter())
        {
            includes.extend(Self::includes_for_type(&field.type_name));
        }
        for function in &funcs {
            includes.extend(Self::includes_for_type(&function.return_type));
        }
        includes.extend(self.includes_for_element(&view.root));
        includes.extend(imports.iter().map(|import| import.path.clone()));

        let mut header = Vec::new();
        header.push(format!(
            "// AUTO-GENERATED - do not edit. Source: {}",
            self.source_file
        ));
        header.push("#pragma once".to_string());
        header.push(String::new());
        for include in includes {
            header.push(format!("#include \"{include}\""));
        }
        header.push(String::new());
        header.push("namespace ui::generated".to_string());
        header.push("{".to_string());
        header.push(String::new());
        header.push(format!("class {} : public UI::View", view.name));
        header.push("{".to_string());
        header.push("public:".to_string());
        let ctor_params = self.ctor_params(&observed, &mutable_fields, &snapshot, true);
        header.push(format!("    explicit {}({ctor_params});", view.name));
        header.push(String::new());
        header.push("protected:".to_string());
        header.push("    void Build() override;".to_string());
        header.push(String::new());
        header.push("private:".to_string());
        for field in &observed {
            header.push(format!(
                "    const {}* {} = nullptr;",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name
            ));
        }
        for field in &mutable_fields {
            header.push(format!(
                "    {}* {} = nullptr;",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name
            ));
        }
        for field in &snapshot {
            header.push(format!(
                "    {} {}{{}};",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name
            ));
        }
        for field in &states {
            let cpp_type = Self::dsl_to_cpp_type(&field.type_name);
            let init = field
                .default
                .as_ref()
                .map(|expr| format!(" = {}", GenContext::empty(self.registry).gen_expr(expr)))
                .unwrap_or_default();
            let setter = format!("Set{}{}", upper_first(&field.name), &field.name[1..]);
            header.push(format!("    {cpp_type} {}{init};", field.name));
            header.push(format!(
                "    void {setter}({cpp_type} v) {{ {} = v; MarkFullDirty(); }}",
                field.name
            ));
        }
        for function in &funcs {
            header.push(format!("    {};", self.gen_func_sig(function, true, "")));
        }
        for block in &cpp_blocks {
            header.push("    // @cui-cpp block".to_string());
            header.push(format!("    {}", block.content));
        }
        header.push("};".to_string());
        header.push(String::new());
        header.push(format!(
            "std::shared_ptr<{}> Create{}(",
            view.name, view.name
        ));
        header.push(format!("    {ctor_params});"));
        header.push(String::new());
        header.push("} // namespace ui::generated".to_string());
        header.push(String::new());
        header.push("namespace UI".to_string());
        header.push("{".to_string());
        header.push(format!(
            "using {} = ui::generated::{};",
            view.name, view.name
        ));
        header.push(format!("using ui::generated::Create{};", view.name));
        header.push("} // namespace UI".to_string());

        let mut cpp = Vec::new();
        cpp.push(format!(
            "// AUTO-GENERATED - do not edit. Source: {}",
            self.source_file
        ));
        cpp.push(format!("#include \"{}.gen.h\"", view.name));
        cpp.push(String::new());
        cpp.push("namespace ui::generated".to_string());
        cpp.push("{".to_string());
        cpp.push(String::new());
        let ctor_params_nodef = self.ctor_params(&observed, &mutable_fields, &snapshot, false);
        let ctor_init = self.ctor_init(&observed, &mutable_fields, &snapshot);
        cpp.push(format!("{}::{}({ctor_params_nodef})", view.name, view.name));
        cpp.push(format!("    : UI::View(bounds, false){ctor_init}"));
        cpp.push("{".to_string());
        cpp.push("    DoSetIdentifierKind(UI::IdentifierKind::TRANSPARENT);".to_string());
        cpp.push("}".to_string());
        cpp.push(String::new());
        cpp.push(format!("void {}::Build()", view.name));
        cpp.push("{".to_string());
        for let_decl in &lets {
            let mut cpp_type = let_decl
                .type_name
                .as_deref()
                .map(Self::dsl_to_cpp_type)
                .unwrap_or_else(|| "auto".to_string());
            if cpp_type == "auto" {
                cpp_type = "constexpr auto".to_string();
            } else if matches!(cpp_type.as_str(), "float" | "double" | "int") {
                cpp_type = format!("constexpr {cpp_type}");
            }
            let value = GenContext::empty(self.registry).gen_expr(&let_decl.value);
            let suffix = if cpp_type.ends_with("float") && value.contains('.') {
                "f"
            } else {
                ""
            };
            cpp.push(format!(
                "    {cpp_type} {} = {value}{suffix};",
                let_decl.name
            ));
        }
        if !lets.is_empty() {
            cpp.push(String::new());
        }
        let root_code = self.gen_element(view, &observed, &mutable_fields, &states)?;
        for line in root_code.lines() {
            cpp.push(format!("    {line}"));
        }
        cpp.push("}".to_string());
        cpp.push(String::new());
        for function in &funcs {
            let sig = self.gen_func_sig(function, true, &format!("{}::", view.name));
            let body = GenContext::empty(self.registry).gen_expr(&function.body);
            cpp.push(sig);
            cpp.push("{".to_string());
            cpp.push(format!("    return {body};"));
            cpp.push("}".to_string());
            cpp.push(String::new());
        }
        cpp.push(format!(
            "std::shared_ptr<{}> Create{}(\n    {ctor_params_nodef})",
            view.name, view.name
        ));
        cpp.push("{".to_string());
        cpp.push(format!(
            "    return std::make_shared<{}>({});",
            view.name,
            self.factory_args(&observed, &mutable_fields, &snapshot)
        ));
        cpp.push("}".to_string());
        cpp.push(String::new());
        cpp.push("} // namespace ui::generated".to_string());

        Ok((header, cpp))
    }

    fn gen_element(
        &self,
        view: &ViewDecl,
        observed: &[WrappedField],
        mutable_fields: &[WrappedField],
        states: &[WrappedField],
    ) -> Result<String, String> {
        let context = GenContext::new(view, observed, mutable_fields, states, self.registry);
        Ok(format!("AddChild({});", context.gen_element(&view.root)))
    }

    fn ctor_params(
        &self,
        observed: &[WrappedField],
        mutable_fields: &[WrappedField],
        snapshot: &[WrappedField],
        with_defaults: bool,
    ) -> String {
        let mut params = vec![if with_defaults {
            "UI::Bounds bounds = UI::Bounds()".to_string()
        } else {
            "UI::Bounds bounds".to_string()
        }];
        for field in observed {
            params.push(format!(
                "const {}* {}{}",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name,
                if with_defaults { " = nullptr" } else { "" }
            ));
        }
        for field in mutable_fields {
            params.push(format!(
                "{}* {}{}",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name,
                if with_defaults { " = nullptr" } else { "" }
            ));
        }
        for field in snapshot {
            params.push(format!(
                "{} {}{}",
                Self::dsl_to_cpp_type(&field.type_name),
                field.name,
                if with_defaults { " = {}" } else { "" }
            ));
        }
        params.join(", ")
    }

    fn ctor_init(
        &self,
        observed: &[WrappedField],
        mutable_fields: &[WrappedField],
        snapshot: &[WrappedField],
    ) -> String {
        let inits = observed
            .iter()
            .chain(mutable_fields.iter())
            .chain(snapshot.iter())
            .map(|field| format!("{}({})", field.name, field.name))
            .collect::<Vec<_>>();
        if inits.is_empty() {
            String::new()
        } else {
            format!(", {}", inits.join(", "))
        }
    }

    fn factory_args(
        &self,
        observed: &[WrappedField],
        mutable_fields: &[WrappedField],
        snapshot: &[WrappedField],
    ) -> String {
        let mut args = vec!["bounds".to_string()];
        args.extend(
            observed
                .iter()
                .chain(mutable_fields.iter())
                .chain(snapshot.iter())
                .map(|field| field.name.clone()),
        );
        args.join(", ")
    }

    fn gen_func_sig(&self, function: &FuncDecl, static_decl: bool, qualified: &str) -> String {
        let ret = Self::dsl_to_cpp_type(&function.return_type);
        let params = function
            .params
            .iter()
            .map(|param| format!("{} {}", Self::dsl_to_cpp_type(&param.type_name), param.name))
            .collect::<Vec<_>>()
            .join(", ");
        let prefix = if static_decl && qualified.is_empty() {
            "static "
        } else {
            ""
        };
        format!("{prefix}{ret} {qualified}{}({params})", function.name)
    }

    fn dsl_to_cpp_type(type_name: &str) -> String {
        match type_name {
            "int" => "int",
            "float" => "float",
            "double" => "double",
            "bool" => "bool",
            "string" => "std::string",
            "nanoseconds" => "std::chrono::nanoseconds",
            "seconds" => "std::chrono::duration<double>",
            "Window" => "Window",
            "Profiler" => "Profiler",
            other => other,
        }
        .to_string()
    }

    fn includes_for_type(type_name: &str) -> BTreeSet<String> {
        let mut includes = BTreeSet::new();
        if type_name.contains("nanoseconds") || type_name.contains("chrono") {
            includes.insert("chrono".to_string());
        }
        if type_name.contains("Window") {
            includes.insert("Graphics/Window.h".to_string());
        }
        if type_name.contains("Profiler") {
            includes.insert("Profiler/Profiler.h".to_string());
        }
        includes
    }

    fn includes_for_element(&self, element: &Element) -> BTreeSet<String> {
        let mut includes = BTreeSet::new();
        if let Some(component) = self.registry.components.get(&element.name) {
            if !component.include_path.is_empty() {
                includes.insert(component.include_path.clone());
            }
        }
        for child in &element.children {
            includes.extend(self.includes_for_element(child));
        }
        includes
    }
}

struct GenContext<'a> {
    observed: BTreeSet<String>,
    mutable_fields: BTreeSet<String>,
    states: BTreeSet<String>,
    fields: BTreeMap<String, WrappedField>,
    registry: &'a Registry,
}

impl<'a> GenContext<'a> {
    fn new(
        view: &ViewDecl,
        observed: &[WrappedField],
        mutable_fields: &[WrappedField],
        states: &[WrappedField],
        registry: &'a Registry,
    ) -> Self {
        let mut fields = BTreeMap::new();
        for field in &view.fields {
            if let ViewField::Wrapped(field) = field {
                fields.insert(field.name.clone(), field.clone());
            }
        }
        Self {
            observed: observed.iter().map(|field| field.name.clone()).collect(),
            mutable_fields: mutable_fields
                .iter()
                .map(|field| field.name.clone())
                .collect(),
            states: states.iter().map(|field| field.name.clone()).collect(),
            fields,
            registry,
        }
    }

    fn empty(registry: &'a Registry) -> Self {
        Self {
            observed: BTreeSet::new(),
            mutable_fields: BTreeSet::new(),
            states: BTreeSet::new(),
            fields: BTreeMap::new(),
            registry,
        }
    }

    fn gen_element(&self, element: &Element) -> String {
        let component = self.registry.components.get(&element.name);
        let factory = component
            .map(|component| component.factory.clone())
            .unwrap_or_else(|| format!("UI::Create{}", element.name));
        let bounds = self.extract_frame_bounds(&element.modifiers);
        let bounds_arg = if bounds.is_empty() {
            "UI::Bounds()".to_string()
        } else {
            bounds
        };
        if component.is_some_and(|component| component.content_model == "text_content") {
            self.gen_text_element(element, &factory, &bounds_arg)
        } else {
            self.gen_container_element(element, &factory, &bounds_arg)
        }
    }

    fn gen_text_element(&self, element: &Element, factory: &str, bounds_arg: &str) -> String {
        let text_content = element
            .args
            .first()
            .map(|expr| self.gen_text_content(expr))
            .unwrap_or_else(|| "UI::TextContent()".to_string());
        let mut lines = vec![format!("{factory}({bounds_arg}, {text_content})")];
        for modifier in element
            .modifiers
            .iter()
            .filter(|modifier| modifier.name != "frame")
        {
            let setter = self.resolve_modifier(&element.name, modifier);
            let args = self.gen_mod_args(modifier);
            let last = lines.last_mut().expect("text element has a root line");
            last.push_str(&format!("\n    ->{setter}({args})"));
        }
        lines.join("\n")
    }

    fn gen_container_element(&self, element: &Element, factory: &str, bounds_arg: &str) -> String {
        if !element.children.is_empty() {
            let mut lines = vec!["[&]() {".to_string()];
            lines.push(format!("    auto _root = {factory}({bounds_arg});"));
            for modifier in element
                .modifiers
                .iter()
                .filter(|modifier| modifier.name != "frame")
            {
                let setter = self.resolve_modifier(&element.name, modifier);
                let args = self.gen_mod_args(modifier);
                lines.push(format!("    _root->{setter}({args});"));
            }
            for child in &element.children {
                lines.push(format!("    _root->AddChild({});", self.gen_element(child)));
            }
            lines.push("    return _root;".to_string());
            lines.push("}()".to_string());
            lines.join("\n")
        } else {
            let mut line = format!("{factory}({bounds_arg})");
            for modifier in element
                .modifiers
                .iter()
                .filter(|modifier| modifier.name != "frame")
            {
                let setter = self.resolve_modifier(&element.name, modifier);
                let args = self.gen_mod_args(modifier);
                line.push_str(&format!("\n    ->{setter}({args})"));
            }
            line
        }
    }

    fn gen_text_content(&self, expr: &Expr) -> String {
        let Expr::StringLit(parts) = expr else {
            return format!("UI::TextContent({})", self.gen_expr(expr));
        };
        let mut generated = Vec::new();
        for part in parts {
            match part {
                StringPart::Text(text) => {
                    if !text.is_empty() {
                        generated.push(format!("\"{}\"", escape_cpp(text)));
                    }
                }
                StringPart::Interp { capture, expr } => match capture.as_deref() {
                    Some("once") => {
                        generated.push(format!("UI::BindStatic({})", self.gen_expr(expr)))
                    }
                    Some("always") => {
                        generated.push(format!("UI::BindAlways({})", self.gen_expr_raw(expr)))
                    }
                    _ => generated.push(self.gen_binding(expr)),
                },
            }
        }
        if generated.is_empty() {
            "UI::TextContent(\"\")".to_string()
        } else {
            format!("UI::TextContent({})", generated.join(", "))
        }
    }

    fn gen_binding(&self, expr: &Expr) -> String {
        match expr {
            Expr::PropertyAccess { obj, prop } => {
                let key = format!("{obj}.{prop}");
                if let Some(function) = self.registry.functions.get(&key) {
                    if function.is_getter {
                        if self.is_tracked(obj) {
                            return format!("UI::Bind({obj}, &{})", function.cpp_qualified);
                        }
                        return format!("UI::Call(&{})", function.cpp_qualified);
                    }
                }
                if self.is_tracked(obj) {
                    let getter = infer_getter(prop);
                    if let Some(cpp_type) = self.get_field_cpp_type(obj) {
                        return format!("UI::Bind({obj}, &{cpp_type}::{getter})");
                    }
                    return format!("UI::Bind({obj}, &decltype(*{obj})::{getter})");
                }
                self.gen_expr(expr)
            }
            Expr::Call {
                callee,
                args,
                named_args: _,
            } => {
                if !callee.contains('.') {
                    let inner_args = args
                        .iter()
                        .map(|arg| self.gen_binding(arg))
                        .collect::<Vec<_>>();
                    return format!("UI::Call(&{callee}, {})", inner_args.join(", "));
                }
                let (owner, method) = split_owner_method(callee);
                if let Some(function) = self.registry.functions.get(callee) {
                    let cpp_args = args
                        .iter()
                        .map(|arg| self.gen_expr(arg))
                        .collect::<Vec<_>>();
                    if function.is_volatile {
                        return format!("UI::BindAlways({})", self.gen_expr_raw(expr));
                    }
                    if function.is_instance && self.is_tracked(owner) {
                        return format!(
                            "UI::Bind({}, &{}{})",
                            owner,
                            function.cpp_qualified,
                            optional_arg_suffix(&cpp_args)
                        );
                    }
                    return format!(
                        "UI::Call(&{}{})",
                        function.cpp_qualified,
                        optional_arg_suffix(&cpp_args)
                    );
                }
                let cpp_args = args
                    .iter()
                    .map(|arg| self.gen_expr(arg))
                    .collect::<Vec<_>>();
                let getter = infer_getter(method);
                if self.is_tracked(owner) {
                    if let Some(cpp_type) = self.get_field_cpp_type(owner) {
                        return format!(
                            "UI::Bind({}, &{}::{}{})",
                            owner,
                            cpp_type,
                            getter,
                            optional_arg_suffix(&cpp_args)
                        );
                    }
                }
                if owner.chars().next().is_some_and(char::is_uppercase) {
                    return format!(
                        "UI::Call(&{}::{}{})",
                        owner,
                        getter,
                        optional_arg_suffix(&cpp_args)
                    );
                }
                format!("{owner}.{method}({})", cpp_args.join(", "))
            }
            Expr::Identifier(name) => {
                if self.is_tracked(name) {
                    return format!("UI::Bind({name})");
                }
                if self.states.contains(name) {
                    return format!("UI::BindAlways([this]() {{ return {name}; }})");
                }
                name.clone()
            }
            _ => self.gen_expr(expr),
        }
    }

    fn gen_expr_raw(&self, expr: &Expr) -> String {
        format!("[&]() {{ return {}; }}", self.gen_expr(expr))
    }

    fn gen_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::Number(value) => format_number(*value),
            Expr::Bool(value) => {
                if *value {
                    "true".to_string()
                } else {
                    "false".to_string()
                }
            }
            Expr::Identifier(name) => name.clone(),
            Expr::EnumCase(name) => self.resolve_enum_case(name),
            Expr::PropertyAccess { obj, prop } => {
                let key = format!("{obj}.{prop}");
                if let Some(function) = self.registry.functions.get(&key) {
                    return format!("{}()", function.cpp_qualified);
                }
                if self.is_tracked(obj) {
                    return format!("{}->{}()", obj, infer_getter(prop));
                }
                format!("{obj}.{prop}")
            }
            Expr::Call {
                callee,
                args,
                named_args,
            } => self.gen_call(callee, args, named_args),
            Expr::Binary { op, left, right } => {
                format!("{} {op} {}", self.gen_expr(left), self.gen_expr(right))
            }
            Expr::Capture { expr, .. } => self.gen_expr(expr),
            Expr::StringLit(parts) => {
                if parts.iter().all(|part| matches!(part, StringPart::Text(_))) {
                    let text = parts
                        .iter()
                        .filter_map(|part| match part {
                            StringPart::Text(text) => Some(text.as_str()),
                            _ => None,
                        })
                        .collect::<String>();
                    return format!("\"{}\"", escape_cpp(&text));
                }
                self.gen_text_content_inline(parts)
            }
        }
    }

    fn gen_call(&self, callee: &str, args: &[Expr], named_args: &BTreeMap<String, Expr>) -> String {
        let mut all_args = args
            .iter()
            .map(|arg| self.gen_expr(arg))
            .collect::<Vec<_>>();
        all_args.extend(
            named_args
                .iter()
                .map(|(key, value)| format!("/* {key}= */{}", self.gen_expr(value))),
        );
        if callee.contains('.') {
            let (owner, method) = split_owner_method(callee);
            if let Some(function) = self.registry.functions.get(callee) {
                if function.is_instance {
                    if all_args.is_empty() {
                        return format!("{}()", function.cpp_qualified);
                    }
                    return format!("{}.{}({})", owner, method, all_args.join(", "));
                }
                return format!("{}({})", function.cpp_qualified, all_args.join(", "));
            }
            let getter = infer_getter(method);
            if self.is_tracked(owner) {
                return format!("{}->{}({})", owner, getter, all_args.join(", "));
            }
            if owner.chars().next().is_some_and(char::is_uppercase) {
                return format!("{}::{}({})", owner, getter, all_args.join(", "));
            }
            return format!("{}.{}({})", owner, method, all_args.join(", "));
        }
        format!("{callee}({})", all_args.join(", "))
    }

    fn gen_text_content_inline(&self, parts: &[StringPart]) -> String {
        let mut generated = Vec::new();
        for part in parts {
            match part {
                StringPart::Text(text) => {
                    if !text.is_empty() {
                        generated.push(format!("\"{}\"", escape_cpp(text)));
                    }
                }
                StringPart::Interp { expr, .. } => generated.push(self.gen_binding(expr)),
            }
        }
        format!("UI::TextContent({})", generated.join(", "))
    }

    fn extract_frame_bounds(&self, modifiers: &[Modifier]) -> String {
        modifiers
            .iter()
            .find(|modifier| modifier.name == "frame")
            .map(|modifier| self.gen_frame_bounds(modifier))
            .unwrap_or_default()
    }

    fn gen_frame_bounds(&self, modifier: &Modifier) -> String {
        let width = modifier
            .named_args
            .get("width")
            .map(|expr| self.gen_pixel_value(expr))
            .unwrap_or_else(|| "UI::Value{0.0, UI::ValueType::PIXEL}".to_string());
        let height = modifier
            .named_args
            .get("height")
            .map(|expr| self.gen_pixel_value(expr))
            .unwrap_or_else(|| "UI::Value{0.0, UI::ValueType::PIXEL}".to_string());
        let anchor = modifier
            .named_args
            .get("anchor")
            .map(|expr| self.gen_expr(expr))
            .unwrap_or_else(|| "UI::Anchor::TOP_LEFT".to_string());
        format!("UI::Bounds({width}, {height}, {anchor})")
    }

    fn gen_pixel_value(&self, expr: &Expr) -> String {
        format!(
            "UI::Value{{static_cast<double>({}), UI::ValueType::PIXEL}}",
            self.gen_expr(expr)
        )
    }

    fn gen_mod_args(&self, modifier: &Modifier) -> String {
        if modifier.name == "throttle" {
            let inner = modifier
                .args
                .first()
                .map(|expr| self.gen_expr(expr))
                .unwrap_or_else(|| "0".to_string());
            return format!("std::chrono::duration<double>({inner})");
        }
        let mut args = modifier
            .args
            .iter()
            .map(|arg| self.gen_expr(arg))
            .collect::<Vec<_>>();
        if args.len() == 1 {
            if matches!(modifier.args.first(), Some(Expr::Number(value)) if value.fract() == 0.0) {
                args[0].push_str(".0f");
            }
        }
        args.extend(
            modifier
                .named_args
                .iter()
                .map(|(key, value)| format!("/* {key}= */{}", self.gen_expr(value))),
        );
        args.join(", ")
    }

    fn resolve_modifier(&self, component_name: &str, modifier: &Modifier) -> String {
        if let Some(component) = self.registry.components.get(component_name) {
            if let Some(info) = component.modifiers.get(&modifier.name) {
                return info.cpp_method.clone();
            }
        }
        if modifier.name == "throttle" {
            return "SetThrottlePeriod".to_string();
        }
        format!("Set{}{}", upper_first(&modifier.name), &modifier.name[1..])
    }

    fn resolve_enum_case(&self, case_name: &str) -> String {
        for enum_info in self.registry.enums.values() {
            for value in &enum_info.values {
                if value.dsl_name == case_name {
                    return format!("{}::{}", enum_info.cpp_type, value.cpp_name);
                }
            }
        }
        match case_name {
            "topLeft" => "UI::Anchor::TOP_LEFT",
            "topCenter" => "UI::Anchor::TOP_CENTER",
            "topRight" => "UI::Anchor::TOP_RIGHT",
            "left" => "UI::HAlign::LEFT",
            "center" => "UI::HAlign::CENTER",
            "right" => "UI::HAlign::RIGHT",
            "scroll" => "UI::OverflowMode::SCROLL",
            "hidden" => "UI::OverflowMode::HIDDEN",
            "wrap" => "UI::OverflowMode::WRAP",
            other => other,
        }
        .to_string()
    }

    fn get_field_cpp_type(&self, field_name: &str) -> Option<String> {
        self.fields
            .get(field_name)
            .map(|field| Generator::dsl_to_cpp_type(&field.type_name))
    }

    fn is_tracked(&self, name: &str) -> bool {
        self.observed.contains(name) || self.mutable_fields.contains(name)
    }
}

fn wrapped_fields(view: &ViewDecl, wrapper: &str) -> Vec<WrappedField> {
    view.fields
        .iter()
        .filter_map(|field| match field {
            ViewField::Wrapped(field) if field.wrapper == wrapper => Some(field.clone()),
            _ => None,
        })
        .collect()
}

fn let_fields(view: &ViewDecl) -> Vec<crate::ast::LetDecl> {
    view.fields
        .iter()
        .filter_map(|field| match field {
            ViewField::Let(field) => Some(field.clone()),
            _ => None,
        })
        .collect()
}

fn func_fields(view: &ViewDecl) -> Vec<FuncDecl> {
    view.fields
        .iter()
        .filter_map(|field| match field {
            ViewField::Func(field) => Some(field.clone()),
            _ => None,
        })
        .collect()
}

fn cpp_fields(view: &ViewDecl) -> Vec<crate::ast::CppBlock> {
    view.fields
        .iter()
        .filter_map(|field| match field {
            ViewField::Cpp(field) => Some(field.clone()),
            _ => None,
        })
        .collect()
}

fn upper_first(value: &str) -> char {
    value
        .chars()
        .next()
        .map(|ch| ch.to_ascii_uppercase())
        .unwrap_or_default()
}

fn infer_getter(prop: &str) -> String {
    format!("Get{}{}", upper_first(prop), &prop[1..])
}

fn split_owner_method(callee: &str) -> (&str, &str) {
    callee.rsplit_once('.').unwrap_or(("", callee))
}

fn optional_arg_suffix(args: &[String]) -> String {
    if args.is_empty() {
        String::new()
    } else {
        format!(", {}", args.join(", "))
    }
}

fn escape_cpp(value: &str) -> String {
    value
        .replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\n', "\\n")
}

fn format_number(value: f64) -> String {
    if value == value.trunc() {
        return format!("{}", value as i64);
    }
    let mut text = value.to_string();
    if text.contains('e') || text.contains('E') {
        return text;
    }
    while text.ends_with('0') {
        text.pop();
    }
    if text.ends_with('.') {
        text.push('0');
    }
    text
}
