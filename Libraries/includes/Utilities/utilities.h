#pragma once


float lerp(float a, float b, float t);

void rotate(float& x, float& y, float angle);

void rotate(float& x, float& y, float& z, float angle);

void rotate(float& x, float& y, float& z, float& w, float angle);

#ifdef DEBUG
#include "Logger.h"
#define GL_CHECK_ERROR_M(...) do { \
    GLenum err; \
    while ((err = glGetError()) != GL_NO_ERROR) { \
        LOG_ERROR(err, "OpenGL error : ", __VA_ARGS__); \
    } \
} while (0)

#define GL_CHECK_ERROR() do { \
    GLenum err; \
    while ((err = glGetError()) != GL_NO_ERROR) { \
        LOG_ERROR(err, "OpenGL error"); \
    } \
} while (0)
#else
#define GL_CHECK_ERROR()
#define GL_CHECK_ERROR_M(...)
#endif