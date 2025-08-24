#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 model;
uniform mat4 camMatrix;

out vec3 normal;
out vec3 crntPos;
out vec3 color;

void main()
{
   crntPos = vec3(model * vec4(aPos, 1.0f));
   normal = aNormal;
   color = aColor;
   
   gl_Position = camMatrix * vec4(crntPos, 1.0f);
}