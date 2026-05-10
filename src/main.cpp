#include "Game.h"
#include "Graphics/Backend/GraphicsAPI.h"
#include "Graphics/Core/GraphicsBackend.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{

bool IsGameLaunchArgument(std::string_view argument) noexcept
{
    if (argument == "--world" || argument.rfind("--world=", 0) == 0)
    {
        return true;
    }
#ifdef DEBUG
    if (argument == "--no-overlay" || argument == "--no-ui" || argument == "--no-profiler" ||
        argument == "--benchmark" || argument.rfind("--benchmark=", 0) == 0 || argument == "--benchmark-seconds" ||
        argument.rfind("--benchmark-seconds=", 0) == 0 || argument == "--benchmark-log-interval" ||
        argument.rfind("--benchmark-log-interval=", 0) == 0)
    {
        return true;
    }
#endif
    return false;
}

bool GameLaunchArgumentConsumesValue(std::string_view argument) noexcept
{
#ifdef DEBUG
    return argument == "--world" || argument == "--benchmark" || argument == "--benchmark-seconds" ||
           argument == "--benchmark-log-interval";
#else
    return argument == "--world";
#endif
}

std::vector<const char *> BuildGraphicsArgv(int argc, char **argv)
{
    std::vector<const char *> graphicsArgv;
    graphicsArgv.reserve(static_cast<std::size_t>(argc));
    graphicsArgv.push_back(argv[0]);

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (IsGameLaunchArgument(argument))
        {
            if (GameLaunchArgumentConsumesValue(argument) && index + 1 < argc)
            {
                ++index;
            }
            continue;
        }
        graphicsArgv.push_back(argv[index]);
    }

    return graphicsArgv;
}

} // namespace

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

        const GameLaunchOptions gameOptions = ParseGameLaunchOptions(argc, argv);
        const std::vector<const char *> graphicsArgv = BuildGraphicsArgv(argc, argv);
        const GraphicsLaunchOptions launchOptions = ParseGraphicsLaunchOptions(static_cast<int>(graphicsArgv.size()), graphicsArgv.data());

        if (launchOptions.showHelp)
        {
            PrintGraphicsAPIUsage(std::cout);
            PrintGameLaunchUsage(std::cout);
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
        const bool shouldPromptForApi = launchOptions.chooseApiInteractively;
        if (shouldPromptForApi && !PromptForGraphicsAPI(std::cin, std::cout, selectedApi))
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

            Game game(gameOptions);
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
