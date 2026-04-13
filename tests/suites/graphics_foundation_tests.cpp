#include "suites/Suites.h"

#include "Graphics/Camera.h"

#include <glm/geometric.hpp>

namespace tests
{

TestSuite CreateGraphicsFoundationSuite()
{
    TestSuite suite{"GraphicsFoundation"};

    AddTest(suite, "camera move adds delta to position",
            []
            {
                Camera camera;
                camera.SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
                camera.Move(glm::vec3(2.0f, -1.0f, 0.5f));

                const glm::vec3 position = camera.GetPosition();
                AssertNear(position.x, 3.0, 1e-6, "Move should add the x delta");
                AssertNear(position.y, 1.0, 1e-6, "Move should add the y delta");
                AssertNear(position.z, 3.5, 1e-6, "Move should add the z delta");
            });

    AddTest(suite, "camera move forward uses orientation",
            []
            {
                Camera camera;
                camera.SetPosition(glm::vec3(0.0f));
                camera.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
                camera.MoveForward(2.0f);

                const glm::vec3 position = camera.GetPosition();
                AssertNear(position.x, 0.0, 1e-6, "MoveForward should not change x when facing forward");
                AssertNear(position.y, 0.0, 1e-6, "MoveForward should not change y when facing forward");
                AssertNear(position.z, -2.0, 1e-6, "MoveForward should move along orientation");
            });

    AddTest(suite, "camera move right uses right vector",
            []
            {
                Camera camera;
                camera.SetPosition(glm::vec3(0.0f));
                camera.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
                camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
                camera.MoveRight(3.0f);

                const glm::vec3 position = camera.GetPosition();
                AssertNear(position.x, 3.0, 1e-6, "MoveRight should move on the positive x axis for the default camera");
                AssertNear(position.y, 0.0, 1e-6, "MoveRight should not change y for the default camera");
                AssertNear(position.z, 0.0, 1e-6, "MoveRight should not change z for the default camera");
            });

    AddTest(suite, "camera yaw rotates orientation around up",
            []
            {
                Camera camera;
                camera.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
                camera.RotateYaw(90.0f);

                const glm::vec3 orientation = camera.GetOrientation();
                AssertNear(orientation.x, -1.0, 1e-4, "Positive yaw should rotate the default forward vector toward negative x");
                AssertNear(orientation.y, 0.0, 1e-4, "Yaw should preserve the vertical component");
                AssertNear(orientation.z, 0.0, 1e-4, "Positive yaw should rotate the default forward vector off the z axis");
            });

    AddTest(suite, "camera rotate applies pitch yaw and roll incrementally",
            []
            {
                Camera camera;
                camera.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
                camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
                camera.Rotate(glm::vec3(15.0f, 20.0f, 10.0f));

                const glm::vec3 orientation = camera.GetOrientation();
                const glm::vec3 up = camera.GetUp();

                AssertNear(glm::length(orientation), 1.0, 1e-4, "Rotate should keep the orientation normalized");
                AssertNear(glm::length(up), 1.0, 1e-4, "Rotate should keep the up vector normalized");
            });

    return suite;
}

} // namespace tests
