# Configuration for packaging
VERSION = 0.0.1
PRODUCT_NAME = "ProceduralGeneration"
PUBLISHER = "Gasshog"
MAINTAINER = "Timothe Sandt <timothe.sandt@proton.me>"
PROJECT_DESCRIPTION = A C++ application for procedural terrain generation using OpenGL and GLFW.
ARCHITECTURE = "amd64"
DEPENDS = "libglfw3 (>= 3.3), libx11-6, libc6, libstdc++6"
SECTION = "utils"
PRIORITY = "optional"
ICON_PATH = ""  # Path to icon.ico for NSIS, e.g., "res/icon.ico"