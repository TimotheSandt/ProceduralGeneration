#version 430 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D textureSampler;
uniform vec2 scrollOffset;
uniform vec2 contentSize;
uniform vec2 scale;    // Visible size
uniform vec4 color;    // Background/tint color

void main() {
    // Calculate UV with scroll offset
    // scrollOffset is in pixels, convert to UV space
    vec2 scrollUV = scrollOffset / contentSize;

    // Scale UV to show visible portion of content
    vec2 visibleRatio = scale / contentSize;
    vec2 scrolledUV = v_TexCoord * visibleRatio + scrollUV;

    // Clamp to avoid showing content outside FBO
    if (scrolledUV.x < 0.0 || scrolledUV.x > 1.0 ||
        scrolledUV.y < 0.0 || scrolledUV.y > 1.0) {
        // Outside content area - use background color or discard
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Sample the FBO texture
    vec4 texColor = texture(textureSampler, scrolledUV);

    // Blend with container color if needed
    if (texColor.a < 0.01) {
        FragColor = color; // Show container background where FBO is transparent
    } else {
        FragColor = texColor;
    }
}
