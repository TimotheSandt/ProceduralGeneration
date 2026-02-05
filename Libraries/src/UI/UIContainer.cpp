#include "UIContainer.h"
#include "utilities.h"
#include <glad/glad.h>
#include "Logger.h"

namespace UI {

UIContainerBase::UIContainerBase(Bounds bounds)
    : UIComponentBase(bounds) {
    contentSize = localBounds.scale;
    // Use container-specific shader with texture and scroll support
    this->mesh.SetShader(
        GET_RESOURCE_PATH("shader/UI/container.vert"),
        GET_RESOURCE_PATH("shader/UI/container.frag")
    );
    UpdateTheme();
}

void UIContainerBase::AddChild(std::shared_ptr<UIComponentBase> child) {
    children.push_back(child);
    child->SetParent(std::static_pointer_cast<UIContainerBase>(shared_from_this()));
}


void UIContainerBase::Initialize() {
    UIComponentBase::Initialize();
    CalculateContentSize();
    InitializedFBO();
    for (auto& child : children) child->Initialize();
}

void UIContainerBase::Update() {
    UIComponentBase::Update();

    // Apply deferred layout properties
    bool layoutChanged = false;
    if (padding.Apply()) layoutChanged = true;
    if (spacing.Apply()) layoutChanged = true;

    if (layoutChanged) {
        MarkFullDirty();
    }

    for (auto& child : children) child->Update();
    CalculateContentSize();
    InitializedFBO();
}


// FBO helper functions
void UIContainerBase::InitializedFBO() {
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


void UIContainerBase::ClearZone(glm::vec4 bounds) {
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
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

void UIContainerBase::RenderChildren() {
    GLint oldFBO;
    GLint viewport[4];
    SaveFBOState(oldFBO, viewport);

    fbo.Bind();
    GL_CHECK_ERROR_M("RenderDirtyChildren Bind");

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Determine dirty level: layout vs appearance only
    bool hasLayoutDirty = IsSelfLayoutDirty(); // If we resized, we must re-render all (anchors changed)
    bool hasAppearanceDirty = false;
    for (auto& child : children) {
        if (child->IsSelfLayoutDirty() || child->IsChildLayoutDirty()) {
            hasLayoutDirty = true;
            break;
        }
        if (child->IsAppearanceDirty()) {
            hasAppearanceDirty = true;
        }
    }

    if (hasLayoutDirty) {
        // Layout changed: clear entire FBO and re-render all
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        for (auto& child : children) {
            glm::vec4 childBounds = child->GetCachedBoundsInParent();
            child->Draw(contentSize, {childBounds.x, childBounds.y});
            child->ClearDirty();
        }
    } else if (hasAppearanceDirty) {
        // Appearance only: zone clear and re-render dirty children
        for (auto& child : children) {
            if (child->IsAppearanceDirty()) {
                glm::vec4 childBounds = child->GetCachedBoundsInParent();
                ClearZone(childBounds);
                child->Draw(contentSize, {childBounds.x, childBounds.y});
                child->ClearDirty();
            }
        }
    }

    RestoreFBOState(oldFBO, viewport);
    GL_CHECK_ERROR_M("RenderDirtyChildren Restore");
}


void UIContainerBase::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible.Get()) return;

    // Update child positions
    RecalculateChildBounds();

    // offset already includes anchor offset from cachedBoundsInParent
    RenderChildren();

    // Draw the FBO texture
    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color.Get()));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
    GLint texSamplerLoc = 0;
    mesh.InitUniform1i("textureSampler", &texSamplerLoc);

    mesh.Draw();

    mesh.UnbindVAO();
    mesh.UnbindShader();


    ClearDirty();
}


void UIContainerBase::MarkFullDirty() {
    MarkSelfLayoutDirty();
    for (auto& child : children) {
        child->MarkFullDirty();
    }
    CalculateContentSize();
    NotifyParentChildLayoutDirty();
}

void UIContainerBase::RecalculateChildBounds() {
    float p = GetPadding();
    // Default implementation: stack children at origin + padding, with anchor offset
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        // Calculate anchor offset based on contentSize
        glm::vec2 anchorOffset = child->GetAnchorOffset(contentSize);
        float x = p + anchorOffset.x;
        float y = p + anchorOffset.y;
        child->SetCachedBoundsInParent({x, y, childSize.x, childSize.y});
    }
}

void UIContainerBase::CalculateContentSize() {
    contentSize.x = localBounds.scale.x;
    contentSize.y = localBounds.scale.y;
}


void UIContainerBase::UpdateTheme() {
    UIComponentBase::UpdateTheme();
    if (auto t = theme.lock()) {
        padding.ForceSet(t->GetPadding());
        spacing.ForceSet(t->GetSpacing());
    }
}

void UIContainerBase::DoSetPadding(float p) {
    padding.Set(p);
}

void UIContainerBase::DoSetSpacing(float s) {
    spacing.Set(s);
}

}
