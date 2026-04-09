#version 430 core
layout (location = 0) in vec2 aPos;

out vec2 v_TexCoord;

uniform vec2 offset;
uniform vec2 scale;
uniform vec2 containerSize;

void main() {
    // Position in screen space
    gl_Position = vec4(
        (aPos.x * scale.x + offset.x) / containerSize.x * 2.0 - 1.0,
        1.0 - (aPos.y * scale.y + offset.y) / containerSize.y * 2.0,
        0.0,
        1.0
    );

    // UV coordinates (0-1 range)
    v_TexCoord = aPos;
}
