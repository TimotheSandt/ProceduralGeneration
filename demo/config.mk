# Configuration for packaging

# GLOBALS
PROJECT_NAME = ProceduralGeneration
VERSION = 0.0.1
PUBLISHER = Gasshog
MAINTAINER = Timothe Sandt <timothe.sandt@proton.me>
ICON_NAME = res/public/icon.ico
ARCHITECTURE = amd64
PROJECT_DESCRIPTION = A C++ application for procedural terrain generation using OpenGL and GLFW. \n\
 .\n\
 This is still in early development, procedural generation is near absent. \n\
 The development is focused on making the graphics engine for now. \n\
 .\n\
 This package provides the executable and necessary runtime files.


# Windows Specific
ARCHITECTURE_WINDOWS = x64


# Linux Specific
PACKAGE = proceduralgeneration
ARCHITECTURE_LINUX = amd64
DEPENDS = libglfw3 (>= 3.3), libx11-6, libc6, libstdc++6
SECTION = utils
PRIORITY = optional
