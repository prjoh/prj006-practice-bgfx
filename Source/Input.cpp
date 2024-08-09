#include <Input.h>


#include <backends/imgui_impl_sdl2.h>


namespace zv
{
    SDL_Event Input::m_event;
    bool Input::m_quit = false;
    std::unordered_map<SDL_Keycode, bool> Input::m_isKeyDown;
    std::unordered_map<SDL_Keycode, bool> Input::m_isKeyUp;
    std::unordered_map<SDL_Keycode, bool> Input::m_isKeyPressed;
    std::unordered_map<u8, bool> Input::m_isMouseButtonDown;
    std::unordered_map<u8, bool> Input::m_isMouseButtonUp;
    std::unordered_map<u8, bool> Input::m_isMouseButtonPressed;
    s32 Input::m_mouseX;
    s32 Input::m_mouseY;
    s32 Input::m_mouseDeltaX;
    s32 Input::m_mouseDeltaY;
    s32 Input::m_mouseWheelX;
    s32 Input::m_mouseWheelY;
    std::unordered_map<u8, bool> Input::m_isControllerButtonDown;
    std::unordered_map<u8, bool> Input::m_isControllerButtonUp;
    std::unordered_map<u8, bool> Input::m_isControllerButtonPressed;
    std::unordered_map<u8, f32> Input::m_controllerAxisValues;


    void Input::update()
    {
        reset();

        //if (!ImGui::GetIO().WantCaptureMouse) {}

        //while (SDL_PollEvent(&event)) {

        //    ImGui_ImplSDL2_ProcessEvent(&event);

        //    switch (event.type) {
        //    case SDL_QUIT:
        //        exit = true;
        //        break;

        //    case SDL_WINDOWEVENT: {
        //        const SDL_WindowEvent& wev = event.window;
        //        switch (wev.event) {
        //        case SDL_WINDOWEVENT_RESIZED:
        //        case SDL_WINDOWEVENT_SIZE_CHANGED:
        //            break;

        //        case SDL_WINDOWEVENT_CLOSE:
        //            exit = true;
        //            break;
        //        }
        //    } break;
        //    }

        while (SDL_PollEvent(&m_event)) {

            ImGui_ImplSDL2_ProcessEvent(&m_event);

            switch (m_event.type) {
                case SDL_QUIT:
                {
                    m_quit = true;
                    break;
                }

                case SDL_KEYDOWN:
                {
                    bool lastFramePressed = keyPressed(m_event.key.keysym.sym);
                    m_isKeyDown[m_event.key.keysym.sym] = true && !lastFramePressed;
                    m_isKeyPressed[m_event.key.keysym.sym] = true;
                    break;
                }

                case SDL_KEYUP:
                {
                    m_isKeyUp[m_event.key.keysym.sym] = true;
                    m_isKeyPressed[m_event.key.keysym.sym] = false;
                    break;
                }

                case SDL_MOUSEBUTTONDOWN:
                {
                    bool lastFramePressed = mouseButtonPressed(m_event.button.button);
                    m_isMouseButtonDown[m_event.button.button] = true && !lastFramePressed;
                    m_isMouseButtonPressed[m_event.button.button] = true;
                    break;
                }

                case SDL_MOUSEBUTTONUP:
                {
                    m_isMouseButtonUp[m_event.button.button] = true;
                    m_isMouseButtonPressed[m_event.button.button] = false;
                    break;
                }

                case SDL_MOUSEMOTION:
                {
                    m_mouseX = m_event.motion.x;
                    m_mouseY = m_event.motion.y;
                    m_mouseDeltaX = m_event.motion.xrel;
                    m_mouseDeltaY = m_event.motion.yrel;
                    break;
                }

                case SDL_MOUSEWHEEL:
                {
                    m_mouseWheelX = m_event.wheel.x;
                    m_mouseWheelY = m_event.wheel.y;
                    break;
                }

                case SDL_CONTROLLERBUTTONDOWN:
                {
                    bool lastFramePressed = controllerButtonPressed(m_event.cbutton.button);
                    m_isControllerButtonDown[m_event.cbutton.button] = true && !lastFramePressed;
                    m_isControllerButtonPressed[m_event.cbutton.button] = true;
                    break;
                }

                case SDL_CONTROLLERBUTTONUP:
                {
                    m_isControllerButtonUp[m_event.cbutton.button] = true;
                    m_isControllerButtonPressed[m_event.cbutton.button] = false;
                    break;
                }

                case SDL_CONTROLLERAXISMOTION:
                {
                    m_controllerAxisValues[m_event.caxis.axis] = (f32)m_event.caxis.value / (f32)SDL_JOYSTICK_AXIS_MIN;
                    break;
                }
            }
        
            m_quit = m_quit || keyPressed(SDLK_ESCAPE);
        }
    }

    void Input::reset()
    {
        for (auto& [_, v] : m_isKeyDown) v = false;
        for (auto& [_, v] : m_isKeyUp) v = false;
        for (auto& [_, v] : m_isMouseButtonDown) v = false;
        for (auto& [_, v] : m_isMouseButtonUp) v = false;
        for (auto& [_, v] : m_isControllerButtonDown) v = false;
        for (auto& [_, v] : m_isControllerButtonUp) v = false;
        m_mouseDeltaX = 0;
        m_mouseDeltaY = 0;
        m_mouseWheelX = 0;
        m_mouseWheelY = 0;
    }

    bool Input::controllerButtonPressed(u8 button)
    {
        auto it = m_isControllerButtonPressed.find(button);
        if (it == m_isControllerButtonPressed.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::controllerButtonDown(u8 button)
    {
        auto it = m_isControllerButtonDown.find(button);
        if (it == m_isControllerButtonDown.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::controllerButtonUp(u8 button)
    {
        auto it = m_isControllerButtonUp.find(button);
        if (it == m_isControllerButtonUp.end()) {
            return false;
        }
        return it->second;
    }

    float Input::controllerAxis(u8 axis)
    {
        auto it = m_controllerAxisValues.find(axis);
        if (it == m_controllerAxisValues.end()) {
            return 0.0f;
        }
        return it->second;
    }

    bool Input::mouseButtonPressed(u8 mouseButton)
    {
        auto it = m_isMouseButtonPressed.find(mouseButton);
        if (it == m_isMouseButtonPressed.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::mouseButtonDown(u8 mouseButton)
    {
        auto it = m_isMouseButtonDown.find(mouseButton);
        if (it == m_isMouseButtonDown.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::mouseButtonUp(u8 mouseButton)
    {
        auto it = m_isMouseButtonUp.find(mouseButton);
        if (it == m_isMouseButtonUp.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::keyPressed(SDL_Keycode keyCode)
    {
        auto it = m_isKeyPressed.find(keyCode);
        if (it == m_isKeyPressed.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::keyDown(SDL_Keycode keyCode)
    {
        auto it = m_isKeyDown.find(keyCode);
        if (it == m_isKeyDown.end()) {
            return false;
        }
        return it->second;
    }

    bool Input::keyUp(SDL_Keycode keyCode)
    {
        auto it = m_isKeyUp.find(keyCode);
        if (it == m_isKeyUp.end()) {
            return false;
        }
        return it->second;
    }
}
