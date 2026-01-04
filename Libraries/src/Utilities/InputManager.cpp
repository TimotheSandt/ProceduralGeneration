#include "Utilities/InputManager.h"
#include <windows.h>
#include <functional>
#include <iostream>
#include "Logger.h"

#undef DELETE

KeyLayout InputManager::keyLayout = KeyLayout::UNDEFINED;
std::unordered_map<KeyButton, GLint> InputManager::keyMap;
std::unordered_map<MouseButton, GLint> InputManager::mouseButtonMap;
std::unordered_map<std::string, InputAction> InputManager::mapActionToInputAction;

std::unordered_map<GLFWwindow*, std::unique_ptr<InputManager>> InputManager::inputManagers;


InputManager::InputManager(GLFWwindow* window) : window(window) {
    Init();
}

InputManager::~InputManager() {
    RemoveInstance(window);
}

InputManager& InputManager::GetInstance(GLFWwindow* window) {
    if (keyLayout == KeyLayout::UNDEFINED) {
        AutoDetectKeyLayout();
    }
    if (inputManagers.find(window) == inputManagers.end()) {
        inputManagers[window] = std::unique_ptr<InputManager>(new InputManager(window));
    }
    return *inputManagers[window];
}

void InputManager::Init() {
    // Keyboard
    for (auto it = keyMap.begin(); it != keyMap.end(); ++it) {
        keyStateMap[it->first] = InputState::Release;
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
            ((it->second == InputState::Release || it->second == InputState::PressEnd) ?
                InputState::PressBegin : InputState::Pressed) :
            ((it->second == InputState::Pressed || it->second == InputState::PressBegin) ?
                InputState::PressEnd : InputState::Release);
    }

    // Mouse
    for (auto it = mouseButtonStateMap.begin(); it != mouseButtonStateMap.end(); ++it) {
        bool isPressed = glfwGetMouseButton(window, mouseButtonMap[it->first]) == GLFW_PRESS;
        it->second = isPressed ?
            ((it->second == InputState::Release || it->second == InputState::PressEnd) ?
                InputState::PressBegin : InputState::Pressed) :
            ((it->second == InputState::Pressed || it->second == InputState::PressBegin) ?
                InputState::PressEnd : InputState::Release);
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
        keyMap[KeyButton::MINUS] = GLFW_KEY_MINUS;
        keyMap[KeyButton::EQUALS] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LEFT_BRACKET] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RIGHT_BRACKET] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::SEMICOLON] = GLFW_KEY_COMMA;
        keyMap[KeyButton::COMMA] = GLFW_KEY_M;
        keyMap[KeyButton::PERIOD] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::SLASH] = GLFW_KEY_SLASH;
        keyMap[KeyButton::BACKSLASH] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::APOSTROPHE] = GLFW_KEY_APOSTROPHE;
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
        keyMap[KeyButton::MINUS] = GLFW_KEY_MINUS;
        keyMap[KeyButton::EQUALS] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LEFT_BRACKET] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RIGHT_BRACKET] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::SEMICOLON] = GLFW_KEY_SEMICOLON;
        keyMap[KeyButton::COMMA] = GLFW_KEY_COMMA;
        keyMap[KeyButton::PERIOD] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::SLASH] = GLFW_KEY_SLASH;
        keyMap[KeyButton::BACKSLASH] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::APOSTROPHE] = GLFW_KEY_APOSTROPHE;
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
        keyMap[KeyButton::MINUS] = GLFW_KEY_LEFT_BRACKET;       // - logique -> [ physique
        keyMap[KeyButton::EQUALS] = GLFW_KEY_RIGHT_BRACKET;     // = logique -> ] physique
        keyMap[KeyButton::LEFT_BRACKET] = GLFW_KEY_SLASH;        // [ logique -> / physique
        keyMap[KeyButton::RIGHT_BRACKET] = GLFW_KEY_EQUAL;       // ] logique -> = physique
        keyMap[KeyButton::SEMICOLON] = GLFW_KEY_S;              // ; logique -> S physique
        keyMap[KeyButton::APOSTROPHE] = GLFW_KEY_MINUS;         // ' logique -> - physique
        keyMap[KeyButton::COMMA] = GLFW_KEY_W;                  // , logique -> W physique
        keyMap[KeyButton::PERIOD] = GLFW_KEY_V;                 // . logique -> V physique
        keyMap[KeyButton::SLASH] = GLFW_KEY_Z;                  // / logique -> Z physique
        keyMap[KeyButton::BACKSLASH] = GLFW_KEY_BACKSLASH;      // Backslash remains Backslash
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
        keyMap[KeyButton::MINUS] = GLFW_KEY_MINUS;
        keyMap[KeyButton::EQUALS] = GLFW_KEY_EQUAL;
        keyMap[KeyButton::LEFT_BRACKET] = GLFW_KEY_LEFT_BRACKET;
        keyMap[KeyButton::RIGHT_BRACKET] = GLFW_KEY_RIGHT_BRACKET;
        keyMap[KeyButton::SEMICOLON] = GLFW_KEY_SEMICOLON;
        keyMap[KeyButton::COMMA] = GLFW_KEY_COMMA;
        keyMap[KeyButton::PERIOD] = GLFW_KEY_PERIOD;
        keyMap[KeyButton::SLASH] = GLFW_KEY_SLASH;
        keyMap[KeyButton::BACKSLASH] = GLFW_KEY_BACKSLASH;
        keyMap[KeyButton::APOSTROPHE] = GLFW_KEY_APOSTROPHE;
        break;
#pragma endregion QWERTY
    }

#pragma region All Layout Mapping
    // Modificateurs (identiques pour tous les layouts)
    keyMap[KeyButton::LEFT_SHIFT] = GLFW_KEY_LEFT_SHIFT;
    keyMap[KeyButton::LEFT_CONTROL] = GLFW_KEY_LEFT_CONTROL;
    keyMap[KeyButton::LEFT_ALT] = GLFW_KEY_LEFT_ALT;
    keyMap[KeyButton::LEFT_SUPER] = GLFW_KEY_LEFT_SUPER;
    keyMap[KeyButton::RIGHT_SHIFT] = GLFW_KEY_RIGHT_SHIFT;
    keyMap[KeyButton::RIGHT_CONTROL] = GLFW_KEY_RIGHT_CONTROL;
    keyMap[KeyButton::RIGHT_ALT] = GLFW_KEY_RIGHT_ALT;
    keyMap[KeyButton::RIGHT_SUPER] = GLFW_KEY_RIGHT_SUPER;
    keyMap[KeyButton::CAPS_LOCK] = GLFW_KEY_CAPS_LOCK;
    keyMap[KeyButton::NUM_LOCK] = GLFW_KEY_NUM_LOCK;
    keyMap[KeyButton::SCROLL_LOCK] = GLFW_KEY_SCROLL_LOCK;

    // Navigation (identique pour tous les layouts)
    keyMap[KeyButton::SPACE] = GLFW_KEY_SPACE;
    keyMap[KeyButton::ENTER] = GLFW_KEY_ENTER;
    keyMap[KeyButton::TAB] = GLFW_KEY_TAB;
    keyMap[KeyButton::ESCAPE] = GLFW_KEY_ESCAPE;
    keyMap[KeyButton::BACKSPACE] = GLFW_KEY_BACKSPACE;
    keyMap[KeyButton::INSERT] = GLFW_KEY_INSERT;
    keyMap[KeyButton::DELETE] = GLFW_KEY_DELETE;
    keyMap[KeyButton::HOME] = GLFW_KEY_HOME;
    keyMap[KeyButton::END] = GLFW_KEY_END;
    keyMap[KeyButton::PAGE_UP] = GLFW_KEY_PAGE_UP;
    keyMap[KeyButton::PAGE_DOWN] = GLFW_KEY_PAGE_DOWN;
    keyMap[KeyButton::LEFT] = GLFW_KEY_LEFT;
    keyMap[KeyButton::RIGHT] = GLFW_KEY_RIGHT;
    keyMap[KeyButton::UP] = GLFW_KEY_UP;
    keyMap[KeyButton::DOWN] = GLFW_KEY_DOWN;

    // Chiffres (identique pour tous les layouts)
    keyMap[KeyButton::NUM_0] = GLFW_KEY_0;
    keyMap[KeyButton::NUM_1] = GLFW_KEY_1;
    keyMap[KeyButton::NUM_2] = GLFW_KEY_2;
    keyMap[KeyButton::NUM_3] = GLFW_KEY_3;
    keyMap[KeyButton::NUM_4] = GLFW_KEY_4;
    keyMap[KeyButton::NUM_5] = GLFW_KEY_5;
    keyMap[KeyButton::NUM_6] = GLFW_KEY_6;
    keyMap[KeyButton::NUM_7] = GLFW_KEY_7;
    keyMap[KeyButton::NUM_8] = GLFW_KEY_8;
    keyMap[KeyButton::NUM_9] = GLFW_KEY_9;

    // Pavé numérique (identique pour tous les layouts)
    keyMap[KeyButton::NUMPAD_0] = GLFW_KEY_KP_0;
    keyMap[KeyButton::NUMPAD_1] = GLFW_KEY_KP_1;
    keyMap[KeyButton::NUMPAD_2] = GLFW_KEY_KP_2;
    keyMap[KeyButton::NUMPAD_3] = GLFW_KEY_KP_3;
    keyMap[KeyButton::NUMPAD_4] = GLFW_KEY_KP_4;
    keyMap[KeyButton::NUMPAD_5] = GLFW_KEY_KP_5;
    keyMap[KeyButton::NUMPAD_6] = GLFW_KEY_KP_6;
    keyMap[KeyButton::NUMPAD_7] = GLFW_KEY_KP_7;
    keyMap[KeyButton::NUMPAD_8] = GLFW_KEY_KP_8;
    keyMap[KeyButton::NUMPAD_9] = GLFW_KEY_KP_9;
    keyMap[KeyButton::NUMPAD_DECIMAL] = GLFW_KEY_KP_DECIMAL;
    keyMap[KeyButton::NUMPAD_DIVIDE] = GLFW_KEY_KP_DIVIDE;
    keyMap[KeyButton::NUMPAD_MULTIPLY] = GLFW_KEY_KP_MULTIPLY;
    keyMap[KeyButton::NUMPAD_SUBTRACT] = GLFW_KEY_KP_SUBTRACT;
    keyMap[KeyButton::NUMPAD_ADD] = GLFW_KEY_KP_ADD;
    keyMap[KeyButton::NUMPAD_ENTER] = GLFW_KEY_KP_ENTER;

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
    mouseButtonMap[MouseButton::LEFT] = GLFW_MOUSE_BUTTON_LEFT;
    mouseButtonMap[MouseButton::RIGHT] = GLFW_MOUSE_BUTTON_RIGHT;
    mouseButtonMap[MouseButton::MIDDLE] = GLFW_MOUSE_BUTTON_MIDDLE;
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
    for (const auto& [key, state] : keyStateMap) {
        if (state != InputState::Release) {
            KeyButtonEvent keyEvent;
            keyEvent.key = key;
            keyEvent.state = state;
            event.keys.push_back(keyEvent);
        }
    }

    for (const auto& [button, state] : mouseButtonStateMap) {
        if (state != InputState::Release) {
            MouseButtonEvent mouseButtonEvent;
            mouseButtonEvent.button = button;
            mouseButtonEvent.state = state;
            event.mouseButtons.push_back(mouseButtonEvent);
        }
    }
    event.mouseMoveData = {
        this->mouseX, this->mouseY,
        this->mouseDeltaX, this->mouseDeltaY, 0, 0
    };
    this->inputEvent = event;
}

void InputManager::BindActionToInput(std::string action, InputAction inputAction) {
    if (IsActionBound(action)) {
        LOG_WARNING("Action ", action, " already bound. Overwriting.");
    }
    mapActionToInputAction[action] = inputAction;
}

bool InputManager::IsCurrentInputEventEmpty() {
    return inputEvent.keys.empty() &&
        inputEvent.mouseButtons.empty() &&
        inputEvent.mouseMoveData.deltaX == 0 &&
        inputEvent.mouseMoveData.deltaY == 0;
}

bool InputManager::IsActionActive(std::string action) {
    if (!IsActionBound(action)) {
        LOG_WARNING("Action ", action, " is not bound.");
        return false;
    }

    InputAction& actionInput = mapActionToInputAction[action];

    // Check keys
    for(const auto& key : actionInput.keys) {
        if (!IsKeyPressed(key)) return false;
    }

    // Check mouse buttons
    for(const auto& button : actionInput.mouseButtons) {
        if (!IsMouseButtonPressed(button)) return false;
    }

    return true;
}
