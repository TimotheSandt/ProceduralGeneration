#include "suites/Suites.h"

#include "UI/Core/Bounds.h"
#include "UI/Core/DeferredValue.h"

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

    return suite;
}

} // namespace tests
