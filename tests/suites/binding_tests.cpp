#include "suites/Suites.h"

#include "UI/Utils/BoundTypes/BindFunc.h"
#include "UI/Utils/BoundTypes/BindMethods.h"

namespace tests
{

TestSuite CreateBindingSuite()
{
    TestSuite suite{"Bindings"};

    AddTest(suite, "bind func caches a dynamic argument",
            []
            {
                int calls = 0;
                auto sum = [&calls](int a, int b) {
                    ++calls;
                    return a + b;
                };

                int a = 1;
                int b = 2;
                auto bound = UI::Call(sum, UI::BindStatic(a), UI::BindDynamic(b));

                Assert(!bound.isDirty(), "Fresh dynamic bindings should not be dirty");
                AssertEqual(bound.apply(), 3, "First apply should compute the result");
                AssertEqual(calls, 1, "First apply should execute the callable once");

                Assert(!bound.IsDirty(), "Snapshot should be clean after apply");
                AssertEqual(bound.apply(), 3, "Second apply should reuse the cached result");
                AssertEqual(calls, 1, "Cached apply should not execute the callable again");

                b = 5;
                Assert(bound.isDirty(), "Changing a dynamic value should mark the binding dirty");
                AssertEqual(bound.apply(), 6, "Dirty binding should recompute the result");
                AssertEqual(calls, 2, "Dirty apply should execute the callable again");

                Assert(!bound.IsDirty(), "Binding should be clean again after recomputing");
                AssertEqual(bound.apply(), 6, "Clean binding should reuse the updated cached result");
                AssertEqual(calls, 2, "Cached result should still avoid extra execution");
            });

    AddTest(suite, "bind func keeps a static snapshot",
            []
            {
                int calls = 0;
                auto identity = [&calls](int value) {
                    ++calls;
                    return value;
                };

                int value = 4;
                auto bound = UI::Call(identity, UI::BindStatic(value));

                value = 9;
                Assert(!bound.isDirty(), "Static bindings should not become dirty when the source changes");
                AssertEqual(bound.apply(), 4, "Static bindings should keep the original value");
                AssertEqual(calls, 1, "Static binding should execute once");
                AssertEqual(bound.apply(), 4, "Second apply should reuse the cached static result");
                AssertEqual(calls, 1, "Cached static result should not re-run the callable");
            });

    AddTest(suite, "bind method caches and tracks dynamic values",
            []
            {
                struct Calculator
                {
                    int base = 0;
                    int calls = 0;

                    bool operator==(const Calculator &) const = default;

                    int Add(int value)
                    {
                        ++calls;
                        return base + value;
                    }
                };

                Calculator calculator{10};
                int value = 7;
                auto bound = UI::Bind(calculator, &Calculator::Add, UI::BindDynamic(value));

                Assert(!bound.isDirty(), "Fresh method bindings should not be dirty");
                AssertEqual(bound.apply(), 17, "First method apply should compute the result");
                AssertEqual(calculator.calls, 1, "First method apply should execute once");
                AssertEqual(bound.apply(), 17, "Second method apply should reuse the cached result");
                AssertEqual(calculator.calls, 1, "Cached method apply should not execute again");

                value = 8;
                Assert(bound.isDirty(), "Changing the dynamic value should mark the method dirty");
                AssertEqual(bound.apply(), 18, "Dirty method binding should recompute the result");
                AssertEqual(calculator.calls, 2, "Dirty method apply should execute again");
                AssertEqual(bound.apply(), 18, "Clean method binding should reuse the updated cache");
                AssertEqual(calculator.calls, 2, "Cached method result should not call the method again");

                calculator.base = 12;
                Assert(bound.isDirty(), "Changing the bound object should mark the method dirty");
                AssertEqual(bound.apply(), 20, "Dirty method binding should recompute when the object changes");
                AssertEqual(calculator.calls, 3, "Object change should execute the method again");
                AssertEqual(bound.apply(), 20, "Clean method binding should reuse the updated cache after the object changes");
                AssertEqual(calculator.calls, 3, "Cached method result should not call the method again after object changes");
            });

    AddTest(suite, "bind method caches zero-argument object state",
            []
            {
                struct Counter
                {
                    int value = 0;
                    int calls = 0;

                    bool operator==(const Counter &) const = default;

                    int Next()
                    {
                        ++calls;
                        return ++value;
                    }
                };

                Counter counter{10};
                auto bound = UI::Bind(counter, &Counter::Next);

                Assert(!bound.isDirty(), "Fresh zero-argument method bindings should not be dirty");
                AssertEqual(bound.apply(), 11, "First zero-argument method apply should compute the result");
                AssertEqual(counter.calls, 1, "First zero-argument method apply should execute once");
                AssertEqual(bound.apply(), 11, "Second zero-argument method apply should reuse the cached result");
                AssertEqual(counter.calls, 1, "Cached zero-argument method apply should not execute again");

                counter.value = 20;
                Assert(bound.isDirty(), "Changing the bound object should mark the zero-argument method dirty");
                AssertEqual(bound.apply(), 21, "Dirty zero-argument method binding should recompute");
                AssertEqual(counter.calls, 2, "Dirty zero-argument method apply should execute again");
                AssertEqual(bound.apply(), 21, "Clean zero-argument method binding should reuse the updated cache");
                AssertEqual(counter.calls, 2, "Cached zero-argument method result should not call the method again");
            });

    AddTest(suite, "bind const methods work on const objects",
            []
            {
                struct Calculator
                {
                    int base = 0;
                    mutable int calls = 0;

                    bool operator==(const Calculator &) const = default;

                    int Add(int value) const
                    {
                        ++calls;
                        return base + value;
                    }
                };

                const Calculator calculator{20};
                int value = 1;
                auto bound = UI::Bind(&calculator, &Calculator::Add, UI::BindDynamic(value));

                AssertEqual(bound.apply(), 21, "Const method bindings should work with pointers to const objects");
                AssertEqual(calculator.calls, 1, "Const method should execute once");

                value = 2;
                Assert(bound.isDirty(), "Changing a dynamic value should mark a const method binding dirty");
                AssertEqual(bound.apply(), 22, "Const method binding should recompute after a change");
                AssertEqual(calculator.calls, 2, "Const method should execute again after recomputing");
            });

    return suite;
}

} // namespace tests
