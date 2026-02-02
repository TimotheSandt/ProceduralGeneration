#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>


enum class InputState {
    Release, // Key not pressed
    Pressed, // Key pressed
    PressBegin,  // Key was just pressed
    PressEnd, // Key was just released
};

enum class MouseButton {
    LEFT,
    RIGHT,
    MIDDLE,
    X1,
    X2,
    X3,
    X4,
    X5,
};

struct MouseMoveData {
    glm::vec2 position;
    glm::vec2 delta;
    glm::vec2 scroll;
};

enum class KeyLayout {
    UNDEFINED = 0,
    QWERTY,
    QWERTZ,
    AZERTY,
    DVORAK
};

enum class KeyButton {
    // Alphabetic
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Function
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
    // Numeric
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
    // Special
    SPACE, ENTER, TAB, ESCAPE, BACKSPACE, INSERT, DELETE,
    HOME, END, PAGE_UP, PAGE_DOWN,
    LEFT, RIGHT, UP, DOWN,
    MINUS, EQUALS, LEFT_BRACKET, RIGHT_BRACKET, BACKSLASH, SEMICOLON, APOSTROPHE, COMMA, PERIOD, SLASH,
    NUMPAD_0, NUMPAD_1, NUMPAD_2, NUMPAD_3, NUMPAD_4, NUMPAD_5, NUMPAD_6, NUMPAD_7, NUMPAD_8, NUMPAD_9,
    NUMPAD_DECIMAL, NUMPAD_DIVIDE, NUMPAD_MULTIPLY, NUMPAD_SUBTRACT, NUMPAD_ADD, NUMPAD_ENTER,
    // Modifiers
    CAPS_LOCK, NUM_LOCK, SCROLL_LOCK,
    LEFT_SHIFT, LEFT_CONTROL, LEFT_ALT, LEFT_SUPER,
    RIGHT_SHIFT, RIGHT_CONTROL, RIGHT_ALT, RIGHT_SUPER
};

struct KeyButtonEvent {
    InputState state;
    KeyButton key;
};

struct MouseButtonEvent {
    InputState state;
    MouseButton button;
};

struct InputEvent {
    std::vector<KeyButtonEvent> keys;
    std::vector<MouseButtonEvent> mouseButtons;
    MouseMoveData mouseMoveData;
};

struct InputAction {
    std::vector<KeyButton> keys;
    std::vector<MouseButton> mouseButtons;
};


class InputManager {

private:
    InputManager(GLFWwindow* window);
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;


    friend struct std::default_delete<InputManager>;

public:
    static InputManager& GetInstance(GLFWwindow* window);
    static void RemoveInstance(GLFWwindow* window) { inputManagers.erase(window); }

    void Init();
    void Update();

    static void AutoDetectKeyLayout();
    static void SetKeyLayout(KeyLayout layout);

#pragma region Getters
public:
    InputState GetKeyState(KeyButton key) const {
        if (keyStateMap.find(key) == keyStateMap.end()) return InputState::Release;
        return keyStateMap.at(key);
    }
    bool IsKeyRelease(KeyButton key) const { return GetKeyState(key) == InputState::Release; }
    bool IsKeyPressed(KeyButton key) const { return GetKeyState(key) == InputState::Pressed; }
    bool IsKeyJustPressed(KeyButton key) const { return GetKeyState(key) == InputState::PressBegin; }
    bool IsKeyJustReleased(KeyButton key) const { return GetKeyState(key) == InputState::PressEnd; }

    InputState GetMouseButtonState(MouseButton button) const {
        if (mouseButtonStateMap.find(button) == mouseButtonStateMap.end()) return InputState::Release;
        return mouseButtonStateMap.at(button);
    }
    bool IsMouseButtonRelease(MouseButton button) const { return GetMouseButtonState(button) == InputState::Release; }
    bool IsMouseButtonPressed(MouseButton button) const { return GetMouseButtonState(button) == InputState::Pressed; }
    bool IsMouseButtonJustPressed(MouseButton button) const { return GetMouseButtonState(button) == InputState::PressBegin; }
    bool IsMouseButtonJustReleased(MouseButton button) const { return GetMouseButtonState(button) == InputState::PressEnd; }

    glm::vec2 GetMousePosition() const { return mouseMoveData.position; }
    glm::vec2 GetMouseDelta() const { return mouseMoveData.delta; }
    glm::vec2 GetMouseScroll() const { return mouseMoveData.scroll; }

    InputEvent GetCurrentInputEvent() const { return inputEvent; };
    bool IsCurrentInputEventEmpty();

#pragma endregion Getters

private:
    void UpdateInputEvent();


#pragma region Actions
private:
    static InputAction _GetInputAction(InputAction inputAction, KeyButton key) {
        inputAction.keys.push_back(key);
        return inputAction;
    }
    static InputAction _GetInputAction(InputAction inputAction, MouseButton mouseButton) {
        inputAction.mouseButtons.push_back(mouseButton);
        return inputAction;
    }
    template<typename... Args, typename T>
    static InputAction _GetInputAction(InputAction inputAction, T arg, Args... args) {
        inputAction = _GetInputAction(inputAction, arg);
        return _GetInputAction(inputAction, args...);
    }

public:
    InputAction GetInputActionOfAction(std::string action) { return mapActionToInputAction[action]; }

    static void BindActionToInput(std::string action, InputAction inputAction);
    template<typename... Args>
    static void BindActionToInput(std::string action, Args... args) {
        InputAction inputAction = _GetInputAction(InputAction(), args...);
        BindActionToInput(action, inputAction);
    }

    static void UnbindAction(std::string action) { mapActionToInputAction.erase(action); }
    static bool IsActionBound(std::string action) { return mapActionToInputAction.find(action) != mapActionToInputAction.end(); }
    bool IsActionActive(std::string action);

#pragma endregion Actions

private:
    GLFWwindow* window;

    InputEvent inputEvent;
    std::unordered_map<KeyButton, InputState> keyStateMap;
    std::unordered_map<MouseButton, InputState> mouseButtonStateMap;
    MouseMoveData mouseMoveData;
    // TODO: Scroll

private:
    static std::unordered_map<std::string, InputAction> mapActionToInputAction;

    static KeyLayout keyLayout;

    static std::unordered_map<KeyButton, GLint> keyMap;
    static std::unordered_map<MouseButton, GLint> mouseButtonMap;

    static std::unordered_map<GLFWwindow*, std::unique_ptr<InputManager>> inputManagers;
};
