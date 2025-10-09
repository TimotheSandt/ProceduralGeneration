#include "InputManager.h"
#include "Logger.h"

#include <windows.h>
#include <functional>


KeyLayout InputManager::keyLayout = KeyLayout::UNDEFINED;
std::unordered_map<KeyButton, GLint> InputManager::keyMap;
std::unordered_map<KeyModifier, GLint> InputManager::KeyModifierMap;
std::unordered_map<MouseButton, GLint> InputManager::mouseButtonMap;

std::unordered_map<GLFWwindow*, std::unique_ptr<InputManager>> InputManager::inputManagers; 


InputManager::InputManager(GLFWwindow* window) : window(window) {
    Init();
}

InputManager::~InputManager() {
    RemoveInputManager(window);
}

InputManager& InputManager::GetInstance(GLFWwindow* window) {
    if (keyLayout == KeyLayout::UNDEFINED) {
        AutoDetectKeyLayout();
    }
    if (inputManagers.find(window) == inputManagers.end()) {
        inputManagers[window] = std::make_unique<InputManager>(window);
    }
    return *inputManagers[window];
}

void InputManager::Init() {
    // Keyboard
    for (auto it = keyMap.begin(); it != keyMap.end(); ++it) {
        keyStateMap[it->first] = InputState::Release;
    }
    for (auto it = KeyModifierMap.begin(); it != KeyModifierMap.end(); ++it) {
        keyModifierStateMap[it->first] = InputState::Release;
    }
    // Mouse
    for (auto it = mouseButtonMap.begin(); it != mouseButtonMap.end(); ++it) {
        mouseButtonStateMap[it->first] = InputState::Release;
    }

    // Mouse position
    glfwGetCursorPos(window, &this->mouseX, &this->mouseY);

    // Mouse delta
    mouseDeltaX = 0.0;
    mouseDeltaY = 0.0;
}

// Update method implementation
void InputManager::Update() {
    glfwPollEvents();

    // Keyboard
    for (auto it = keyStateMap.begin(); it != keyStateMap.end(); ++it) {
        bool isPressed = glfwGetKey(window, keyMap[it->first]) == GLFW_PRESS;
        it->second = isPressed ? 
            ((it->second == InputState::Release) ? 
                InputState::PressBegin : InputState::Pressed) : 
            ((it->second == InputState::Pressed || it->second == InputState::PressBegin) ? 
                InputState::Clicked : InputState::Release);
    }
    for (auto it = keyModifierStateMap.begin(); it != keyModifierStateMap.end(); ++it) {
        bool isPressed = glfwGetKey(window, KeyModifierMap[it->first]) == GLFW_PRESS;
        it->second = isPressed ? 
            ((it->second == InputState::Release) ? 
                InputState::PressBegin : InputState::Pressed) : 
            ((it->second == InputState::Pressed || it->second == InputState::PressBegin) ? 
                InputState::Clicked : InputState::Release);
    }

    // Mouse
    for (auto it = mouseButtonStateMap.begin(); it != mouseButtonStateMap.end(); ++it) {
        bool isPressed = glfwGetMouseButton(window, mouseButtonMap[it->first]) == GLFW_PRESS;
        it->second = isPressed ? 
            ((it->second == InputState::Release) ? 
                InputState::PressBegin : InputState::Pressed) : 
            ((it->second == InputState::Pressed || it->second == InputState::PressBegin) ? 
                InputState::Clicked : InputState::Release);
    }

    // Mouse Position
    double lastX = this->mouseX, lastY = this->mouseY;
    glfwGetCursorPos(window, &this->mouseX, &this->mouseY);

    this->mouseDeltaX = this->mouseX - lastX;
    this->mouseDeltaY = this->mouseY - lastY;
    UpdateInputEvent();
}



void InputManager::AutoDetectKeyLayout() {
    KeyLayout layout = KeyLayout::QWERTY;
#ifdef _WIN32
    #pragma comment(lib, "user32.lib")
    HKL keyboardLayout = GetKeyboardLayout(0);
    DWORD layoutID = LOWORD(reinterpret_cast<DWORD_PTR>(keyboardLayout)); 

    switch (layoutID) { // Use the LOWORD for the switch
    case 0x0409: // US English (Primary QWERTY)
        if (reinterpret_cast<UINT_PTR>(keyboardLayout) == 0x00010409) {
            layout = KeyLayout::DVORAK;
        } else {
            layout = KeyLayout::QWERTY;
        }
        break;

    case 0x040C: // French (AZERTY)
        layout = KeyLayout::AZERTY;
        break;

    case 0x0407: // German (QWERTZ)
        layout = KeyLayout::QWERTZ;
        break;

    default:
        layout = KeyLayout::QWERTY;
        break;
    }
#else
    layout = KeyLayout::QWERTY;
#endif

    InputManager::SetKeyLayout(layout);
}

#pragma region Key Layout Mapping
void InputManager::SetKeyLayout(KeyLayout layout) {
    InputManager::keyLayout = layout;
    InputManager::keyMap.clear();
    InputManager::KeyModifierMap.clear();
    InputManager::mouseButtonMap.clear();

    switch (layout) {
#pragma region AZERTY
    case KeyLayout::AZERTY:
        // Lettres AZERTY
        keyMap[KeyButton::A] = GLFW_KEY_Q;
        keyMap[KeyButton::B] = GLFW_KEY_B;
        keyMap[KeyButton::C] = GLFW_KEY_C;
        keyMap[KeyButton::D] = GLFW_KEY_D;
        keyMap[KeyButton::E] = GLFW_KEY_E;
        keyMap[KeyButton::F] = GLFW_KEY_F;
        keyMap[KeyButton::G] = GLFW_KEY_G;
        keyMap[KeyButton::H] = GLFW_KEY_H;
        keyMap[KeyButton::I] = GLFW_KEY_I;
        keyMap[KeyButton::J] = GLFW_KEY_J;
        keyMap[KeyButton::K] = GLFW_KEY_K;
        keyMap[KeyButton::L] = GLFW_KEY_L;
        keyMap[KeyButton::M] = GLFW_KEY_SEMICOLON;
        keyMap[KeyButton::N] = GLFW_KEY_N;
        keyMap[KeyButton::O] = GLFW_KEY_O;
        keyMap[KeyButton::P] = GLFW_KEY_P;
        keyMap[KeyButton::Q] = GLFW_KEY_A;
        keyMap[KeyButton::R] = GLFW_KEY_R;
        keyMap[KeyButton::S] = GLFW_KEY_S;
        keyMap[KeyButton::T] = GLFW_KEY_T;
        keyMap[KeyButton::U] = GLFW_KEY_U;
        keyMap[KeyButton::V] = GLFW_KEY_V;
        keyMap[KeyButton::W] = GLFW_KEY_Z;
        keyMap[KeyButton::X] = GLFW_KEY_X;
        keyMap[KeyButton::Y] = GLFW_KEY_Y;
        keyMap[KeyButton::Z] = GLFW_KEY_W;

        // Symboles AZERTY
        keyMap[KeyButton::Minus] = GLFW_KEY_MINUS;
        keyMap[KeyButton::Equals] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LeftBracket] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RightBracket] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::Semicolon] = GLFW_KEY_COMMA;
        keyMap[KeyButton::Comma] = GLFW_KEY_M;
        keyMap[KeyButton::Period] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::Slash] = GLFW_KEY_SLASH;
        keyMap[KeyButton::Backslash] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::Apostrophe] = GLFW_KEY_APOSTROPHE;
        break;
#pragma endregion AZERTY

#pragma region QWERTZ
    case KeyLayout::QWERTZ:
        // Lettres QWERTZ (seuls Y et Z sont échangés par rapport à QWERTY)
        keyMap[KeyButton::A] = GLFW_KEY_A;
        keyMap[KeyButton::B] = GLFW_KEY_B;
        keyMap[KeyButton::C] = GLFW_KEY_C;
        keyMap[KeyButton::D] = GLFW_KEY_D;
        keyMap[KeyButton::E] = GLFW_KEY_E;
        keyMap[KeyButton::F] = GLFW_KEY_F;
        keyMap[KeyButton::G] = GLFW_KEY_G;
        keyMap[KeyButton::H] = GLFW_KEY_H;
        keyMap[KeyButton::I] = GLFW_KEY_I;
        keyMap[KeyButton::J] = GLFW_KEY_J;
        keyMap[KeyButton::K] = GLFW_KEY_K;
        keyMap[KeyButton::L] = GLFW_KEY_L;
        keyMap[KeyButton::M] = GLFW_KEY_M;
        keyMap[KeyButton::N] = GLFW_KEY_N;
        keyMap[KeyButton::O] = GLFW_KEY_O;
        keyMap[KeyButton::P] = GLFW_KEY_P;
        keyMap[KeyButton::Q] = GLFW_KEY_Q;
        keyMap[KeyButton::R] = GLFW_KEY_R;
        keyMap[KeyButton::S] = GLFW_KEY_S;
        keyMap[KeyButton::T] = GLFW_KEY_T;
        keyMap[KeyButton::U] = GLFW_KEY_U;
        keyMap[KeyButton::V] = GLFW_KEY_V;
        keyMap[KeyButton::W] = GLFW_KEY_W;
        keyMap[KeyButton::X] = GLFW_KEY_X;
        keyMap[KeyButton::Y] = GLFW_KEY_Z;
        keyMap[KeyButton::Z] = GLFW_KEY_Y;

        // Symboles QWERTZ
        keyMap[KeyButton::Minus] = GLFW_KEY_MINUS;
        keyMap[KeyButton::Equals] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LeftBracket] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RightBracket] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::Semicolon] = GLFW_KEY_SEMICOLON;
        keyMap[KeyButton::Comma] = GLFW_KEY_COMMA;
        keyMap[KeyButton::Period] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::Slash] = GLFW_KEY_SLASH;
        keyMap[KeyButton::Backslash] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::Apostrophe] = GLFW_KEY_APOSTROPHE;
        break;
#pragma endregion QWERTZ

#pragma region DVORAK
    case KeyLayout::DVORAK:
        // Disposition Dvorak complète
        // Rangée supérieure: ',.PYFGCRL
        keyMap[KeyButton::Q] = GLFW_KEY_APOSTROPHE;  // Q logique -> ' physique
        keyMap[KeyButton::W] = GLFW_KEY_COMMA;        // W logique -> , physique
        keyMap[KeyButton::E] = GLFW_KEY_PERIOD;       // E logique -> . physique
        keyMap[KeyButton::R] = GLFW_KEY_P;            // R logique -> P physique
        keyMap[KeyButton::T] = GLFW_KEY_Y;            // T logique -> Y physique
        keyMap[KeyButton::Y] = GLFW_KEY_F;            // Y logique -> F physique
        keyMap[KeyButton::U] = GLFW_KEY_G;            // U logique -> G physique
        keyMap[KeyButton::I] = GLFW_KEY_C;            // I logique -> C physique
        keyMap[KeyButton::O] = GLFW_KEY_R;            // O logique -> R physique
        keyMap[KeyButton::P] = GLFW_KEY_L;            // P logique -> L physique

        // Rangée du milieu: AOEUIDHTNS
        keyMap[KeyButton::A] = GLFW_KEY_A;            // A reste A
        keyMap[KeyButton::S] = GLFW_KEY_O;            // S logique -> O physique
        keyMap[KeyButton::D] = GLFW_KEY_E;            // D logique -> E physique
        keyMap[KeyButton::F] = GLFW_KEY_U;            // F logique -> U physique
        keyMap[KeyButton::G] = GLFW_KEY_I;            // G logique -> I physique
        keyMap[KeyButton::H] = GLFW_KEY_D;            // H logique -> D physique
        keyMap[KeyButton::J] = GLFW_KEY_H;            // J logique -> H physique
        keyMap[KeyButton::K] = GLFW_KEY_T;            // K logique -> T physique
        keyMap[KeyButton::L] = GLFW_KEY_N;            // L logique -> N physique
        
        // Rangée inférieure: ;QJKXBM
        keyMap[KeyButton::Z] = GLFW_KEY_SEMICOLON;    // Z logique -> ; physique
        keyMap[KeyButton::X] = GLFW_KEY_Q;            // X logique -> Q physique
        keyMap[KeyButton::C] = GLFW_KEY_J;            // C logique -> J physique
        keyMap[KeyButton::V] = GLFW_KEY_K;            // V logique -> K physique
        keyMap[KeyButton::B] = GLFW_KEY_X;            // B logique -> X physique
        keyMap[KeyButton::N] = GLFW_KEY_B;            // N logique -> B physique
        keyMap[KeyButton::M] = GLFW_KEY_M;            // M reste M

        // Symboles Dvorak
        keyMap[KeyButton::Minus] = GLFW_KEY_LEFT_BRACKET;       // - logique -> [ physique
        keyMap[KeyButton::Equals] = GLFW_KEY_RIGHT_BRACKET;     // = logique -> ] physique
        keyMap[KeyButton::LeftBracket] = GLFW_KEY_SLASH;        // [ logique -> / physique
        keyMap[KeyButton::RightBracket] = GLFW_KEY_EQUAL;       // ] logique -> = physique
        keyMap[KeyButton::Semicolon] = GLFW_KEY_S;              // ; logique -> S physique
        keyMap[KeyButton::Apostrophe] = GLFW_KEY_MINUS;         // ' logique -> - physique
        keyMap[KeyButton::Comma] = GLFW_KEY_W;                  // , logique -> W physique
        keyMap[KeyButton::Period] = GLFW_KEY_V;                 // . logique -> V physique
        keyMap[KeyButton::Slash] = GLFW_KEY_Z;                  // / logique -> Z physique
        keyMap[KeyButton::Backslash] = GLFW_KEY_BACKSLASH;      // \ reste \
        break;
#pragma endregion DVORAK

#pragma region QWERTY
    case KeyLayout::QWERTY:
    default:
        // Lettres QWERTY (mapping direct)
        keyMap[KeyButton::A] = GLFW_KEY_A;
        keyMap[KeyButton::B] = GLFW_KEY_B;
        keyMap[KeyButton::C] = GLFW_KEY_C;
        keyMap[KeyButton::D] = GLFW_KEY_D;
        keyMap[KeyButton::E] = GLFW_KEY_E;
        keyMap[KeyButton::F] = GLFW_KEY_F;
        keyMap[KeyButton::G] = GLFW_KEY_G;
        keyMap[KeyButton::H] = GLFW_KEY_H;
        keyMap[KeyButton::I] = GLFW_KEY_I;
        keyMap[KeyButton::J] = GLFW_KEY_J;
        keyMap[KeyButton::K] = GLFW_KEY_K;
        keyMap[KeyButton::L] = GLFW_KEY_L;
        keyMap[KeyButton::M] = GLFW_KEY_M;
        keyMap[KeyButton::N] = GLFW_KEY_N;
        keyMap[KeyButton::O] = GLFW_KEY_O;
        keyMap[KeyButton::P] = GLFW_KEY_P;
        keyMap[KeyButton::Q] = GLFW_KEY_Q;
        keyMap[KeyButton::R] = GLFW_KEY_R;
        keyMap[KeyButton::S] = GLFW_KEY_S;
        keyMap[KeyButton::T] = GLFW_KEY_T;
        keyMap[KeyButton::U] = GLFW_KEY_U;
        keyMap[KeyButton::V] = GLFW_KEY_V;
        keyMap[KeyButton::W] = GLFW_KEY_W;
        keyMap[KeyButton::X] = GLFW_KEY_X;
        keyMap[KeyButton::Y] = GLFW_KEY_Y;
        keyMap[KeyButton::Z] = GLFW_KEY_Z;

        // Symboles QWERTY
        keyMap[KeyButton::Minus] = GLFW_KEY_MINUS;
        keyMap[KeyButton::Equals] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LeftBracket] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RightBracket] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::Semicolon] = GLFW_KEY_SEMICOLON;
        keyMap[KeyButton::Comma] = GLFW_KEY_COMMA;
        keyMap[KeyButton::Period] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::Slash] = GLFW_KEY_SLASH;
        keyMap[KeyButton::Backslash] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::Apostrophe] = GLFW_KEY_APOSTROPHE;
        break;
#pragma endregion QWERTY
    }

#pragma region All Layout Mapping
    // Modificateurs (identiques pour tous les layouts)
    KeyModifierMap[KeyModifier::LeftShift] = GLFW_KEY_LEFT_SHIFT;
    KeyModifierMap[KeyModifier::LeftControl] = GLFW_KEY_LEFT_CONTROL;
    KeyModifierMap[KeyModifier::LeftAlt] = GLFW_KEY_LEFT_ALT;
    KeyModifierMap[KeyModifier::LeftSuper] = GLFW_KEY_LEFT_SUPER;
    KeyModifierMap[KeyModifier::RightShift] = GLFW_KEY_RIGHT_SHIFT;
    KeyModifierMap[KeyModifier::RightControl] = GLFW_KEY_RIGHT_CONTROL;
    KeyModifierMap[KeyModifier::RightAlt] = GLFW_KEY_RIGHT_ALT;
    KeyModifierMap[KeyModifier::RightSuper] = GLFW_KEY_RIGHT_SUPER;
    KeyModifierMap[KeyModifier::CapsLock] = GLFW_KEY_CAPS_LOCK;
    KeyModifierMap[KeyModifier::NumLock] = GLFW_KEY_NUM_LOCK;
    KeyModifierMap[KeyModifier::ScrollLock] = GLFW_KEY_SCROLL_LOCK;

    // Navigation (identique pour tous les layouts)
    keyMap[KeyButton::Space] = GLFW_KEY_SPACE;
    keyMap[KeyButton::Enter] = GLFW_KEY_ENTER;
    keyMap[KeyButton::Tab] = GLFW_KEY_TAB;
    keyMap[KeyButton::Escape] = GLFW_KEY_ESCAPE;
    keyMap[KeyButton::Backspace] = GLFW_KEY_BACKSPACE;
    keyMap[KeyButton::Insert] = GLFW_KEY_INSERT;
    keyMap[KeyButton::Delete] = GLFW_KEY_DELETE;
    keyMap[KeyButton::Home] = GLFW_KEY_HOME;
    keyMap[KeyButton::End] = GLFW_KEY_END;
    keyMap[KeyButton::PageUp] = GLFW_KEY_PAGE_UP;
    keyMap[KeyButton::PageDown] = GLFW_KEY_PAGE_DOWN;
    keyMap[KeyButton::Left] = GLFW_KEY_LEFT;
    keyMap[KeyButton::Right] = GLFW_KEY_RIGHT;
    keyMap[KeyButton::Up] = GLFW_KEY_UP;
    keyMap[KeyButton::Down] = GLFW_KEY_DOWN;

    // Chiffres (identique pour tous les layouts)
    keyMap[KeyButton::Num0] = GLFW_KEY_0;
    keyMap[KeyButton::Num1] = GLFW_KEY_1;
    keyMap[KeyButton::Num2] = GLFW_KEY_2;
    keyMap[KeyButton::Num3] = GLFW_KEY_3;
    keyMap[KeyButton::Num4] = GLFW_KEY_4;
    keyMap[KeyButton::Num5] = GLFW_KEY_5;
    keyMap[KeyButton::Num6] = GLFW_KEY_6;
    keyMap[KeyButton::Num7] = GLFW_KEY_7;
    keyMap[KeyButton::Num8] = GLFW_KEY_8;
    keyMap[KeyButton::Num9] = GLFW_KEY_9;

    // Pavé numérique (identique pour tous les layouts)
    keyMap[KeyButton::Numpad0] = GLFW_KEY_KP_0;
    keyMap[KeyButton::Numpad1] = GLFW_KEY_KP_1;
    keyMap[KeyButton::Numpad2] = GLFW_KEY_KP_2;
    keyMap[KeyButton::Numpad3] = GLFW_KEY_KP_3;
    keyMap[KeyButton::Numpad4] = GLFW_KEY_KP_4;
    keyMap[KeyButton::Numpad5] = GLFW_KEY_KP_5;
    keyMap[KeyButton::Numpad6] = GLFW_KEY_KP_6;
    keyMap[KeyButton::Numpad7] = GLFW_KEY_KP_7;
    keyMap[KeyButton::Numpad8] = GLFW_KEY_KP_8;
    keyMap[KeyButton::Numpad9] = GLFW_KEY_KP_9;
    keyMap[KeyButton::NumpadDecimal] = GLFW_KEY_KP_DECIMAL;
    keyMap[KeyButton::NumpadDivide] = GLFW_KEY_KP_DIVIDE;
    keyMap[KeyButton::NumpadMultiply] = GLFW_KEY_KP_MULTIPLY;
    keyMap[KeyButton::NumpadSubtract] = GLFW_KEY_KP_SUBTRACT;
    keyMap[KeyButton::NumpadAdd] = GLFW_KEY_KP_ADD;
    keyMap[KeyButton::NumpadEnter] = GLFW_KEY_KP_ENTER;

    // Touches de fonction (identique pour tous les layouts)
    keyMap[KeyButton::F1] = GLFW_KEY_F1;
    keyMap[KeyButton::F2] = GLFW_KEY_F2;
    keyMap[KeyButton::F3] = GLFW_KEY_F3;
    keyMap[KeyButton::F4] = GLFW_KEY_F4;
    keyMap[KeyButton::F5] = GLFW_KEY_F5;
    keyMap[KeyButton::F6] = GLFW_KEY_F6;
    keyMap[KeyButton::F7] = GLFW_KEY_F7;
    keyMap[KeyButton::F8] = GLFW_KEY_F8;
    keyMap[KeyButton::F9] = GLFW_KEY_F9;
    keyMap[KeyButton::F10] = GLFW_KEY_F10;
    keyMap[KeyButton::F11] = GLFW_KEY_F11;
    keyMap[KeyButton::F12] = GLFW_KEY_F12;
    keyMap[KeyButton::F13] = GLFW_KEY_F13;
    keyMap[KeyButton::F14] = GLFW_KEY_F14;
    keyMap[KeyButton::F15] = GLFW_KEY_F15;
    keyMap[KeyButton::F16] = GLFW_KEY_F16;
    keyMap[KeyButton::F17] = GLFW_KEY_F17;
    keyMap[KeyButton::F18] = GLFW_KEY_F18;
    keyMap[KeyButton::F19] = GLFW_KEY_F19;
    keyMap[KeyButton::F20] = GLFW_KEY_F20;
    keyMap[KeyButton::F21] = GLFW_KEY_F21;
    keyMap[KeyButton::F22] = GLFW_KEY_F22;
    keyMap[KeyButton::F23] = GLFW_KEY_F23;
    keyMap[KeyButton::F24] = GLFW_KEY_F24;
    keyMap[KeyButton::F25] = GLFW_KEY_F25;

    // Boutons de souris (identique pour tous les layouts)
    mouseButtonMap[MouseButton::Left] = GLFW_MOUSE_BUTTON_LEFT;
    mouseButtonMap[MouseButton::Right] = GLFW_MOUSE_BUTTON_RIGHT;
    mouseButtonMap[MouseButton::Middle] = GLFW_MOUSE_BUTTON_MIDDLE;
    mouseButtonMap[MouseButton::X1] = GLFW_MOUSE_BUTTON_4;
    mouseButtonMap[MouseButton::X2] = GLFW_MOUSE_BUTTON_5;
    mouseButtonMap[MouseButton::X3] = GLFW_MOUSE_BUTTON_6;
    mouseButtonMap[MouseButton::X4] = GLFW_MOUSE_BUTTON_7;
    mouseButtonMap[MouseButton::X5] = GLFW_MOUSE_BUTTON_8;
#pragma endregion All Layouts Mapping

    // Réinitialise tous les InputManagers existants avec le nouveau layout
    for (auto it = inputManagers.begin(); it != inputManagers.end(); ++it) {
        it->second->Init();
    }
}
#pragma endregion Key Layout Mapping


void InputManager::UpdateInputEvent() {
    InputEvent event;
    for (auto it = keyStateMap.begin(); it != keyStateMap.end(); ++it) {
        if (it->second != InputState::Release) {
            KeyEvent keyEvent;
            keyEvent.key = it->first;
            keyEvent.state = it->second;
            event.keyEvents.push_back(keyEvent);
        }
    }

    for (auto it = keyModifierStateMap.begin(); it != keyModifierStateMap.end(); ++it) {
        if (it->second != InputState::Release) {
            KeyModifierEvent keyModifierEvent;
            keyModifierEvent.modifier = it->first;
            keyModifierEvent.state = it->second;
            event.keyModifierEvents.push_back(keyModifierEvent);
        }
    }

    for (auto it = mouseButtonStateMap.begin(); it != mouseButtonStateMap.end(); ++it) {
        if (it->second != InputState::Release) {
            MouseButtonEvent mouseButtonEvent;
            mouseButtonEvent.button = it->first;
            mouseButtonEvent.state = it->second;
            event.mouseButtonEvents.push_back(mouseButtonEvent);
        }
    }
    event.mouseMoveEvents = { 
        this->mouseX, this->mouseY, 
        this->mouseDeltaX, this->mouseDeltaY 
    };
    event.type = 
        (event.keyEvents.size() > 0 ? EventType::eKey : 0) |
        (event.keyModifierEvents.size() > 0 ? EventType::eKeyModifier : 0) |
        (event.mouseButtonEvents.size() > 0 ? EventType::eMouseButton : 0) |
        ((this->mouseDeltaX != 0 || this->mouseDeltaY != 0) ? EventType::eMouseMove : 0);
    this->inputEvent = event;
}



bool InputManager::IsCurrentInputEventEmpty() {
    return inputEvent.keyEvents.empty() && 
        inputEvent.keyModifierEvents.empty() && 
        inputEvent.mouseButtonEvents.empty() &&
        inputEvent.mouseMoveEvents.deltaX == 0 &&
        inputEvent.mouseMoveEvents.deltaY == 0;
}

template <typename T>
// T can be one of the following types: KeyEvent, KeyModifierEvent, MouseButtonEvent
bool IsActionInCurrent(const std::vector<T>& current, const std::vector<T>& action, std::function<bool(const T&)> GetValue) {
    bool isActive;
    for (auto actionEvent : action.keyEvents) {
        isActive = false;
        for (auto event : current.keyEvents){
            if (GetValue(actionEvent) != GetValue(event)) break;

            if (actionEvent.state == InputState::Pressed) {
                isActive == (event.state != InputState::Release);
            } else {
                isActive == (actionEvent.state == event.state);
            }
            if (isActive) break;
        }
        if (!isActive) return false;
    }
    return true;
}

bool InputManager::IsActionActive(std::string action) {
    if (!IsActionBound(action)) return false;
    
    InputEvent ActionEvent = GetInputEventOfAction(action);
    InputEvent currentEvent = GetCurrentInputEvent();
    
    bool isAllKeyActive         = IsActionInCurrent<KeyEvent>(currentEvent.keyEvents, ActionEvent.keyEvents, 
            [](const KeyEvent& event) { return event.key; });
    if (!isAllKeyActive) return false;

    bool isAllKeyModifierActive = IsActionInCurrent<KeyModifierEvent>(currentEvent.keyModifierEvents, ActionEvent.keyModifierEvents, 
            [](const KeyModifierEvent& event) { return event.modifier; });
    if (!isAllKeyModifierActive) return false;

    bool isAllMouseButtonActive = IsActionInCurrent<MouseButtonEvent>(currentEvent.mouseButtonEvents, ActionEvent.mouseButtonEvents, 
            [](const MouseButtonEvent& event) { return event.button; });
    
    return isAllMouseButtonActive;
}
