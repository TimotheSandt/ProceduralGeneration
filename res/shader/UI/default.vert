#version 430 core
layout (location = 0) in vec2 aPos;

uniform vec2 offset;
uniform vec2 scale;
uniform vec2 containerSize;

void main() {
    // aPos is unit quad (0-1), scale to actual size, add offset, normalize to [-1,1]
    vec2 pixelPos = aPos * scale + offset;
    vec2 normalizedPos = pixelPos / containerSize * 2.0 - 1.0;

    // Flip Y axis (OpenGL has Y up, UI has Y down)
    normalizedPos.y = -normalizedPos.y;

    gl_Position = vec4(normalizedPos, 0.0, 1.0);
}
