#include "framework/TestFramework.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tests
{

namespace
{

struct SuiteProgress
{
    std::string name;
    int total = 0;
    int completed = 0;
    int passed = 0;
    int failed = 0;
};

std::string ProgressBar(int completed, int total, int width = 28)
{
    if (total <= 0)
    {
        return "[" + std::string(width, '-') + "]";
    }

    const int filled = std::clamp((completed * width) / total, 0, width);
    return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
}

std::string BuildSummaryLine(const std::string &name, int completed, int total, int passed, int failed)
{
    std::ostringstream output;
    output << std::left << std::setw(14) << name << " " << ProgressBar(completed, total) << " " << std::setw(3) << completed << "/"
           << std::setw(3) << total << " | todo " << std::setw(3) << (total - completed) << " | ok " << std::setw(3) << passed << " | fail "
           << failed;
    return output.str();
}

void RenderDashboard(const std::vector<SuiteProgress> &suiteProgress, const std::vector<FailureInfo> &failures,
                     const std::string &currentSuite, const std::string &currentTest)
{
    int totalTests = 0;
    int totalCompleted = 0;
    int totalPassed = 0;
    int totalFailed = 0;

    for (const SuiteProgress &suite : suiteProgress)
    {
        totalTests += suite.total;
        totalCompleted += suite.completed;
        totalPassed += suite.passed;
        totalFailed += suite.failed;
    }

    std::ostringstream output;
    output << "\x1b[2J\x1b[H";
    output << "ProceduralGeneration Test Runner\n\n";
    output << "Current suite: " << (currentSuite.empty() ? "-" : currentSuite) << "\n";
    output << "Current test : " << (currentTest.empty() ? "-" : currentTest) << "\n\n";
    output << BuildSummaryLine("Total", totalCompleted, totalTests, totalPassed, totalFailed) << "\n\n";

    for (const SuiteProgress &suite : suiteProgress)
    {
        output << BuildSummaryLine(suite.name, suite.completed, suite.total, suite.passed, suite.failed) << "\n";
    }

    output << "\nRecent failures:\n";
    if (failures.empty())
    {
        output << "  none\n";
    }
    else
    {
        const size_t begin = failures.size() > 6 ? failures.size() - 6 : 0;
        for (size_t i = begin; i < failures.size(); ++i)
        {
            output << "  - " << failures[i].suiteName << " / " << failures[i].testName << ": " << failures[i].message << "\n";
        }
    }

    std::cout << output.str() << std::flush;
}

std::string ExceptionMessage(const std::exception &exception)
{
    return exception.what() == nullptr ? "unknown std::exception" : exception.what();
}

} // namespace

AssertionFailure::AssertionFailure(const std::string &message) : std::runtime_error(message) {}

void AddTest(TestSuite &suite, const std::string &name, std::function<void()> run)
{
    suite.tests.push_back(TestCase{name, std::move(run)});
}

void Assert(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw AssertionFailure(message);
    }
}

void AssertNear(double actual, double expected, double epsilon, const std::string &message)
{
    if (std::abs(actual - expected) > epsilon)
    {
        std::ostringstream output;
        output << message << " (expected " << expected << ", got " << actual << ")";
        throw AssertionFailure(output.str());
    }
}

int RunSuites(const std::vector<TestSuite> &suites)
{
    std::vector<SuiteProgress> suiteProgress;
    suiteProgress.reserve(suites.size());

    for (const TestSuite &suite : suites)
    {
        suiteProgress.push_back(SuiteProgress{suite.name, static_cast<int>(suite.tests.size())});
    }

    std::vector<FailureInfo> failures;
    std::string currentSuite;
    std::string currentTest;

    RenderDashboard(suiteProgress, failures, currentSuite, currentTest);

    for (size_t suiteIndex = 0; suiteIndex < suites.size(); ++suiteIndex)
    {
        const TestSuite &suite = suites[suiteIndex];
        SuiteProgress &progress = suiteProgress[suiteIndex];

        for (const TestCase &test : suite.tests)
        {
            currentSuite = suite.name;
            currentTest = test.name;
            RenderDashboard(suiteProgress, failures, currentSuite, currentTest);

            try
            {
                test.run();
                ++progress.passed;
            }
            catch (const AssertionFailure &failure)
            {
                ++progress.failed;
                failures.push_back(FailureInfo{suite.name, test.name, failure.what()});
            }
            catch (const std::exception &exception)
            {
                ++progress.failed;
                failures.push_back(FailureInfo{suite.name, test.name, ExceptionMessage(exception)});
            }
            catch (...)
            {
                ++progress.failed;
                failures.push_back(FailureInfo{suite.name, test.name, "unknown exception"});
            }

            ++progress.completed;
            RenderDashboard(suiteProgress, failures, currentSuite, currentTest);
        }
    }

    currentSuite.clear();
    currentTest.clear();
    RenderDashboard(suiteProgress, failures, currentSuite, currentTest);

    return failures.empty() ? 0 : 1;
}

} // namespace tests
