use crate::json::{JsonValue, json_escape};
use std::collections::BTreeMap;

#[derive(Clone, Debug, Default)]
pub struct ModifierInfo {
    pub dsl_name: String,
    pub cpp_method: String,
    pub param_type: String,
    pub param_name: String,
}

#[derive(Clone, Debug, Default)]
pub struct ComponentInfo {
    pub dsl_name: String,
    pub cpp_class: String,
    pub factory: String,
    pub accepts_children: bool,
    pub content_model: String,
    pub modifiers: BTreeMap<String, ModifierInfo>,
    pub include_path: String,
}

#[derive(Clone, Debug, Default)]
pub struct ParamInfo {
    pub name: String,
    pub type_name: String,
}

#[derive(Clone, Debug, Default)]
pub struct FunctionInfo {
    pub dsl_name: String,
    pub cpp_qualified: String,
    pub return_type: String,
    pub params: Vec<ParamInfo>,
    pub is_instance: bool,
    pub is_getter: bool,
    pub is_volatile: bool,
    pub owner_class: String,
    pub include_path: String,
}

#[derive(Clone, Debug, Default)]
pub struct EnumValueInfo {
    pub dsl_name: String,
    pub cpp_name: String,
}

#[derive(Clone, Debug, Default)]
pub struct EnumInfo {
    pub dsl_name: String,
    pub cpp_type: String,
    pub values: Vec<EnumValueInfo>,
    pub include_path: String,
}

#[derive(Clone, Debug, Default)]
pub struct Registry {
    pub components: BTreeMap<String, ComponentInfo>,
    pub functions: BTreeMap<String, FunctionInfo>,
    pub enums: BTreeMap<String, EnumInfo>,
}

pub fn registry_to_json(registry: &Registry) -> String {
    let mut out = String::new();
    out.push_str("{\n");
    out.push_str("  \"components\": ");
    write_components(&mut out, registry, 2);
    out.push_str(",\n  \"functions\": ");
    write_functions(&mut out, registry, 2);
    out.push_str(",\n  \"enums\": ");
    write_enums(&mut out, registry, 2);
    out.push_str("\n}\n");
    out
}

pub fn registry_from_json(text: &str) -> Result<Registry, String> {
    let value = crate::json::parse_json(text)?;
    let root = value
        .as_object()
        .ok_or_else(|| "registry root must be a JSON object".to_string())?;
    let mut registry = Registry::default();

    if let Some(JsonValue::Object(components)) = root.get("components") {
        for (key, value) in components {
            let obj = value
                .as_object()
                .ok_or_else(|| format!("component {key} must be an object"))?;
            let mut modifiers = BTreeMap::new();
            if let Some(JsonValue::Object(mods)) = obj.get("modifiers") {
                for (mod_key, mod_value) in mods {
                    let mod_obj = mod_value
                        .as_object()
                        .ok_or_else(|| format!("modifier {mod_key} must be an object"))?;
                    modifiers.insert(
                        mod_key.clone(),
                        ModifierInfo {
                            dsl_name: get_string(mod_obj, "dsl_name")
                                .unwrap_or_else(|| mod_key.clone()),
                            cpp_method: get_string(mod_obj, "cpp_method").unwrap_or_default(),
                            param_type: get_string(mod_obj, "param_type")
                                .unwrap_or_else(|| "void".to_string()),
                            param_name: get_string(mod_obj, "param_name")
                                .unwrap_or_else(|| "value".to_string()),
                        },
                    );
                }
            }
            registry.components.insert(
                key.clone(),
                ComponentInfo {
                    dsl_name: get_string(obj, "dsl_name").unwrap_or_else(|| key.clone()),
                    cpp_class: get_string(obj, "cpp_class").unwrap_or_default(),
                    factory: get_string(obj, "factory").unwrap_or_default(),
                    accepts_children: get_bool(obj, "accepts_children").unwrap_or(false),
                    content_model: get_string(obj, "content_model")
                        .unwrap_or_else(|| "none".to_string()),
                    modifiers,
                    include_path: get_string(obj, "include_path").unwrap_or_default(),
                },
            );
        }
    }

    if let Some(JsonValue::Object(functions)) = root.get("functions") {
        for (key, value) in functions {
            let obj = value
                .as_object()
                .ok_or_else(|| format!("function {key} must be an object"))?;
            let mut params = Vec::new();
            if let Some(JsonValue::Array(items)) = obj.get("params") {
                for item in items {
                    let param_obj = item
                        .as_object()
                        .ok_or_else(|| format!("function {key} has a non-object param"))?;
                    params.push(ParamInfo {
                        name: get_string(param_obj, "name").unwrap_or_default(),
                        type_name: get_string(param_obj, "type_")
                            .or_else(|| get_string(param_obj, "type_name"))
                            .unwrap_or_default(),
                    });
                }
            }
            registry.functions.insert(
                key.clone(),
                FunctionInfo {
                    dsl_name: get_string(obj, "dsl_name").unwrap_or_else(|| key.clone()),
                    cpp_qualified: get_string(obj, "cpp_qualified").unwrap_or_default(),
                    return_type: get_string(obj, "return_type").unwrap_or_default(),
                    params,
                    is_instance: get_bool(obj, "is_instance").unwrap_or(false),
                    is_getter: get_bool(obj, "is_getter").unwrap_or(false),
                    is_volatile: get_bool(obj, "is_volatile").unwrap_or(false),
                    owner_class: get_string(obj, "owner_class").unwrap_or_default(),
                    include_path: get_string(obj, "include_path").unwrap_or_default(),
                },
            );
        }
    }

    if let Some(JsonValue::Object(enums)) = root.get("enums") {
        for (key, value) in enums {
            let obj = value
                .as_object()
                .ok_or_else(|| format!("enum {key} must be an object"))?;
            let mut values = Vec::new();
            if let Some(JsonValue::Array(items)) = obj.get("values") {
                for item in items {
                    let value_obj = item
                        .as_object()
                        .ok_or_else(|| format!("enum {key} has a non-object value"))?;
                    values.push(EnumValueInfo {
                        dsl_name: get_string(value_obj, "dsl_name").unwrap_or_default(),
                        cpp_name: get_string(value_obj, "cpp_name").unwrap_or_default(),
                    });
                }
            }
            registry.enums.insert(
                key.clone(),
                EnumInfo {
                    dsl_name: get_string(obj, "dsl_name").unwrap_or_else(|| key.clone()),
                    cpp_type: get_string(obj, "cpp_type").unwrap_or_default(),
                    values,
                    include_path: get_string(obj, "include_path").unwrap_or_default(),
                },
            );
        }
    }

    Ok(registry)
}

fn get_string(obj: &BTreeMap<String, JsonValue>, key: &str) -> Option<String> {
    obj.get(key).and_then(JsonValue::as_str).map(str::to_string)
}

fn get_bool(obj: &BTreeMap<String, JsonValue>, key: &str) -> Option<bool> {
    obj.get(key).and_then(JsonValue::as_bool)
}

fn indent(out: &mut String, spaces: usize) {
    out.push_str(&" ".repeat(spaces));
}

fn write_components(out: &mut String, registry: &Registry, level: usize) {
    out.push_str("{");
    if !registry.components.is_empty() {
        out.push('\n');
    }
    let len = registry.components.len();
    for (index, (name, comp)) in registry.components.iter().enumerate() {
        indent(out, level + 2);
        out.push('"');
        out.push_str(&json_escape(name));
        out.push_str("\": {\n");
        write_string_field(out, level + 4, "dsl_name", &comp.dsl_name, true);
        write_string_field(out, level + 4, "cpp_class", &comp.cpp_class, true);
        write_string_field(out, level + 4, "factory", &comp.factory, true);
        write_bool_field(
            out,
            level + 4,
            "accepts_children",
            comp.accepts_children,
            true,
        );
        write_string_field(out, level + 4, "content_model", &comp.content_model, true);
        indent(out, level + 4);
        out.push_str("\"modifiers\": ");
        write_modifiers(out, &comp.modifiers, level + 4);
        out.push_str(",\n");
        write_string_field(out, level + 4, "include_path", &comp.include_path, false);
        indent(out, level + 2);
        out.push('}');
        if index + 1 != len {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push('}');
}

fn write_modifiers(out: &mut String, modifiers: &BTreeMap<String, ModifierInfo>, level: usize) {
    out.push('{');
    if !modifiers.is_empty() {
        out.push('\n');
    }
    let len = modifiers.len();
    for (index, (name, modifier)) in modifiers.iter().enumerate() {
        indent(out, level + 2);
        out.push('"');
        out.push_str(&json_escape(name));
        out.push_str("\": {\n");
        write_string_field(out, level + 4, "dsl_name", &modifier.dsl_name, true);
        write_string_field(out, level + 4, "cpp_method", &modifier.cpp_method, true);
        write_string_field(out, level + 4, "param_type", &modifier.param_type, true);
        write_string_field(out, level + 4, "param_name", &modifier.param_name, false);
        indent(out, level + 2);
        out.push('}');
        if index + 1 != len {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push('}');
}

fn write_functions(out: &mut String, registry: &Registry, level: usize) {
    out.push_str("{");
    if !registry.functions.is_empty() {
        out.push('\n');
    }
    let len = registry.functions.len();
    for (index, (name, func)) in registry.functions.iter().enumerate() {
        indent(out, level + 2);
        out.push('"');
        out.push_str(&json_escape(name));
        out.push_str("\": {\n");
        write_string_field(out, level + 4, "dsl_name", &func.dsl_name, true);
        write_string_field(out, level + 4, "cpp_qualified", &func.cpp_qualified, true);
        write_string_field(out, level + 4, "return_type", &func.return_type, true);
        indent(out, level + 4);
        out.push_str("\"params\": ");
        write_params(out, &func.params, level + 4);
        out.push_str(",\n");
        write_bool_field(out, level + 4, "is_instance", func.is_instance, true);
        write_bool_field(out, level + 4, "is_getter", func.is_getter, true);
        write_bool_field(out, level + 4, "is_volatile", func.is_volatile, true);
        write_string_field(out, level + 4, "owner_class", &func.owner_class, true);
        write_string_field(out, level + 4, "include_path", &func.include_path, false);
        indent(out, level + 2);
        out.push('}');
        if index + 1 != len {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push('}');
}

fn write_params(out: &mut String, params: &[ParamInfo], level: usize) {
    out.push('[');
    if !params.is_empty() {
        out.push('\n');
    }
    for (index, param) in params.iter().enumerate() {
        indent(out, level + 2);
        out.push_str("{\n");
        write_string_field(out, level + 4, "name", &param.name, true);
        write_string_field(out, level + 4, "type_", &param.type_name, false);
        indent(out, level + 2);
        out.push('}');
        if index + 1 != params.len() {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push(']');
}

fn write_enums(out: &mut String, registry: &Registry, level: usize) {
    out.push_str("{");
    if !registry.enums.is_empty() {
        out.push('\n');
    }
    let len = registry.enums.len();
    for (index, (name, enum_info)) in registry.enums.iter().enumerate() {
        indent(out, level + 2);
        out.push('"');
        out.push_str(&json_escape(name));
        out.push_str("\": {\n");
        write_string_field(out, level + 4, "dsl_name", &enum_info.dsl_name, true);
        write_string_field(out, level + 4, "cpp_type", &enum_info.cpp_type, true);
        indent(out, level + 4);
        out.push_str("\"values\": ");
        write_enum_values(out, &enum_info.values, level + 4);
        out.push_str(",\n");
        write_string_field(
            out,
            level + 4,
            "include_path",
            &enum_info.include_path,
            false,
        );
        indent(out, level + 2);
        out.push('}');
        if index + 1 != len {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push('}');
}

fn write_enum_values(out: &mut String, values: &[EnumValueInfo], level: usize) {
    out.push('[');
    if !values.is_empty() {
        out.push('\n');
    }
    for (index, value) in values.iter().enumerate() {
        indent(out, level + 2);
        out.push_str("{\n");
        write_string_field(out, level + 4, "dsl_name", &value.dsl_name, true);
        write_string_field(out, level + 4, "cpp_name", &value.cpp_name, false);
        indent(out, level + 2);
        out.push('}');
        if index + 1 != values.len() {
            out.push(',');
        }
        out.push('\n');
    }
    indent(out, level);
    out.push(']');
}

fn write_string_field(out: &mut String, level: usize, key: &str, value: &str, comma: bool) {
    indent(out, level);
    out.push('"');
    out.push_str(key);
    out.push_str("\": \"");
    out.push_str(&json_escape(value));
    out.push('"');
    if comma {
        out.push(',');
    }
    out.push('\n');
}

fn write_bool_field(out: &mut String, level: usize, key: &str, value: bool, comma: bool) {
    indent(out, level);
    out.push('"');
    out.push_str(key);
    out.push_str("\": ");
    out.push_str(if value { "true" } else { "false" });
    if comma {
        out.push(',');
    }
    out.push('\n');
}
