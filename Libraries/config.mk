# Engine identity — single source of truth for the build system and the engine tool binary.
# Commit this file.  Values here are baked into the engine[.exe] binary at compile time.

ENGINE_NAME        ?= GameEngine
ENGINE_VERSION     ?= 0.1.0
ENGINE_PUBLISHER   ?= Gasshog
ENGINE_AUTHOR      ?= Timothé Sandt
ENGINE_EMAIL       ?= timothe.sandt@proton.me
ENGINE_DESCRIPTION ?= A C++ game engine with OpenGL, GLFW, and procedural generation support.
ENGINE_HOMEPAGE    ?= https://github.com/TimotheSandt/ProceduralGeneration
