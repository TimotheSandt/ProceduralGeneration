#include "suites/Suites.h"

#include "UI/Core/Bounds.h"
#include "UI/Core/DeferredValue.h"
#include "UI/Widgets.h"

#include <memory>
#include <stdexcept>

using namespace UI;

namespace tests
{

TestSuite CreateUIFoundationSuite()
{
    TestSuite suite{"UI"};

    AddTest(suite, "fixed bounds return pixel size",
            []
            {
                Bounds bounds(120_px, 80_px);
                const glm::vec2 size = bounds.getPixelSize();
                AssertNear(size.x, 120.0, 1e-6, "Pixel width should stay unchanged");
                AssertNear(size.y, 80.0, 1e-6, "Pixel height should stay unchanged");
            });

    AddTest(suite, "percentage bounds use parent size",
            []
            {
                Bounds bounds(50_pct, 25_pct);
                const glm::vec2 size = bounds.getPixelSize(glm::vec2(400.0f, 200.0f));
                AssertNear(size.x, 200.0, 1e-6, "Percentage width should use the parent width");
                AssertNear(size.y, 50.0, 1e-6, "Percentage height should use the parent height");
            });

    AddTest(suite, "percentage bounds without parent throw",
            []
            {
                bool thrown = false;
                try
                {
                    Bounds bounds(50_pct, 50_pct);
                    (void)bounds.getPixelSize();
                }
                catch (const std::runtime_error &)
                {
                    thrown = true;
                }
                Assert(thrown, "Bounds without a parent size should throw when percentages are used");
            });

    AddTest(suite, "center anchor computes centered offset",
            []
            {
                Bounds bounds(100_px, 50_px, Anchor::CENTER);
                bounds.getPixelSize(glm::vec2(100.0f, 50.0f));
                const glm::vec2 offset = bounds.getAnchorOffset(glm::vec2(300.0f, 150.0f));
                AssertNear(offset.x, 100.0, 1e-6, "Center anchor should offset x to the middle");
                AssertNear(offset.y, 50.0, 1e-6, "Center anchor should offset y to the middle");
            });

    AddTest(suite, "hover detection uses the computed scale",
            []
            {
                Bounds bounds(100_px, 50_px);
                bounds.getPixelSize(glm::vec2(100.0f, 50.0f));
                Assert(bounds.isHover(glm::vec2(10.0f, 10.0f)), "Point inside the bounds should hover");
                Assert(!bounds.isHover(glm::vec2(120.0f, 10.0f)), "Point outside the bounds should not hover");
            });

    AddTest(suite, "deferred value applies queued updates",
            []
            {
                DeferredValue<int> value(5);
                value.Set(10);
                Assert(value.HasNewValue(), "Set should queue a new value");
                Assert(value.Apply(), "Apply should commit the queued value");
                AssertEqual(value.Get(), 10, "Apply should update the stored value");
            });

    AddTest(suite, "deferred value ignores identical values",
            []
            {
                DeferredValue<int> value(5);
                value.Set(5);
                Assert(!value.HasNewValue(), "Setting the same value should not queue an update");
            });

    AddTest(suite, "deferred value modify updates through callback",
            []
            {
                DeferredValue<int> value(5);
                value.Modify([](int &current) { current += 7; });
                Assert(value.Apply(), "Modify should queue an update");
                AssertEqual(value.Get(), 12, "Modify should update the stored value after apply");
            });

    AddTest(suite, "deferred value weak ptr equality is stable",
            []
            {
                auto owned = std::make_shared<int>(42);
                std::weak_ptr<int> weak = owned;
                DeferredValue<std::weak_ptr<int>> value(weak);
                value.Set(weak);
                Assert(!value.HasNewValue(), "Setting the same weak_ptr owner should not queue an update");
            });

    AddTest(suite, "label stores a fixed string",
            []
            {
                auto label = CreateLabel(Bounds(), "Score");
                AssertEqual(label->GetText(), "Score", "Label should store the initial text");

                label->SetText("Lives");
                AssertEqual(label->GetText(), "Lives", "Label should update when its string changes");
            });

    AddTest(suite, "text content composes static, values and methods",
            []
            {
                struct Sample
                {
                    int value = 34;

                    bool operator==(const Sample &) const = default;

                    int GetValue() const { return value; }
                };

                int score = 12;
                Sample sample;
                TextContent content("Score: ");
                content.AppendValue(&score).AppendText(" / ").AppendMethod(&sample, &Sample::GetValue);
                auto text = CreateText(Bounds(), std::move(content));

                AssertEqual(text->GetText(), "Score: 12 / 34", "Text should compose literals, bound values and methods");
                text->ClearDirty();

                score = 13;
                sample.value = 35;
                text->Update();
                AssertEqual(text->GetText(), "Score: 13 / 35", "Text should refresh when bound sources change");
                Assert(text->IsAppearanceDirty(), "Text should mark itself dirty when the composed string changes");
            });

    AddTest(suite, "text can be built directly from content arguments",
            []
            {
                struct Sample
                {
                    int value = 34;
                    int calls = 0;

                    bool operator==(const Sample &) const = default;

                    int GetValue()
                    {
                        ++calls;
                        return value;
                    }
                };

                int score = 12;
                Sample sample;
                Text text(Bounds(), "Score: ", &score, " / ", Bind(&sample, &Sample::GetValue));

                AssertEqual(text.GetText(), "Score: 12 / 34", "Text should build directly from constructor arguments");
                AssertEqual(sample.calls, 1, "Direct constructor should evaluate callable content once");

                score = 13;
                sample.value = 35;
                text.Update();
                AssertEqual(text.GetText(), "Score: 13 / 35", "Direct constructor should refresh when bound sources change");
                AssertEqual(sample.calls, 2, "Direct constructor should re-evaluate callable content after changes");
            });

    AddTest(suite, "text content refreshes bound methods when the object changes",
            []
            {
                struct Sample
                {
                    int value = 34;
                    int calls = 0;

                    bool operator==(const Sample &) const = default;

                    int GetValue()
                    {
                        ++calls;
                        return value;
                    }
                };

                Sample sample;
                TextContent content("Value: ");
                content.AppendMethod(&sample, &Sample::GetValue);

                AssertEqual(content.BuildText(), "Value: 34", "Method parts should be evaluated when building text");
                AssertEqual(sample.calls, 1, "Method parts should run once for the initial build");
                AssertEqual(content.BuildText(), "Value: 34", "Cached method parts should reuse the previous text");
                AssertEqual(sample.calls, 1, "Cached method parts should not run again");

                sample.value = 35;
                Assert(content.IsDirty(), "Changing the bound object should mark the text content dirty");
                AssertEqual(content.BuildText(), "Value: 35", "Method parts should refresh when the object changes");
                AssertEqual(sample.calls, 2, "Method parts should run again after the object changes");
                Assert(!content.IsDirty(), "Text content should be clean again after rebuilding");
            });

    AddTest(suite, "text content refreshes callable parts",
            []
            {
                int score = 12;
                int calls = 0;

                TextContent content("Score: ");
                content.AppendValue(&score).AppendText(" / ").AppendFunction([&]() {
                    ++calls;
                    return score * 2;
                });

                AssertEqual(content.BuildText(), "Score: 12 / 24", "Callable parts should be evaluated when building text");
                AssertEqual(calls, 1, "Callable parts should run once for the initial build");

                score = 13;
                Assert(content.IsDirty(), "Changing a dynamic value should mark the text content dirty");
                AssertEqual(content.BuildText(), "Score: 13 / 26", "Callable parts should refresh when sources change");
                AssertEqual(calls, 2, "Callable parts should run again after the source changes");
                Assert(content.IsDirty(), "Callable parts without tracked inputs remain dynamic");
            });

    return suite;
}

} // namespace tests
