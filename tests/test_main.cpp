#include "Utilities/Logger.h"
#include "suites/Suites.h"

#include <cstdlib>
#include <exception>
#include <vector>

int main()
{
    try
    {
        Logger::SetMinimumLevel(L_FATAL);

        const std::vector<tests::TestSuite> suites = {
            tests::CreateUtilitiesSuite(),  tests::CreateGraphicsAPISuite(), tests::CreateGraphicsCoreSuite(),
            tests::CreateRenderersSuite(),  tests::CreateShaderSuite(),      tests::CreateNoiseSuite(),
            tests::CreateProceduralSuite(), tests::CreateProfilerSuite(),    tests::CreateUIFoundationSuite()};

        return tests::RunSuites(suites);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(1, "Unhandled test exception: ", e.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        LOG_ERROR(1, "Unhandled non-standard test exception");
        return EXIT_FAILURE;
    }
}
