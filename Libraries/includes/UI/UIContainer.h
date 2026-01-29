#pragma once
#include "UIComponent.h"

class UIContainer : public UIComponent {
protected:
    std::vector<std::shared_ptr<UIComponent>> children;

    // FBO pour cache
    GLuint fbo = 0;
    GLuint fboTexture = 0;
    int fboWidth = 0, fboHeight = 0;

public:
    template<typename T, typename... Args>
    T* AddChild(Args&&... args) {
        auto child = std::make_shared<T>(std::forward<Args>(args)...);
        child->SetParent(shared_from_this());
        T* ptr = child.get();
        children.push_back(std::move(child));
        MarkDirty();
        return ptr;
    }

    void Draw(int screenW, int screenH) override;
    void HandleInput(InputManager* input, int screenW, int screenH) override;

private:
    void InitFBO(int w, int h);
    void RenderToFBO(int screenW, int screenH);
};



class UIHBox : public UIContainer {
    float spacing = 5.0f;
    bool wrap = true;

public:
    void ArrangeChildren();
    void Draw(int screenW, int screenH) override;
};


class UIVBox : public UIContainer {
    float spacing = 5.0f;

public:
    void ArrangeChildren();
};

class UIScrollView : public UIVBox {
    float scrollOffset = 0;
    float contentHeight = 0;
    float viewportHeight = 0;
    bool showScrollbar = true;

public:
    void HandleInput(InputManager* input, int screenW, int screenH) override;
    void Draw(int screenW, int screenH) override;

    void ScrollTo(float offset);
    void ScrollBy(float delta);
};





class UIPage : public UIContainer {
public:
    virtual void OnEnter() {}
    virtual void OnExit() {}
};

class UIOverlay : public UIContainer {
    bool modal = true;
    glm::vec4 backdropColor = {0, 0, 0, 0.5f};

public:
    bool IsModal() const { return modal; }
    void DrawBackdrop(int w, int h);
};

class UIPanel : public UIContainer {
    bool collapsed = false;

public:
    void Toggle() { collapsed = !collapsed; MarkDirty(); }
};
