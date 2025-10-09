#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

enum MouseButton {
    None = 0,
    Left,
    Right,
    Middle,
    X1,
    X2,
    X3,
    X4,
    X5,
};

enum KeyLayout {
    UNDEFINED = 0,
    QWERTY,
    QWERTZ,
    AZERTY,
    DVORAK
};

enum KeyButton {
    None = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space, Enter, Tab, Escape, Backspace, Insert, Delete, 
    Home, End, PageUp, PageDown, 
    Left, Right, Up, Down,
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Comma, Period, Slash,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDecimal, NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadEnter
};

enum KeyModifier {
    None = 0, 
    CapsLock, NumLock, ScrollLock, 
    LeftShift, LeftControl, LeftAlt, LeftSuper, 
    RightShift, RightControl, RightAlt, RightSuper
};

enum InputState {
    Release, // Key not pressed
    Pressed, // Key pressed
    PressBegin,  // Key was just pressed
    Clicked, // Key was just released
};

struct KeyEvent {
    InputState state;
    KeyButton key;
};

struct KeyModifierEvent {
    InputState state;
    KeyModifier modifier;
};

struct MouseButtonEvent {
    InputState state;
    MouseButton button;
};

struct MouseMoveData {
    double x, y;
    double deltaX, deltaY;
};

// struct MouseScrollData {
//     double scrollX, scrollY;
// };

enum EventType {
    eKey           = 0b00001,
    eKeyModifier   = 0b00010,
    eMouseButton   = 0b00100,
    eMouseMove     = 0b01000,
    // eMouseScroll   = 0b10000
};

struct InputEvent {
    int type;
    std::vector<KeyEvent> keyEvents;
    std::vector<KeyModifierEvent> keyModifierEvents;
    std::vector<MouseButtonEvent> mouseButtonEvents;
    MouseMoveData mouseMoveEvents;
    // MouseScrollData mouseScrollEvents;
};


class InputManager {

private:
    InputManager(GLFWwindow* window);
    ~InputManager();
    
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

public:
    static InputManager& GetInstance(GLFWwindow* window);


    void Init();

    void Update();

public:    
    InputState GetKeyState(KeyButton key) const { 
        if (keyStateMap.find(key) == keyStateMap.end()) return InputState::Release;
        return keyStateMap.at(key); 
    }
    bool IsKeyRelease(KeyButton key) const { return GetKeyState(key) == InputState::Release; }
    bool IsKeyPress(KeyButton key) const { return GetKeyState(key) == InputState::Pressed; }
    bool IsKeyPressBegin(KeyButton key) const { return GetKeyState(key) == InputState::PressBegin; }
    bool IsKeyClicked(KeyButton key) const { return GetKeyState(key) == InputState::Clicked; }

    InputState GetKeyModifierState(KeyModifier modifier) const {
        if (keyModifierStateMap.find(modifier) == keyModifierStateMap.end()) return InputState::Release;
        return keyModifierStateMap.at(modifier); 
    }
    bool IsKeyModifierRelease(KeyModifier modifier) const { return GetKeyModifierState(modifier) == InputState::Release; }
    bool IsKeyModifierPress(KeyModifier modifier) const { return GetKeyModifierState(modifier) == InputState::Pressed; }
    bool IsKeyModifierPressBegin(KeyModifier modifier) const { return GetKeyModifierState(modifier) == InputState::PressBegin; }
    bool IsKeyModifierClicked(KeyModifier modifier) const { return GetKeyModifierState(modifier) == InputState::Clicked; }

    InputState GetMouseButtonState(MouseButton button) const { 
        if (mouseButtonStateMap.find(button) == mouseButtonStateMap.end()) return InputState::Release;
        return mouseButtonStateMap.at(button); 
    }
    bool IsMouseButtonRelease(MouseButton button) const { return GetMouseButtonState(button) == InputState::Release; }
    bool IsMouseButtonPress(MouseButton button) const { return GetMouseButtonState(button) == InputState::Pressed; }
    bool IsMouseButtonPressBegin(MouseButton button) const { return GetMouseButtonState(button) == InputState::PressBegin; }
    bool IsMouseButtonClicked(MouseButton button) const { return GetMouseButtonState(button) == InputState::Clicked; }


    double GetMouseX() const { return mouseX; }
    double GetMouseY() const { return mouseY; }

    double GetMouseDeltaX() const { return mouseDeltaX; }
    double GetMouseDeltaY() const { return mouseDeltaY; }

    InputEvent GetCurrentInputEvent() const { return inputEvent; };
    bool IsCurrentInputEventEmpty(); 

private:
    void UpdateInputEvent();

private:
    GLFWwindow* window;

    InputEvent inputEvent;
    
    std::unordered_map<KeyButton, InputState> keyStateMap;
    std::unordered_map<KeyModifier, InputState> keyModifierStateMap;
    std::unordered_map<MouseButton, InputState> mouseButtonStateMap;
    
    double mouseDeltaX = 0.0f, mouseDeltaY = 0.0f;
    double mouseX = 0.0f, mouseY = 0.0f;
    // TODO: Scroll

public:
    std::unordered_map<std::string, InputEvent> mapActionToInputEvent; // Maps action to an InputEvent, e.g. "MoveForward" to an InputEvent containing the W key pressed.

    InputEvent GetInputEventOfAction(std::string action) { return mapActionToInputEvent[action]; }
    void BindActionToInputEvent(std::string action, InputEvent event) { mapActionToInputEvent[action] = event; }
    bool IsActionBound(std::string action) { return mapActionToInputEvent.find(action) != mapActionToInputEvent.end(); }
    
    bool IsActionActive(std::string action);


    
   
public:
    static void AutoDetectKeyLayout();
    static void SetKeyLayout(KeyLayout layout);

private:
    static void RemoveInputManager(GLFWwindow* window) { inputManagers.erase(window); }

private:
    static KeyLayout keyLayout;
    
    static std::unordered_map<KeyButton, GLint> keyMap;
    static std::unordered_map<KeyModifier, GLint> KeyModifierMap;
    static std::unordered_map<MouseButton, GLint> mouseButtonMap;

    static std::unordered_map<GLFWwindow*, std::unique_ptr<InputManager>> inputManagers;
};