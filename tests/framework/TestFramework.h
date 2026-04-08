#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tests {

struct TestCase {
    std::string name;
    std::function<void()> run;
};

struct TestSuite {
    std::string name;
    std::vector<TestCase> tests;
};

struct FailureInfo {
    std::string suiteName;
    std::string testName;
    std::string message;
};

class AssertionFailure final : public std::runtime_error {
public:
    explicit AssertionFailure(const std::string& message);
};

void AddTest(TestSuite& suite, const std::string& name, std::function<void()> run);
int RunSuites(const std::vector<TestSuite>& suites);

void Assert(bool condition, const std::string& message);
void AssertNear(double actual, double expected, double epsilon, const std::string& message);

template <typename T, typename U>
void AssertEqual(const T& actual, const U& expected, const std::string& message) {
    if (!(actual == expected)) {
        throw AssertionFailure(message);
    }
}

}  // namespace tests
