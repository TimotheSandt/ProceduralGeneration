#pragma once


float lerp(float a, float b, float t);

void rotate(float& x, float& y, float angle);

void rotate(float& x, float& y, float& z, float angle);

void rotate(float& x, float& y, float& z, float& w, float angle);



#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P) = (P)
#endif