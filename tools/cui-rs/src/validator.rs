use crate::ast::{Element, FileAst, Modifier, ViewDecl, ViewField};
use crate::registry::{ComponentInfo, Registry};

#[derive(Clone, Debug)]
pub struct ValidationError {
    pub view: String,
    pub message: String,
    pub is_warning: bool,
    pub line: usize,
    pub col: usize,
}

impl ValidationError {
    pub fn fatal(&self) -> bool {
        !self.is_warning
    }
}

impl std::fmt::Display for ValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let tag = if self.is_warning { "WARNING" } else { "ERROR" };
        if self.line > 0 {
            write!(
                f,
                "[{}:{}:{}] {}: {}",
                self.view, self.line, self.col, tag, self.message
            )
        } else {
            write!(f, "[{}] {}: {}", self.view, tag, self.message)
        }
    }
}

pub fn validate(ast: &FileAst, registry: &Registry) -> Vec<ValidationError> {
    let mut errors = Vec::new();
    for view in &ast.views {
        validate_view(view, registry, &mut errors);
    }
    errors
}

fn validate_view(view: &ViewDecl, registry: &Registry, errors: &mut Vec<ValidationError>) {
    let mut seen = std::collections::BTreeMap::<String, String>::new();
    for field in &view.fields {
        let (name, kind, line, col) = match field {
            ViewField::Wrapped(field) => (
                field.name.clone(),
                field.wrapper.clone(),
                field.line,
                field.col,
            ),
            ViewField::Let(field) => (field.name.clone(), "let".to_string(), field.line, field.col),
            ViewField::Func(field) => (
                field.name.clone(),
                "func".to_string(),
                field.line,
                field.col,
            ),
            ViewField::Cpp(_) => continue,
        };
        if let Some(previous) = seen.get(&name) {
            errors.push(ValidationError {
                view: view.name.clone(),
                message: format!("Duplicate name '{name}' (already declared as {previous})."),
                is_warning: false,
                line,
                col,
            });
        } else {
            seen.insert(name, kind);
        }
    }

    for field in &view.fields {
        if let ViewField::Wrapped(field) = field {
            if field.wrapper == "@State" && field.type_name.is_empty() {
                errors.push(ValidationError {
                    view: view.name.clone(),
                    message: format!(
                        "@State field '{}' must have an explicit type annotation (e.g. @State var {}: int = 0).",
                        field.name, field.name
                    ),
                    is_warning: false,
                    line: field.line,
                    col: field.col,
                });
            }
        }
    }

    for field in &view.fields {
        if let ViewField::Cpp(field) = field {
            errors.push(ValidationError {
                view: view.name.clone(),
                message: "@cui-cpp is an escape hatch for UI system internals. Prefer func declarations for view-local helpers.".to_string(),
                is_warning: true,
                line: field.line,
                col: field.col,
            });
        }
    }

    validate_element(&view.root, &view.name, registry, errors);
}

fn validate_element(
    element: &Element,
    view_name: &str,
    registry: &Registry,
    errors: &mut Vec<ValidationError>,
) {
    let component = registry.components.get(&element.name);
    if let Some(component) = component {
        if !element.children.is_empty() && !component.accepts_children {
            errors.push(ValidationError {
                view: view_name.to_string(),
                message: format!(
                    "'{}' does not accept children (accepts_children=false).",
                    element.name
                ),
                is_warning: false,
                line: element.line,
                col: element.col,
            });
        }
        for modifier in &element.modifiers {
            validate_modifier(modifier, &element.name, component, view_name, errors);
        }
    } else {
        let known = registry
            .components
            .keys()
            .cloned()
            .collect::<Vec<_>>()
            .join(", ");
        errors.push(ValidationError {
            view: view_name.to_string(),
            message: format!(
                "Unknown component '{}'. Known components: {}.",
                element.name, known
            ),
            is_warning: false,
            line: element.line,
            col: element.col,
        });
    }

    for child in &element.children {
        validate_element(child, view_name, registry, errors);
    }
}

fn validate_modifier(
    modifier: &Modifier,
    component_name: &str,
    component: &ComponentInfo,
    view_name: &str,
    errors: &mut Vec<ValidationError>,
) {
    if matches!(modifier.name.as_str(), "throttle" | "frame") {
        if modifier.name == "throttle" && modifier.args.is_empty() {
            errors.push(ValidationError {
                view: view_name.to_string(),
                message: format!(
                    "Modifier '{}.throttle' requires a period argument (e.g. .throttle(0.25)).",
                    component_name
                ),
                is_warning: false,
                line: modifier.line,
                col: modifier.col,
            });
        }
        return;
    }

    let Some(info) = component.modifiers.get(&modifier.name) else {
        let known = component
            .modifiers
            .keys()
            .cloned()
            .collect::<Vec<_>>()
            .join(", ");
        errors.push(ValidationError {
            view: view_name.to_string(),
            message: format!(
                "'{}' has no modifier '{}'. Known modifiers: {}.",
                component_name, modifier.name, known
            ),
            is_warning: false,
            line: modifier.line,
            col: modifier.col,
        });
        return;
    };

    if info.param_type != "void" && modifier.args.is_empty() && modifier.named_args.is_empty() {
        errors.push(ValidationError {
            view: view_name.to_string(),
            message: format!(
                "Modifier '{}.{}' expects a {} argument but none was provided.",
                component_name, modifier.name, info.param_type
            ),
            is_warning: false,
            line: modifier.line,
            col: modifier.col,
        });
    }
}
