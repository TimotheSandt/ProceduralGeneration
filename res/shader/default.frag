#version 430 core

out vec4 FragColor;

uniform int wireframe;

in vec3 normal;
in vec3 crntPos;
in vec3 color;

layout(binding = 0, std140) uniform CamBlock {
   vec3 position;
   mat4 matrix;
} camera;


struct LightStruct
{
   int type;

   vec3 position;
   vec3 direction;
   vec3 color;

   float strength;

   float outerCone;
   float innerCone;
   float a;
   float b;
   float _pad[3];
};

layout(binding = 1, std140) uniform AmbientLightBlock {
   vec3 color;
   float strength;
} AmbientLight;

layout(std430, binding = 2) buffer LightBlock {
   int size;
   LightStruct lights[];   
} lights;



vec4 directLight(vec3 lightDirection, vec3 lightColor, float lightIntensity) {
   // diffuse light
   vec3 norm = normalize(normal);
   lightDirection = normalize(lightDirection);
   float diffuse = max(dot(norm, lightDirection), 0.0f);

   // specular light
   vec3 viexDirection = normalize(camera.position - crntPos);
   vec3 reflectionDirection = reflect(-lightDirection, norm);
   float specAmount = pow(max(dot(viexDirection, reflectionDirection), 0.0f), 16);
   float specularLight = 0.5f;
   float specular = specAmount * specularLight;

   return vec4(lightColor, 1.0f) * (diffuse + specular) * lightIntensity;
}

vec4 wireframeRender() {
   return vec4(0.0f);
}

vec4 fillRender() {

   vec4 l = vec4(AmbientLight.color.rgb * AmbientLight.strength, 1.0f);
   for(int i = 0; i < lights.size; i++) {
      LightStruct lght = lights.lights[i];
      if (lght.type == 1) {
         l += vec4(lght.color * lght.strength, 1.0f);
      } else if (lght.type == 2) {
         l += directLight(lght.direction, lght.color, lght.strength);
      }
   }
   

   return vec4(color, 1.0f) * l;
}

void main()
{
   if (wireframe == 1) {
      FragColor = wireframeRender();
   } else {
      FragColor = fillRender();
   }
}