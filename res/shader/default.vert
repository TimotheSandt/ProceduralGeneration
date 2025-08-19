#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 camMatrix;

out vec2 texCoord;
out vec3 normal;
out vec3 crntPos;

void main()
{
   crntPos = vec3(model * vec4(aPos, 1.0f));

   gl_Position = camMatrix * vec4(crntPos, 1.0f);
   texCoord = aTexCoord;
   normal = aNormal;
}