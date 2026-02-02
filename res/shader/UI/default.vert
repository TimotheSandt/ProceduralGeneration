#version 430 core
layout (location = 0) in vec2 aPos;

uniform vec2 offset;
uniform vec2 scale;
uniform vec2 containerSize;

void main() {
    gl_Position = vec4((aPos.x * scale.x + offset.x)/containerSize.x, (aPos.y * scale.y + offset.y)/containerSize.y, 0.0, 1.0);
}
