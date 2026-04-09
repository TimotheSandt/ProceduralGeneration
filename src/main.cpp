#include "Game.h"
#include "Graphics/Backend/GraphicsAPI.h"
#include "Graphics/Core/GraphicsBackend.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char **argv)
{
    GraphicsAPI selectedApi = GraphicsAPI::OpenGL;
    std::unique_ptr<IGraphicsBackend> graphicsBackend;
    std::unique_ptr<IGraphicsDevice> graphicsDevice;

    try
    {
        SetWorkingDirectoryToExe();

#ifdef DEBUG
        SET_LOG_FILE("logs/log.log");
#else
        SET_LOG_FILE_DEFAULT;
#endif

        const GraphicsLaunchOptions launchOptions = ParseGraphicsLaunchOptions(argc, argv);

        if (launchOptions.showHelp)
        {
            PrintGraphicsAPIUsage(std::cout);
            return EXIT_SUCCESS;
        }

        if (launchOptions.listApis)
        {
            for (const GraphicsAPI api : {GraphicsAPI::OpenGL, GraphicsAPI::Vulkan, GraphicsAPI::Metal})
            {
                std::cout << GraphicsAPIToString(api) << ": " << GetGraphicsAPIAvailabilityMessage(api) << '\n';
            }
            return EXIT_SUCCESS;
        }

        selectedApi = launchOptions.api;
        graphicsBackend = CreateGraphicsBackend({selectedApi});
        if (launchOptions.chooseApiInteractively && !PromptForGraphicsAPI(std::cin, std::cout, selectedApi))
        {
            LOG_ERROR(1, "Interactive graphics API selection failed");
            PrintGraphicsAPIUsage(std::cout);
            FLUSH_LOG_TO_FILE;
            return EXIT_FAILURE;
        }

        graphicsBackend = CreateGraphicsBackend({selectedApi});
        if (!graphicsBackend->IsAvailable())
        {
            LOG_ERROR(1, graphicsBackend->DescribeAvailability());
            PrintGraphicsAPIUsage(std::cout);
            FLUSH_LOG_TO_FILE;
            return EXIT_FAILURE;
        }

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

        if (!graphicsBackend->Initialize())
        {
            exitCode = EXIT_FAILURE;
        }
        else
        {
            graphicsDevice = graphicsBackend->CreateDevice({});
            BindGraphicsRuntime({.api = selectedApi, .backend = graphicsBackend.get(), .device = graphicsDevice.get()});
            LOG_INFO("Starting game with graphics API: ", GraphicsAPIToString(selectedApi));

            Game game;
            LOG_TRACE("Game created");
            game.init();
            LOG_TRACE("Game initialized");
            game.run();
            LOG_INFO("Game stopped");
        }

        if (graphicsBackend)
        {
            ClearGraphicsRuntime();
            graphicsDevice.reset();
            graphicsBackend->Shutdown();
        }
        FLUSH_LOG_TO_FILE;
        return exitCode;
    }
    catch (const std::exception &e)
    {
        if (graphicsBackend)
        {
            ClearGraphicsRuntime();
            graphicsDevice.reset();
            graphicsBackend->Shutdown();
        }
        LOG_ERROR(1, "Unhandled exception: ", e.what());
        FLUSH_LOG_TO_FILE;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        if (graphicsBackend)
        {
            ClearGraphicsRuntime();
            graphicsDevice.reset();
            graphicsBackend->Shutdown();
        }
        LOG_ERROR(1, "Unhandled non-standard exception");
        FLUSH_LOG_TO_FILE;
        return EXIT_FAILURE;
    }
}
