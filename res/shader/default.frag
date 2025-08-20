#version 330 core

out vec4 FragColor;

uniform int wireframe;

in vec3 normal;
in vec3 crntPos;



uniform vec3 AmbientLight;
uniform vec3 camPos;

uniform vec3 SunDirection;
uniform vec3 SunColor;
uniform float SunIntensity;


vec4 directLight()
{
   // diffuse light
   vec3 norm = normalize(normal);
   vec3 lightDirection = normalize(SunDirection);
   float diffuse = max(dot(norm, lightDirection), 0.0f);

   // specular light
   vec3 viexDirection = normalize(camPos - crntPos);
   vec3 reflectionDirection = reflect(-lightDirection, norm);
   float specAmount = pow(max(dot(viexDirection, reflectionDirection), 0.0f), 16);
   float specularLight = 0.5f;
   float specular = specAmount * specularLight;

   return vec4(SunColor, 1.0f) * (diffuse + specular) * SunIntensity;
}

vec4 wireframeRender() {
   return vec4(0.0f);
}

vec4 fillRender() {
   return vec4(1.0f) * (directLight() + vec4(AmbientLight, 1.0f));
}

void main()
{
   if (wireframe == 1) {
      FragColor = wireframeRender();
   } else {
      FragColor = fillRender();
   }
}