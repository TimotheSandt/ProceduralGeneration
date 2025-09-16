#include "utilities.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


float lerp(float a, float b, float t) {
    return a + t * (b - a);
}



void rotate(float& x, float& y, float angle) {
    glm::vec2 v = glm::rotate(glm::vec2(x, y), angle);
    x = v.x;
    y = v.y;
}


void rotate(float& x, float& y, float& z, float angle) {
    glm::vec3 axis = glm::normalize(glm::vec3(1, 1, 1));
    glm::quat q = glm::angleAxis(angle, axis);
    glm::vec3 rotatedQuat = q * glm::vec3(x, y, z);
    x = rotatedQuat.x;
    y = rotatedQuat.y;
    z = rotatedQuat.z;
}


void rotate(float& x, float& y, float& z, float& w, float angle) {
    glm::mat4 rotXY(1.0f);
    rotXY[0][0] = cos(angle);  rotXY[0][1] = sin(angle);
    rotXY[1][0] = -sin(angle); rotXY[1][1] = cos(angle);
    
    // Rotation in ZW plane
    glm::mat4 rotZW(1.0f);
    rotZW[2][2] = cos(angle);  rotZW[2][3] = sin(angle);
    rotZW[3][2] = -sin(angle); rotZW[3][3] = cos(angle);
    
    // Combined rotation
    glm::mat4 combinedRot = rotZW * rotXY;
    glm::vec4 rotatedCombined = combinedRot * glm::vec4(x, y, z, w);
    x = rotatedCombined.x;
    y = rotatedCombined.y;
    z = rotatedCombined.z;
    w = rotatedCombined.w;
}