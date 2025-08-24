#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


float lerp(float a, float b, float t);

void rotate(float& x, float& y, float angle);
void rotate(float& x, float& y, float& z, float angle);
void rotate(float& x, float& y, float& z, float& w, float angle);