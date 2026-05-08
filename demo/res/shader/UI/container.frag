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
    vec2 scrollUV = scrollOffset / contentSize;

    // Scale UV to show visible portion of content
    vec2 visibleRatio = scale / contentSize;
    vec2 scrolledUV = v_TexCoord * visibleRatio + scrollUV;

    // Clamp to avoid showing content outside FBO
    if (scrolledUV.x < 0.0 || scrolledUV.x > 1.0 ||
        scrolledUV.y < 0.0 || scrolledUV.y > 1.0) {
        discard;
    }

    // UI layout uses a top-left origin while the render target texture is sampled
    // with OpenGL's bottom-left UV convention, so flip Y when reading back.
    vec2 sampleUV = vec2(scrolledUV.x, 1.0 - scrolledUV.y);
    vec4 texColor = texture(textureSampler, sampleUV);

    // Determine final color
    vec4 finalColor;
    if (color.a > 0.01 && texColor.a < 0.99) {
        // Blend texture over background color
        finalColor = mix(color, texColor, texColor.a);
    } else {
        finalColor = texColor;
    }

    // Discard fully transparent pixels
    if (finalColor.a < 0.01) {
        discard;
    }

    FragColor = finalColor;
}
