#include "UIContainer.h"
#include "utilities.h"
#include <glad/glad.h>
#include "Logger.h"

namespace UI {

UIContainerBase::UIContainerBase(Bounds bounds)
    : UIComponentBase(bounds) {
    // Ne PAS initialiser contentSize ici car scale n'est pas encore calculé
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

    if (auto t = theme.Get().lock()) {
        child->DoSetTheme(t);
    }
}


void UIContainerBase::Initialize() {
    // FIX CRITIQUE: Ordre d'initialisation correct
    // 1. Initialiser le composant de base (calcule localBounds.scale)
    UIComponentBase::Initialize();

    // 2. Calculer notre contentSize (utilise localBounds.scale qui est maintenant valide)
    CalculateContentSize();

    // 3. Initialiser le FBO avec la bonne taille
    InitializedFBO();

    // 4. MAINTENANT initialiser les enfants (ils peuvent calculer en % de notre taille)
    for (auto& child : children) {
        child->Initialize();
    }

    // 5. Recalculer les positions des enfants après leur initialisation
    RecalculateChildBounds();

    // 6. Forcer un rendu complet initial
    MarkFullDirty(false);
}

void UIContainerBase::Update() {
    if (theme.Apply()) {
        UpdateTheme();
    }

    // FIX: Utiliser |= pour accumuler les changements
    bool needsLayoutUpdate = false;
    needsLayoutUpdate = needsLayoutUpdate || padding.Apply();
    needsLayoutUpdate = needsLayoutUpdate || spacing.Apply();

    if (needsLayoutUpdate) {
        dirtyLayout = true;
    }

    if (scrollOffset.Apply()) MarkDirty();

    // FIX: Si contentSize change, réinitialiser le FBO
    if (contentSize.Apply()) {
        InitializedFBO();
        dirtyLayout = true;
    }

    UIComponentBase::Update();

    // Update children AFTER parent update
    for (auto& child : children) {
        child->Update();
    }
}


// FBO helper functions
void UIContainerBase::InitializedFBO() {
    glm::vec2 size = contentSize.Get();

    // FIX: Vérifier que la taille est valide
    if (size.x <= 0 || size.y <= 0) {
        LOG_WARNING("Cannot initialize FBO with invalid size: (", size.x, ", ", size.y, ")");
        return;
    }

    if (fbo.GetWidth() != static_cast<int>(size.x) ||
        fbo.GetHeight() != static_cast<int>(size.y)) {

        fbo.Init(static_cast<int>(size.x), static_cast<int>(size.y));
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

void SaveFBOState(GLint& oldFBO, GLint viewport[4]) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    glGetIntegerv(GL_VIEWPORT, viewport);
}

void RestoreFBOState(GLint oldFBO, GLint viewport[4]) {
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void UIContainerBase::ClearFBOFrom(int indexChildToClear) {
    if (!fboInitialized) return;
    UNREFERENCED_PARAMETER(indexChildToClear);
    fbo.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fbo.Unbind();
}


void UIContainerBase::ClearZone(glm::vec4 bounds) {
    glEnable(GL_SCISSOR_TEST);
    // Flip Y for OpenGL (Bottom-Left origin)
    // Bounds are (x, y, w, h) in Top-Left origin
    GLint yGl = static_cast<GLint>(contentSize.Get().y - (bounds.y + bounds.w));

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
    if (!fboInitialized) {
        LOG_WARNING("Attempting to render children without initialized FBO");
        return;
    }

    GLint oldFBO;
    GLint viewport[4];
    SaveFBOState(oldFBO, viewport);

    fbo.Bind();
    GL_CHECK_ERROR_M("RenderDirtyChildren Bind");

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.Get().x), static_cast<GLsizei>(contentSize.Get().y));

    // Only render dirty children
    for (auto& child : children) {
        if (child->IsDirty()) {
            glm::vec4 childBounds = child->GetCachedBoundsInParent();
            ClearZone(childBounds);

            // Render the child with its offset (x, y) from cached bounds
            child->Draw({childBounds.x, childBounds.y});
            child->ClearDirty();
            GL_CHECK_ERROR_M("RenderDirtyChildren Child Draw");
        }
    }

    RestoreFBOState(oldFBO, viewport);
    GL_CHECK_ERROR_M("RenderDirtyChildren Restore");
}


void UIContainerBase::Draw(glm::vec2 offset) {
    if (!visible.Get()) return;

    // Calculate container's position with anchor
    glm::vec2 containerSize;
    if (auto p = parent.Get().lock()) {
        containerSize = p->localBounds.Get().scale;
    } else {
        containerSize = localBounds.Get().scale;
    }
    glm::vec2 anchorOffset = localBounds.Get().getAnchorOffset(containerSize);
    glm::vec2 myOffset = offset + anchorOffset;

    RenderChildren();

    // Draw the FBO texture
    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(myOffset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.Get().scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset.Get()));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize.Get()));
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


void UIContainerBase::MarkFullDirty(bool propagate) {
    MarkDirty();
    for (auto& child : children) {
        child->MarkFullDirty(false);
    }
    ClearFBOFrom();
    if (propagate) NotifyParentDirty();
}

void UIContainerBase::UpdateLayout() {
    UIComponentBase::UpdateLayout();
    CalculateContentSize();
    RecalculateChildBounds();
}

void UIContainerBase::RecalculateChildBounds() {
    float p = GetPadding();
    // Default implementation: stack children at origin + padding
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({p, p, childSize.x, childSize.y});
    }
}

void UIContainerBase::CalculateContentSize() {
    contentSize.ModifyForce([&](glm::vec2& c) {
        c = {
            localBounds.Get().scale.x,
            localBounds.Get().scale.y
        };
    });
}


void UIContainerBase::UpdateTheme() {
    UIComponentBase::UpdateTheme();
    if (auto t = theme.Get().lock()) {
        // FIX: Ne pas toucher dirtyLayout ici
        padding.ForceSet(t->GetPadding());
        spacing.ForceSet(t->GetSpacing());
    }
}

void UIContainerBase::DoSetTheme(std::weak_ptr<UITheme> t) {
    UIComponentBase::DoSetTheme(t);
    for (auto& child : children) {
        child->DoSetTheme(t);
    }
}

void UIContainerBase::DoSetPadding(float p) {
    padding.Set(p);
}

void UIContainerBase::DoSetSpacing(float s) {
    spacing.Set(s);
}

}
