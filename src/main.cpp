#include "Game.h"
#include "Logger.h"

#include <exception>
#include <iostream>
#include <vector>

int main()
{
    bool openGLInitialized = false;

    try
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

        int exitCode = EXIT_SUCCESS;

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

        if (openGLInitialized)
        {
            Window::TerminateOpenGL();
        }
        FLUSH_LOG_TO_FILE;
        return exitCode;
    }
    catch (const std::exception &e)
    {
        if (openGLInitialized)
        {
            Window::TerminateOpenGL();
        }
        LOG_ERROR(1, "Unhandled exception: ", e.what());
        FLUSH_LOG_TO_FILE;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        if (openGLInitialized)
        {
            Window::TerminateOpenGL();
        }
        LOG_ERROR(1, "Unhandled non-standard exception");
        FLUSH_LOG_TO_FILE;
        return EXIT_FAILURE;
    }
}
