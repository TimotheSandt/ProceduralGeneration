#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTranslate;
layout (location = 3) in vec4 aColor;

uniform mat4 model;
uniform mat4 camMatrix;



out vec3 crntPos;
out vec4 color;

void main()
{
    crntPos = vec3(model * vec4(aPos + aTranslate, 1.0f));
    color = aColor;
    gl_Position = camMatrix * vec4(crntPos, 1.0f);
}