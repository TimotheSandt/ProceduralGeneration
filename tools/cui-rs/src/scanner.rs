use crate::registry::{
    ComponentInfo, EnumInfo, EnumValueInfo, FunctionInfo, ModifierInfo, ParamInfo, Registry,
};
use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Default)]
pub struct Scanner {
    registry: Registry,
    pending_component: Option<String>,
    pending_modifiers: Vec<(String, String)>,
}

impl Scanner {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn scan_directory(mut self, headers_dir: &Path) -> Result<Registry, String> {
        let mut files = Vec::new();
        collect_headers(headers_dir, &mut files)?;
        files.sort();

        for path in &files {
            self.pending_component = None;
            let text = fs::read_to_string(path)
                .map_err(|err| format!("failed to read {}: {err}", path.display()))?;
            let include_path = relative_include(headers_dir, path);
            self.scan_text(&text, &include_path, true);
        }

        for path in &files {
            self.pending_component = None;
            let text = fs::read_to_string(path)
                .map_err(|err| format!("failed to read {}: {err}", path.display()))?;
            let include_path = relative_include(headers_dir, path);
            self.scan_text(&text, &include_path, false);
        }

        self.resolve_pending_modifiers();
        Ok(self.registry)
    }

    #[allow(dead_code)]
    pub fn scan_file(mut self, path: &Path) -> Result<Registry, String> {
        let text = fs::read_to_string(path)
            .map_err(|err| format!("failed to read {}: {err}", path.display()))?;
        let include_path = path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or("")
            .to_string();
        self.pending_component = None;
        self.scan_text(&text, &include_path, true);
        self.pending_component = None;
        self.scan_text(&text, &include_path, false);
        self.resolve_pending_modifiers();
        Ok(self.registry)
    }

    fn resolve_pending_modifiers(&mut self) {
        for (base_name, derived_name) in self.pending_modifiers.clone() {
            let Some(base) = self.registry.components.get(&base_name).cloned() else {
                continue;
            };
            let Some(derived) = self.registry.components.get_mut(&derived_name) else {
                continue;
            };
            for (mod_name, mod_info) in base.modifiers {
                derived.modifiers.entry(mod_name).or_insert(mod_info);
            }
        }
    }

    fn scan_text(&mut self, text: &str, include_path: &str, components_only: bool) {
        let mut file_component_dsl = None;
        if !components_only {
            let mut file_components = Vec::new();
            for block in doxygen_blocks(text) {
                if !block.content.contains("@cui-") {
                    continue;
                }
                let tags = parse_tags(block.content);
                if tags.contains_key("component") || tags.contains_key("generate-component") {
                    let end = (block.end + 200).min(text.len());
                    if let Some(class_name) = find_class_decl(&text[block.end..end]) {
                        file_components.push(tags.get("dsl-name").cloned().unwrap_or(class_name));
                    }
                }
            }
            if file_components.len() == 1 {
                file_component_dsl = file_components.into_iter().next();
            }
        }

        for block in doxygen_blocks(text) {
            if !block.content.contains("@cui-") {
                continue;
            }
            let tags = parse_tags(block.content);
            if tags.is_empty() {
                continue;
            }
            let namespace = find_last_namespace(text, block.start);
            let class_ctx = find_last_class_context(text, block.start);
            let owner = if class_ctx.is_empty() {
                namespace.clone()
            } else {
                class_ctx
            };
            let end = (block.end + 500).min(text.len());
            let snippet = &text[block.end..end];
            self.process_block(
                &tags,
                snippet,
                &owner,
                &namespace,
                include_path,
                components_only,
                file_component_dsl.as_deref(),
            );
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn process_block(
        &mut self,
        tags: &BTreeMap<String, String>,
        decl_snippet: &str,
        owner: &str,
        namespace: &str,
        include_path: &str,
        components_only: bool,
        file_component_dsl: Option<&str>,
    ) {
        if components_only
            && !tags.contains_key("component")
            && !tags.contains_key("generate-component")
            && !tags.contains_key("alias")
        {
            return;
        }

        let ns_prefix = if namespace.is_empty() {
            String::new()
        } else {
            format!("{namespace}::")
        };

        if tags.contains_key("component") {
            if let Some(class_name) = find_class_decl(decl_snippet) {
                let dsl_name = tags
                    .get("dsl-name")
                    .cloned()
                    .unwrap_or_else(|| class_name.clone());
                if components_only {
                    self.registry.components.insert(
                        dsl_name.clone(),
                        ComponentInfo {
                            dsl_name: dsl_name.clone(),
                            cpp_class: format!("{ns_prefix}{class_name}"),
                            factory: tags
                                .get("factory")
                                .cloned()
                                .unwrap_or_else(|| format!("{ns_prefix}Create{class_name}")),
                            accepts_children: tags
                                .get("accepts-children")
                                .map(|value| {
                                    let lower = value.to_ascii_lowercase();
                                    !lower.is_empty() && lower != "false"
                                })
                                .unwrap_or(false),
                            content_model: tags
                                .get("content-model")
                                .cloned()
                                .unwrap_or_else(|| "none".to_string()),
                            modifiers: BTreeMap::new(),
                            include_path: include_path.to_string(),
                        },
                    );
                    if let Some(base) = tags.get("extends") {
                        self.pending_modifiers
                            .push((base.trim().to_string(), dsl_name.clone()));
                    }
                }
                self.pending_component = Some(dsl_name);
            }
        } else if tags.contains_key("generate-component") {
            if let Some(class_name) = find_class_decl(decl_snippet) {
                let dsl_name = class_name
                    .strip_suffix("Base")
                    .unwrap_or(&class_name)
                    .to_string();
                if components_only {
                    self.registry.components.insert(
                        dsl_name.clone(),
                        ComponentInfo {
                            dsl_name: dsl_name.clone(),
                            cpp_class: format!("{ns_prefix}{dsl_name}"),
                            factory: format!("{ns_prefix}Create{dsl_name}"),
                            accepts_children: tags
                                .get("accepts-children")
                                .map(|value| {
                                    let lower = value.to_ascii_lowercase();
                                    !lower.is_empty() && lower != "false"
                                })
                                .unwrap_or(false),
                            content_model: tags
                                .get("content-model")
                                .cloned()
                                .unwrap_or_else(|| "none".to_string()),
                            modifiers: BTreeMap::new(),
                            include_path: include_path.to_string(),
                        },
                    );
                }
                self.pending_component = Some(dsl_name);
            }
        } else if let Some(alias_of) = tags.get("alias") {
            if let Some(class_name) =
                find_class_decl(decl_snippet).or_else(|| find_using_alias(decl_snippet))
            {
                if components_only {
                    let factory = tags
                        .get("factory")
                        .cloned()
                        .unwrap_or_else(|| format!("{ns_prefix}Create{class_name}"));
                    let info = if let Some(base) = self.registry.components.get(alias_of) {
                        ComponentInfo {
                            dsl_name: class_name.clone(),
                            cpp_class: format!("{ns_prefix}{class_name}"),
                            factory,
                            accepts_children: base.accepts_children,
                            content_model: base.content_model.clone(),
                            modifiers: BTreeMap::new(),
                            include_path: include_path.to_string(),
                        }
                    } else {
                        ComponentInfo {
                            dsl_name: class_name.clone(),
                            cpp_class: format!("{ns_prefix}{class_name}"),
                            factory,
                            accepts_children: false,
                            content_model: "none".to_string(),
                            modifiers: BTreeMap::new(),
                            include_path: include_path.to_string(),
                        }
                    };
                    self.registry.components.insert(class_name.clone(), info);
                    self.pending_modifiers
                        .push((alias_of.trim().to_string(), class_name.clone()));
                }
                self.pending_component = Some(class_name);
            }
        }

        if let Some(explicit_dsl) = tags.get("modifier") {
            let Some((return_type, method_name, params_raw)) = find_method_decl(decl_snippet)
            else {
                return;
            };
            let _ = return_type;
            let dsl_names: Vec<String> = if explicit_dsl.trim().is_empty() {
                vec![method_to_dsl_name(&method_name)]
            } else {
                explicit_dsl
                    .split(',')
                    .map(|name| name.trim().to_string())
                    .filter(|name| !name.is_empty())
                    .collect()
            };
            let cpp_method = if let Some(rest) = method_name.strip_prefix("DoSet") {
                format!("Set{rest}")
            } else {
                method_name.clone()
            };
            let param_type = parse_first_param_type(&params_raw);

            if let Some(component_name) = self.pending_component.clone() {
                if let Some(component) = self.registry.components.get_mut(&component_name) {
                    for dsl_name in dsl_names {
                        component.modifiers.insert(
                            dsl_name.clone(),
                            ModifierInfo {
                                dsl_name,
                                cpp_method: cpp_method.clone(),
                                param_type: param_type.clone(),
                                param_name: "value".to_string(),
                            },
                        );
                    }
                }
            } else if let Some(component_name) = file_component_dsl {
                if let Some(component) = self.registry.components.get_mut(component_name) {
                    for dsl_name in dsl_names {
                        component
                            .modifiers
                            .entry(dsl_name.clone())
                            .or_insert(ModifierInfo {
                                dsl_name,
                                cpp_method: cpp_method.clone(),
                                param_type: param_type.clone(),
                                param_name: "value".to_string(),
                            });
                    }
                }
            } else {
                for component in self.registry.components.values_mut() {
                    for dsl_name in &dsl_names {
                        component
                            .modifiers
                            .entry(dsl_name.clone())
                            .or_insert(ModifierInfo {
                                dsl_name: dsl_name.clone(),
                                cpp_method: cpp_method.clone(),
                                param_type: param_type.clone(),
                                param_name: "value".to_string(),
                            });
                    }
                }
            }
        }

        if tags.contains_key("expose") {
            let Some((return_type, method_name, params_raw)) = find_method_decl(decl_snippet)
            else {
                return;
            };
            let is_volatile = tags.contains_key("volatile");
            let owner_class = owner.to_string();
            let cpp_qualified = if owner_class.is_empty() {
                method_name.clone()
            } else {
                format!("{owner_class}::{method_name}")
            };
            let params = parse_params(&params_raw);
            let is_getter = method_name.starts_with("Get") && params.is_empty();
            let dsl_name = tags
                .get("dsl-name")
                .filter(|value| !value.trim().is_empty())
                .cloned()
                .unwrap_or_else(|| method_to_dsl_name(&method_name));
            let is_static = decl_snippet
                .get(..decl_snippet.len().min(100))
                .is_some_and(|prefix| contains_word(prefix, "static"));
            let is_instance = !owner_class.is_empty() && !is_static;
            let key = if owner_class.is_empty() {
                dsl_name.clone()
            } else {
                format!("{owner_class}.{dsl_name}")
            };
            self.registry.functions.insert(
                key,
                FunctionInfo {
                    dsl_name,
                    cpp_qualified,
                    return_type,
                    params,
                    is_instance,
                    is_getter,
                    is_volatile,
                    owner_class,
                    include_path: include_path.to_string(),
                },
            );
        }

        if tags.contains_key("enum") {
            let Some((enum_name, values)) = find_enum_decl(decl_snippet) else {
                return;
            };
            let cpp_type = if ns_prefix.is_empty() {
                format!("UI::{enum_name}")
            } else {
                format!("{ns_prefix}{enum_name}")
            };
            self.registry.enums.insert(
                enum_name.clone(),
                EnumInfo {
                    dsl_name: enum_name,
                    cpp_type,
                    values,
                    include_path: include_path.to_string(),
                },
            );
        }
    }
}

fn collect_headers(root: &Path, out: &mut Vec<PathBuf>) -> Result<(), String> {
    for entry in
        fs::read_dir(root).map_err(|err| format!("failed to read {}: {err}", root.display()))?
    {
        let entry = entry.map_err(|err| format!("failed to read directory entry: {err}"))?;
        let path = entry.path();
        if path.is_dir() {
            collect_headers(&path, out)?;
        } else if path.extension().is_some_and(|ext| ext == "h") {
            out.push(path);
        }
    }
    Ok(())
}

fn relative_include(root: &Path, path: &Path) -> String {
    path.strip_prefix(root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

struct DoxygenBlock<'a> {
    start: usize,
    end: usize,
    content: &'a str,
}

fn doxygen_blocks(text: &str) -> Vec<DoxygenBlock<'_>> {
    let mut blocks = Vec::new();
    let mut offset = 0usize;
    while let Some(start_rel) = text[offset..].find("/**") {
        let start = offset + start_rel;
        let content_start = start + 3;
        let Some(end_rel) = text[content_start..].find("*/") else {
            break;
        };
        let content_end = content_start + end_rel;
        let end = content_end + 2;
        blocks.push(DoxygenBlock {
            start,
            end,
            content: &text[content_start..content_end],
        });
        offset = end;
    }
    blocks
}

fn parse_tags(block: &str) -> BTreeMap<String, String> {
    let mut tags = BTreeMap::new();
    let mut cleaned = String::new();
    for line in block.lines() {
        let line = line.trim().trim_start_matches('*').trim();
        cleaned.push_str(line);
        cleaned.push('\n');
    }

    let mut offset = 0usize;
    while let Some(rel) = cleaned[offset..].find("@cui-") {
        let tag_start = offset + rel + "@cui-".len();
        let mut pos = tag_start;
        while cleaned
            .as_bytes()
            .get(pos)
            .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'-' || *byte == b'_')
        {
            pos += 1;
        }
        let key = cleaned[tag_start..pos].trim().to_string();
        while cleaned
            .as_bytes()
            .get(pos)
            .is_some_and(|byte| *byte == b' ' || *byte == b'\t')
        {
            pos += 1;
        }
        let value_start = pos;
        while cleaned
            .as_bytes()
            .get(pos)
            .is_some_and(|byte| *byte != b'\n')
        {
            pos += 1;
        }
        let value = cleaned[value_start..pos].trim().to_string();
        if !key.is_empty() {
            tags.insert(key, value);
        }
        offset = pos;
    }
    tags
}

fn find_last_namespace(text: &str, before: usize) -> String {
    let limit = before.min(text.len());
    let mut idx = 0usize;
    let mut depth = 0usize;
    let mut namespaces: Vec<(String, usize)> = Vec::new();

    while idx < limit {
        if text[idx..].starts_with("namespace") && is_word_boundary(text, idx, "namespace") {
            let mut look = idx + "namespace".len();
            skip_spaces(text, &mut look);
            let name = read_ident(text, &mut look);
            skip_spaces(text, &mut look);
            if !name.is_empty() && text.as_bytes().get(look) == Some(&b'{') {
                depth += 1;
                namespaces.push((name, depth));
                idx = look + 1;
                continue;
            }
        }

        match text.as_bytes()[idx] {
            b'/' if text.as_bytes().get(idx + 1) == Some(&b'/') => {
                idx += 2;
                while idx < limit && text.as_bytes().get(idx) != Some(&b'\n') {
                    idx += 1;
                }
            }
            b'/' if text.as_bytes().get(idx + 1) == Some(&b'*') => {
                idx += 2;
                while idx + 1 < limit {
                    if text.as_bytes()[idx] == b'*' && text.as_bytes()[idx + 1] == b'/' {
                        idx += 2;
                        break;
                    }
                    idx += 1;
                }
            }
            b'"' => {
                idx += 1;
                while idx < limit {
                    if text.as_bytes()[idx] == b'\\' {
                        idx += 2;
                    } else if text.as_bytes()[idx] == b'"' {
                        idx += 1;
                        break;
                    } else {
                        idx += 1;
                    }
                }
            }
            b'{' => {
                depth += 1;
                idx += 1;
            }
            b'}' => {
                depth = depth.saturating_sub(1);
                namespaces.retain(|(_, namespace_depth)| *namespace_depth <= depth);
                idx += 1;
            }
            _ => {
                idx += 1;
            }
        }
    }

    namespaces
        .into_iter()
        .map(|(name, _)| name)
        .collect::<Vec<_>>()
        .join("::")
}

fn find_last_class_context(text: &str, before: usize) -> String {
    let mut result = String::new();
    let limit = before.min(text.len());
    let mut byte_pos = 0usize;
    for line in text[..limit].lines() {
        let trimmed = line.trim_start();
        if let Some(rest) = trimmed.strip_prefix("class ") {
            let mut idx = 0usize;
            let name = read_ident(rest, &mut idx);
            if !name.is_empty() {
                result = name;
            }
        }
        byte_pos += line.len() + 1;
        if byte_pos >= limit {
            break;
        }
    }
    result
}

fn find_class_decl(snippet: &str) -> Option<String> {
    for keyword in ["class", "struct"] {
        let mut offset = 0usize;
        while let Some(rel) = snippet[offset..].find(keyword) {
            let pos = offset + rel;
            if !is_word_boundary(snippet, pos, keyword) {
                offset = pos + keyword.len();
                continue;
            }
            let mut idx = pos + keyword.len();
            skip_spaces(snippet, &mut idx);
            let name = read_ident(snippet, &mut idx);
            if !name.is_empty() {
                return Some(name);
            }
            offset = pos + keyword.len();
        }
    }
    None
}

fn find_using_alias(snippet: &str) -> Option<String> {
    let pos = snippet.find("using")?;
    if !is_word_boundary(snippet, pos, "using") {
        return None;
    }
    let mut idx = pos + "using".len();
    skip_spaces(snippet, &mut idx);
    let name = read_ident(snippet, &mut idx);
    if name.is_empty() { None } else { Some(name) }
}

fn find_method_decl(snippet: &str) -> Option<(String, String, String)> {
    let prefixes = ["DoSet", "Set", "Get", "Is", "Has"];
    let bytes = snippet.as_bytes();
    for pos in 0..snippet.len() {
        if pos > 0 {
            let prev = bytes[pos - 1] as char;
            if prev.is_ascii_alphanumeric() || prev == '_' {
                continue;
            }
        }
        let matched = prefixes
            .iter()
            .any(|prefix| snippet[pos..].starts_with(prefix));
        if !matched {
            continue;
        }
        let mut name_end = pos;
        while bytes
            .get(name_end)
            .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'_')
        {
            name_end += 1;
        }
        let method_name = snippet[pos..name_end].to_string();
        let mut idx = name_end;
        skip_spaces(snippet, &mut idx);
        if bytes.get(idx) != Some(&b'(') {
            continue;
        }
        let params_start = idx + 1;
        let params_end = find_matching_paren(snippet, idx)?;
        let line_start = snippet[..pos]
            .rfind('\n')
            .map(|value| value + 1)
            .unwrap_or(0);
        let return_type = snippet[line_start..pos].trim().to_string();
        let params = snippet[params_start..params_end].trim().to_string();
        if !return_type.is_empty() {
            return Some((return_type, method_name, params));
        }
    }
    None
}

fn find_matching_paren(text: &str, open: usize) -> Option<usize> {
    let mut depth = 0usize;
    for (idx, ch) in text[open..].char_indices() {
        match ch {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth == 0 {
                    return Some(open + idx);
                }
            }
            _ => {}
        }
    }
    None
}

fn find_enum_decl(snippet: &str) -> Option<(String, Vec<EnumValueInfo>)> {
    let enum_pos = snippet.find("enum ")?;
    let mut idx = enum_pos + "enum ".len();
    skip_spaces(snippet, &mut idx);
    if snippet[idx..].starts_with("class") && is_word_boundary(snippet, idx, "class") {
        idx += "class".len();
    } else if snippet[idx..].starts_with("struct") && is_word_boundary(snippet, idx, "struct") {
        idx += "struct".len();
    } else {
        return None;
    }
    skip_spaces(snippet, &mut idx);
    let enum_name = read_ident(snippet, &mut idx);
    if enum_name.is_empty() {
        return None;
    }
    let brace_start = snippet[idx..].find('{').map(|rel| idx + rel)?;
    let brace_end = snippet[brace_start + 1..]
        .find('}')
        .map(|rel| brace_start + 1 + rel)?;
    let body = &snippet[brace_start + 1..brace_end];
    let mut values = Vec::new();
    for raw in body.split([',', '\n']) {
        let val = raw
            .split('=')
            .next()
            .unwrap_or("")
            .split("//")
            .next()
            .unwrap_or("")
            .trim();
        if !val.is_empty()
            && val
                .chars()
                .all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
        {
            values.push(EnumValueInfo {
                dsl_name: to_lower_camel(val),
                cpp_name: val.to_string(),
            });
        }
    }
    Some((enum_name, values))
}

fn method_to_dsl_name(method_name: &str) -> String {
    let name = method_name
        .strip_prefix("DoSet")
        .or_else(|| method_name.strip_prefix("Get"))
        .or_else(|| method_name.strip_prefix("Set"))
        .unwrap_or(method_name);
    lower_first(name)
}

fn to_lower_camel(value: &str) -> String {
    let mut parts = value.split('_');
    let mut out = parts.next().unwrap_or("").to_ascii_lowercase();
    for part in parts {
        let mut chars = part.chars();
        if let Some(first) = chars.next() {
            out.push(first.to_ascii_uppercase());
            out.push_str(&chars.as_str().to_ascii_lowercase());
        }
    }
    out
}

fn lower_first(value: &str) -> String {
    let mut chars = value.chars();
    match chars.next() {
        Some(first) => {
            let mut out = String::new();
            out.push(first.to_ascii_lowercase());
            out.push_str(chars.as_str());
            out
        }
        None => String::new(),
    }
}

fn parse_first_param_type(params_raw: &str) -> String {
    let first = params_raw.split(',').next().unwrap_or("").trim();
    if first.is_empty() {
        return "void".to_string();
    }
    let parts: Vec<&str> = first.split_whitespace().collect();
    if parts.len() > 1 {
        parts[..parts.len() - 1].join(" ")
    } else {
        first.to_string()
    }
}

fn parse_params(params_raw: &str) -> Vec<ParamInfo> {
    if params_raw.trim().is_empty() {
        return Vec::new();
    }
    let mut result = Vec::new();
    for param in params_raw.split(',') {
        let param = param.trim();
        if param.is_empty() {
            continue;
        }
        let parts: Vec<&str> = param.split_whitespace().collect();
        if parts.len() >= 2 {
            result.push(ParamInfo {
                name: parts
                    .last()
                    .unwrap_or(&"")
                    .trim_start_matches(['*', '&'])
                    .to_string(),
                type_name: parts[..parts.len() - 1].join(" "),
            });
        } else {
            result.push(ParamInfo {
                name: param.to_string(),
                type_name: param.to_string(),
            });
        }
    }
    result
}

fn contains_word(text: &str, word: &str) -> bool {
    let mut offset = 0usize;
    while let Some(rel) = text[offset..].find(word) {
        let pos = offset + rel;
        if is_word_boundary(text, pos, word) {
            return true;
        }
        offset = pos + word.len();
    }
    false
}

fn is_word_boundary(text: &str, pos: usize, word: &str) -> bool {
    let before_ok = pos == 0
        || text
            .as_bytes()
            .get(pos - 1)
            .is_none_or(|byte| !byte.is_ascii_alphanumeric() && *byte != b'_');
    let end = pos + word.len();
    let after_ok = text
        .as_bytes()
        .get(end)
        .is_none_or(|byte| !byte.is_ascii_alphanumeric() && *byte != b'_');
    before_ok && after_ok
}

fn skip_spaces(text: &str, idx: &mut usize) {
    while text
        .as_bytes()
        .get(*idx)
        .is_some_and(|byte| byte.is_ascii_whitespace())
    {
        *idx += 1;
    }
}

fn read_ident(text: &str, idx: &mut usize) -> String {
    let start = *idx;
    while text
        .as_bytes()
        .get(*idx)
        .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'_')
    {
        *idx += 1;
    }
    text[start..*idx].to_string()
}
