#pragma once
#include "UIComponent.h"
#include <functional>

class UILabel : public UIComponent {
    std::string text;
    float fontSize = 1.0f;
    TextAlign align = TextAlign::LEFT;

public:
    UILabel(const std::string& text, float x, float y);

    void SetText(const std::string& t) { text = t; MarkAppearanceDirty(); }
    void Draw(int screenW, int screenH) override;
};



class UIButton : public UIComponent {
    enum class State { Normal, Hovered, Pressed, Disabled };

    std::string label;
    State state = State::Normal;
    std::function<void()> onClick;

public:
    UIButton(const std::string& label, float x, float y, float w, float h);

    void SetCallback(std::function<void()> cb) { onClick = cb; }
    void SetEnabled(bool enabled);

    void HandleInput(InputManager* input, int w, int h) override;
    void Draw(int screenW, int screenH) override;
};


class UISlider : public UIComponent {
    float value = 0.5f;
    float minVal = 0, maxVal = 1;
    bool dragging = false;
    std::function<void(float)> onChange;

public:
    UISlider(float x, float y, float w, float h);

    float GetValue() const { return value; }
    void SetValue(float v);
    void SetRange(float min, float max);
    void SetCallback(std::function<void(float)> cb) { onChange = cb; }

    void HandleInput(InputManager* input, int w, int h) override;
    void Draw(int screenW, int screenH) override;
};

class UIInput : public UIComponent {
    std::string text;
    int cursorPos = 0;
    int selStart = -1, selEnd = -1;
    bool multiLine = false;
    std::string placeholder;
    std::function<void(const std::string&)> onChange;

public:
    UIInput(float x, float y, float w, float h);

    std::string GetText() const { return text; }
    void SetText(const std::string& t);
    void SetPlaceholder(const std::string& p) { placeholder = p; }
    void SetMultiLine(bool ml) { multiLine = ml; }

    void HandleInput(InputManager* input, int w, int h) override;
    void Draw(int screenW, int screenH) override;

private:
    void InsertText(const std::string& t);
    void DeleteSelection();
    void Copy();
    void Paste();
    void SelectAll();
};
