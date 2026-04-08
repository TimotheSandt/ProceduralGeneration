#include "Game.h"
#include "Logger.h"

#include <exception>
#include <iostream>
#include <vector>

int main()
{
    SetWorkingDirectoryToExe();

#ifdef DEBUG
    SET_LOG_FILE("logs/log.log");
#else
    SET_LOG_FILE_DEFAULT;
#endif

    const std::vector<std::string> requiredAssets = {
        GET_RESOURCE_PATH("fonts/Roboto-Regular.ttf"),      GET_RESOURCE_PATH("shader/default.vert"),
        GET_RESOURCE_PATH("shader/default.frag"),           GET_RESOURCE_PATH("shader/upscaling/upscale.vert"),
        GET_RESOURCE_PATH("shader/upscaling/upscale.frag"), GET_RESOURCE_PATH("shader/UI/default.vert"),
        GET_RESOURCE_PATH("shader/UI/default.frag"),        GET_RESOURCE_PATH("shader/UI/container.vert"),
        GET_RESOURCE_PATH("shader/UI/container.frag")};

    if (!ValidateAssets(requiredAssets))
    {
        FLUSH_LOG_TO_FILE;
        return EXIT_FAILURE;
    }

    bool openGLInitialized = false;
    int exitCode = EXIT_SUCCESS;

    try
    {
        if (!Window::InitOpenGL())
        {
            exitCode = EXIT_FAILURE;
        }
        else
        {
            openGLInitialized = true;
            LOG_INFO("Starting game");

            Game game;
            LOG_TRACE("Game created");
            game.init();
            LOG_TRACE("Game initialized");
            game.run();
            LOG_INFO("Game stopped");
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(1, "Unhandled exception: ", e.what());
        exitCode = EXIT_FAILURE;
    }
    catch (...)
    {
        LOG_ERROR(1, "Unhandled non-standard exception");
        exitCode = EXIT_FAILURE;
    }

    if (openGLInitialized)
    {
        Window::TerminateOpenGL();
    }
    FLUSH_LOG_TO_FILE;
    return exitCode;
}
