#include "Graphics/Backend/GraphicsAPI.h"

#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace
{

bool EqualsIgnoreCase(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(left[index])) != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }

    return true;
}

std::string JoinAvailableApis()
{
    constexpr std::array<GraphicsAPI, 3> allApis = {
        GraphicsAPI::OpenGL,
        GraphicsAPI::Vulkan,
        GraphicsAPI::Metal,
    };

    std::ostringstream builder;
    for (std::size_t index = 0; index < allApis.size(); ++index)
    {
        if (index != 0)
        {
            builder << ", ";
        }
        builder << GraphicsAPIToString(allApis[index]);
    }
    return builder.str();
}

} // namespace

std::string_view GraphicsAPIToString(GraphicsAPI api) noexcept
{
    switch (api)
    {
        case GraphicsAPI::OpenGL:
            return "OpenGL";
        case GraphicsAPI::Vulkan:
            return "Vulkan";
        case GraphicsAPI::Metal:
            return "Metal";
    }

    return "Unknown";
}

std::optional<GraphicsAPI> ParseGraphicsAPI(std::string_view value) noexcept
{
    if (EqualsIgnoreCase(value, "opengl") || EqualsIgnoreCase(value, "gl"))
    {
        return GraphicsAPI::OpenGL;
    }
    if (EqualsIgnoreCase(value, "vulkan") || EqualsIgnoreCase(value, "vk"))
    {
        return GraphicsAPI::Vulkan;
    }
    if (EqualsIgnoreCase(value, "metal"))
    {
        return GraphicsAPI::Metal;
    }

    return std::nullopt;
}

GraphicsLaunchOptions ParseGraphicsLaunchOptions(int argc, const char *const *argv)
{
    GraphicsLaunchOptions options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument(argv[index]);

        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            continue;
        }
        if (argument == "--list-apis")
        {
            options.listApis = true;
            continue;
        }
        if (argument == "--choose-api")
        {
            options.chooseApiInteractively = true;
            continue;
        }
        if (argument == "--api")
        {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument("Missing value after --api");
            }

            const auto parsedApi = ParseGraphicsAPI(argv[++index]);
            if (!parsedApi.has_value())
            {
                throw std::invalid_argument("Unknown graphics API: " + std::string(argv[index]));
            }

            options.api = *parsedApi;
            options.apiExplicitlyRequested = true;
            continue;
        }
        if (argument.rfind("--api=", 0) == 0)
        {
            const auto parsedApi = ParseGraphicsAPI(argument.substr(6));
            if (!parsedApi.has_value())
            {
                throw std::invalid_argument("Unknown graphics API: " + argument.substr(6));
            }

            options.api = *parsedApi;
            options.apiExplicitlyRequested = true;
            continue;
        }

        throw std::invalid_argument("Unknown argument: " + argument);
    }

    return options;
}

void PrintGraphicsAPIUsage(std::ostream &out)
{
    out << "Usage: ProceduralGeneration [--api <name>] [--choose-api] [--list-apis] [--help]\n";
    out << "Available API names: " << JoinAvailableApis() << '\n';
    out << "Example: ProceduralGeneration --api opengl\n";
}

bool PromptForGraphicsAPI(std::istream &input, std::ostream &output, GraphicsAPI &selectedApi)
{
    output << "Select a graphics API:\n";
    output << "  1. OpenGL  - available now\n";
    output << "  2. Vulkan  - recognized, not implemented yet\n";
    output << "  3. Metal   - recognized, not implemented yet\n";
    output << "Press Enter for OpenGL.\n";
    output << "> ";

    std::string choice;
    if (!std::getline(input, choice))
    {
        return false;
    }

    if (choice.empty())
    {
        selectedApi = GraphicsAPI::OpenGL;
        return true;
    }

    if (choice == "1")
    {
        selectedApi = GraphicsAPI::OpenGL;
        return true;
    }
    if (choice == "2")
    {
        selectedApi = GraphicsAPI::Vulkan;
        return true;
    }
    if (choice == "3")
    {
        selectedApi = GraphicsAPI::Metal;
        return true;
    }

    return false;
}

GraphicsAPIAvailability GetGraphicsAPIAvailability(GraphicsAPI api) noexcept
{
    switch (api)
    {
        case GraphicsAPI::OpenGL:
            return GraphicsAPIAvailability::Available;
        case GraphicsAPI::Vulkan:
            return GraphicsAPIAvailability::NotBuilt;
        case GraphicsAPI::Metal:
#ifdef __APPLE__
            return GraphicsAPIAvailability::NotBuilt;
#else
            return GraphicsAPIAvailability::UnsupportedPlatform;
#endif
    }

    return GraphicsAPIAvailability::UnsupportedPlatform;
}

bool IsGraphicsAPIAvailable(GraphicsAPI api) noexcept { return GetGraphicsAPIAvailability(api) == GraphicsAPIAvailability::Available; }

std::string GetGraphicsAPIAvailabilityMessage(GraphicsAPI api)
{
    switch (GetGraphicsAPIAvailability(api))
    {
        case GraphicsAPIAvailability::Available:
            return std::string(GraphicsAPIToString(api)) + " backend is available.";
        case GraphicsAPIAvailability::NotBuilt:
            return std::string(GraphicsAPIToString(api)) + " backend selection is recognized, but this backend is not implemented yet.";
        case GraphicsAPIAvailability::UnsupportedPlatform:
            return std::string(GraphicsAPIToString(api)) + " backend is not supported on this platform.";
    }

    return "Unknown graphics backend state.";
}
