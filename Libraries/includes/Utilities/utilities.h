#pragma once

#include <string>
#include <vector>

float lerp(float a, float b, float t);

void rotate(float &x, float &y, float angle);

void rotate(float &x, float &y, float &z, float angle);

void rotate(float &x, float &y, float &z, float &w, float angle);

#ifndef UNUSED
#define UNUSED(P) (void)(P)
#endif

#ifdef RELEASE
#define GET_RESOURCE_PATH(path) path
#else
#define GET_RESOURCE_PATH(path) "res/" path
#endif

std::string GetExecutablePath();
std::string GetExecutableDirectory();
void SetWorkingDirectoryToExe();
std::string GetUserDataPath();
bool ValidateAssets(const std::vector<std::string> &assetPaths, std::vector<std::string> *missingAssets = nullptr);
