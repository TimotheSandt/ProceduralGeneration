#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;


layout(binding = 0, std140) uniform CamBlock {
   vec3 position;
   mat4 matrix;
} camera;

layout(binding = 4, std140) uniform ModelBlock {
   mat4 model;
} model;

out vec3 normal;
out vec3 crntPos;
out vec3 color;

void main()
{
   crntPos = vec3(model.model * vec4(aPos, 1.0f));
   normal = aNormal;
   color = aColor;
   
   gl_Position = camera.matrix * vec4(crntPos, 1.0f);
}