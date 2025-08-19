#version 330 core

out vec4 FragColor;

in vec3 crntPos;
in vec4 color;

uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec4 AmbientLight;
uniform vec3 camPos;

uniform vec3 SunDirection;
uniform float SunIntensity;
uniform float a;
uniform float b;

/*

vec4 pointLight()
{
   vec3 lightVec = lightPos - crntPos;

   // intensity of light depending of the distance
   float dist = length(lightVec);
   float inten = 1.0f / (a * dist*dist + b * dist + 1.0f);

   // diffuse light
   vec3 norm = normalize(normal);
   vec3 lightDirection = normalize(lightPos - crntPos);
   float diffuse = max(dot(norm, lightDirection), 0.0f) * inten;

   // specular light
   vec3 viexDirection = normalize(camPos - crntPos);
   vec3 reflectionDirection = reflect(-lightDirection, norm);
   float specAmount = pow(max(dot(viexDirection, reflectionDirection), 0.0f), 16);
   float specularLight = 0.5f;
   float specular = specAmount * specularLight;

   return lightColor * (diffuse + specular + AmbientLight);
}

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

   return lightColor * (diffuse + specular + AmbientLight) * SunIntensity;
}

vec4 spotLight()
{
   vec3 spotDirection = vec3(1.0f, 1.0f, 1.0f);

	// controls how big the area that is lit up is
	float outerCone = 0.90f;
	float innerCone = 0.95f;
   
   // diffuse light
   vec3 norm = normalize(normal);
   vec3 lightDirection = normalize(lightPos - crntPos);
   float diffuse = max(dot(norm, lightDirection), 0.0f);

   // specular light
   vec3 viexDirection = normalize(camPos - crntPos);
   vec3 reflectionDirection = reflect(-lightDirection, norm);
   float specAmount = pow(max(dot(viexDirection, reflectionDirection), 0.0f), 64);
   float specularLight = 0.5f;
   float specular = specAmount * specularLight;

   
   float angle = dot(spotDirection, -lightDirection);
   float inten = clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);
   diffuse = diffuse * inten;

   return lightColor * (diffuse + specular + AmbientLight);
}

*/

void main()
{
   FragColor = color; // * (pointLight() + directLight());
}