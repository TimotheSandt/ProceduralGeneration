#include "Utilities/Logger.h"
#include "suites/Suites.h"

#include <vector>

int main()
{
    Logger::SetMinimumLevel(L_FATAL);

    const std::vector<tests::TestSuite> suites = {tests::CreateUtilitiesSuite(), tests::CreateShaderSuite(),
                                                  tests::CreateNoiseSuite(),     tests::CreateProceduralSuite(),
                                                  tests::CreateProfilerSuite(),  tests::CreateUIFoundationSuite()};

    return tests::RunSuites(suites);
}
