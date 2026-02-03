#include "UIContainer.h"
#include "utilities.h"
#include <glad/glad.h>
#include "Logger.h"

namespace UI {

UIContainer::UIContainer(Bounds bounds)
    : UIComponent(bounds) {
    contentSize = localBounds.scale;
    // Use container-specific shader with texture and scroll support
    this->mesh.SetShader(
        GET_RESOURCE_PATH("shader/UI/container.vert"),
        GET_RESOURCE_PATH("shader/UI/container.frag")
    );
}

void UIContainer::AddChild(std::shared_ptr<UIComponent> child) {
    children.push_back(child);
    child->SetParent(std::static_pointer_cast<UIContainer>(shared_from_this()));
}


void UIContainer::Initialize() {
    UIComponent::Initialize();
    CalculateContentSize();
    InitializedFBO();
    for (auto& child : children) child->Initialize();
}

void UIContainer::Update() {
    UIComponent::Update();
    for (auto& child : children) child->Update();
    CalculateContentSize();
    InitializedFBO();
}


// FBO helper functions
void UIContainer::InitializedFBO() {
    if (fbo.GetWidth() != static_cast<int>(contentSize.x) ||
        fbo.GetHeight() != static_cast<int>(contentSize.y)) {

        if (contentSize.x > 0 && contentSize.y > 0) {
            fbo.Init(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
            fboInitialized = true;
            GL_CHECK_ERROR_M("UIContainer FBO Init");

            // Clear FBO to transparent immediately after init
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            fbo.Bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, currentFBO);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

            // Force re-render on next frame
            MarkFullDirty();
        }
    }
}

void SaveFBOState(GLint& oldFBO, GLint viewport[4]) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    glGetIntegerv(GL_VIEWPORT, viewport);
}

void RestoreFBOState(GLint oldFBO, GLint viewport[4]) {
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}


void UIContainer::ClearZone(glm::vec4 bounds) {
    glEnable(GL_SCISSOR_TEST);
    // Flip Y for OpenGL (Bottom-Left origin)
    // Bounds are (x, y, w, h) in Top-Left origin
    GLint yGl = static_cast<GLint>(contentSize.y - (bounds.y + bounds.w));

    glScissor(
        static_cast<GLint>(bounds.x),
        yGl,
        static_cast<GLsizei>(bounds.z),
        static_cast<GLsizei>(bounds.w)
    );
    // Clear to transparent (or background color?)
    // Usually we want to clear to 0,0,0,0 so we can re-draw
    // But if we have background, we might need to clear to background?
    // Since we composite later, clearing to 0 is likely correct for "dirty" region update
    // assuming we redraw everything in that region.
    // However, if we only redraw the child, we lose the container background behind it?
    // Wait. If container has background, clearing child zone clears the background too.
    // We strictly should only clear if we re-render everything in that rect.
    // But RenderChildren only calls child->Draw().
    // If Child Draw doesn't fill the rect (e.g. rounded corners), we get holes.
    // For now, let's assume correcting the rect is step 1.
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}


void UIContainer::RenderChildren() {
    GLint oldFBO;
    GLint viewport[4];
    SaveFBOState(oldFBO, viewport);

    fbo.Bind();
    GL_CHECK_ERROR_M("RenderDirtyChildren Bind");

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Only render dirty children
    for (auto& child : children) {
        if (child->IsDirty()) {
            glm::vec4 childBounds = child->GetCachedBoundsInParent();
            ClearZone(childBounds);

            // Render the child with its offset (x, y) from cached bounds
            // Note: cached bounds are (x, y, w, h)
            child->Draw(contentSize, {childBounds.x, childBounds.y});
            child->ClearDirty();
            GL_CHECK_ERROR_M("RenderDirtyChildren Child Draw");
        }
    }

    RestoreFBOState(oldFBO, viewport);
    GL_CHECK_ERROR_M("RenderDirtyChildren Restore");
}


void UIContainer::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    // Update our pixel size based on parent
    GetPixelSize();
    RecalculateChildBounds();

    // Calculate container's position with anchor
    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);
    glm::vec2 myOffset = offset + anchorOffset;

    RenderChildren();

    // Draw the FBO texture
    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(myOffset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
    GLint texSamplerLoc = 0;
    mesh.InitUniform1i("textureSampler", &texSamplerLoc);

    mesh.Draw();

    mesh.UnbindVAO();
    mesh.UnbindShader();


    ClearDirty();
}


void UIContainer::MarkFullDirty() {
    MarkDirty();
    for (auto& child : children) {
        child->MarkDirty();
    }
    CalculateContentSize();
    NotifyParentDirty();
}

void UIContainer::RecalculateChildBounds() {
    float p = GetPadding();
    // Default implementation: stack children at origin + padding
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({p, p, childSize.x, childSize.y});
    }
}

void UIContainer::CalculateContentSize() {
    contentSize.x = localBounds.scale.x;
    contentSize.y = localBounds.scale.y;
}

}
