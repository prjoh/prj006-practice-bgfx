#pragma once


#include <unordered_map>
#include <string>

#include <SDL.h>

#include <Types.h>


namespace zv
{
	class Input
	{
	private:
		Input() = default;

    public:
        static void update();

        //// Input-related functions: gamepads
        //bool isGamepadAvailable(s32 gamepadId) { IsGamepadAvailable(gamepadId); }                                               // Check if a gamepad is available
        //const std::string& gamepadName(s32 gamepadId) { return GetGamepadName(gamepadId); }                                       // Get gamepad internal name id
        //bool controllerButtonPressed(s32 gamepadId, s32 button) const { return IsGamepadButtonPressed(gamepadId, button); }     // Get gamepad internal name id
        //bool controllerButtonReleased(s32 gamepadId, s32 button) const { return IsGamepadButtonReleased(gamepadId, button); }   // Get gamepad internal name id
        //bool controllerButtonDown(s32 gamepadId, s32 button) const { return IsGamepadButtonDown(gamepadId, button); }           // Get gamepad internal name id
        //bool controllerButtonUp(s32 gamepadId, s32 button) const { return IsGamepadButtonUp(gamepadId, button); }               // Get gamepad internal name id
        //f32 controllerAxis(s32 gamepadId, s32 axis) const { GetGamepadAxisMovement(gamepadId, axis); }                          // Get axis movement value for a gamepad axis

        //// Input-related functions: mouse
        //bool mouseButtonPressed(s32 mouseButton) const { return IsMouseButtonPressed(mouseButton); }    // Check if a mouse button has been pressed once
        //bool mouseButtonReleased(s32 mouseButton) const { return IsMouseButtonReleased(mouseButton); }  // Check if a mouse button has been released once
        //bool mouseButtonDown(s32 mouseButton) const { return IsMouseButtonDown(mouseButton); }          // Check if a mouse button is being pressed
        //bool mouseButtonUp(s32 mouseButton) const { return IsMouseButtonUp(mouseButton); }              // Check if a mouse button is NOT being pressed
        //s32 mouseX() const { return GetMouseX(); }                                                      // Get mouse position X
        //s32 mouseY() const { return GetMouseY(); }                                                      // Get mouse position Y
        //Vector2 mousePosition() const { return GetMousePosition(); }                                    // Get mouse position XY
        //Vector2 mouseDelta() const { return GetMouseDelta(); }                                          // Get mouse delta between frames
        //Vector2 mouseWheel() const { return GetMouseWheelMoveV(); }                                     // Get mouse wheel movement for both X and Y
        //void setMousePosition(s32 x, s32 y) { setMousePosition(x, y); }                                 // Set mouse position XY

        //// Cursor-related functions
        //void showCursor() { ShowCursor(); }              // Shows cursor
        //void hideCursor() { HideCursor(); }              // Hides cursor
        //bool isCursorHidden() { IsCursorHidden(); }      // Check if cursor is not visible
        //void enableCursor() { EnableCursor(); }          // Enables cursor (unlock cursor)
        //void disableCursor() { DisableCursor(); }        // Disables cursor (lock cursor)
        //bool isCursorOnScreen() { IsCursorOnScreen(); }  // Check if cursor is on the screen


        //// Input-related functions: keyboard
        //bool keyPressed(s32 keyCode) const { return IsKeyPressed(keyCode); }    // Check if a key has been pressed once
        //bool keyReleased(s32 keyCode) const { return IsKeyReleased(keyCode); }  // Check if a key has been released once
        //bool keyDown(s32 keyCode) const { return IsKeyDown(keyCode); }          // Check if a key is being pressed
        //bool keyUp(s32 keyCode) const { return IsKeyUp(keyCode); }              // Check if a key is NOT being pressed
        //void setExitKey(s32 keyCode) { return SetExitKey(keyCode); }            // Set a custom key to exit program (default is ESC)
	

        static bool quitEvent() { return m_quit; };

        static bool controllerButtonPressed(u8 button);
        static bool controllerButtonDown(u8 button);
        static bool controllerButtonUp(u8 button);
        static f32 controllerAxis(u8 axis);

        static bool mouseButtonPressed(u8 mouseButton);
        static bool mouseButtonDown(u8 mouseButton);
        static bool mouseButtonUp(u8 mouseButton);
        static s32 mouseX() { return m_mouseX; };
        static s32 mouseY() { return m_mouseY; };
        static s32 mouseDeltaX() { return m_mouseDeltaX; };
        static s32 mouseDeltaY() { return m_mouseDeltaY; };
        static s32 mouseWheelX() { return m_mouseWheelX; };
        static s32 mouseWheelY() { return m_mouseWheelY; };

        static bool keyPressed(SDL_Keycode keyCode);
        static bool keyDown(SDL_Keycode keyCode);
        static bool keyUp(SDL_Keycode keyCode);


    private:
        static void reset();

    private:
        static SDL_Event m_event;

        static bool m_quit;

        /*
        SDLK_{keyname}
        */
        static std::unordered_map<SDL_Keycode, bool> m_isKeyDown;
        static std::unordered_map<SDL_Keycode, bool> m_isKeyUp;
        static std::unordered_map<SDL_Keycode, bool> m_isKeyPressed;

        /*
        SDL_BUTTON_LEFT
        SDL_BUTTON_MIDDLE
        SDL_BUTTON_RIGHT
        SDL_BUTTON_X1
        SDL_BUTTON_X2
        */
        static std::unordered_map<u8, bool> m_isMouseButtonDown;
        static std::unordered_map<u8, bool> m_isMouseButtonUp;
        static std::unordered_map<u8, bool> m_isMouseButtonPressed;

        static s32 m_mouseX;
        static s32 m_mouseY;
        static s32 m_mouseDeltaX;
        static s32 m_mouseDeltaY;
        static s32 m_mouseWheelX;
        static s32 m_mouseWheelY;

        /*
        SDL_CONTROLLER_BUTTON_INVALID
        SDL_CONTROLLER_BUTTON_A
        SDL_CONTROLLER_BUTTON_B
        SDL_CONTROLLER_BUTTON_X
        SDL_CONTROLLER_BUTTON_Y
        SDL_CONTROLLER_BUTTON_BACK
        SDL_CONTROLLER_BUTTON_GUIDE
        SDL_CONTROLLER_BUTTON_START
        SDL_CONTROLLER_BUTTON_LEFTSTICK
        SDL_CONTROLLER_BUTTON_RIGHTSTICK
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
        SDL_CONTROLLER_BUTTON_DPAD_UP
        SDL_CONTROLLER_BUTTON_DPAD_DOWN
        SDL_CONTROLLER_BUTTON_DPAD_LEFT
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT
        SDL_CONTROLLER_BUTTON_MISC1
        SDL_CONTROLLER_BUTTON_PADDLE1
        SDL_CONTROLLER_BUTTON_PADDLE2
        SDL_CONTROLLER_BUTTON_PADDLE3
        SDL_CONTROLLER_BUTTON_PADDLE4
        SDL_CONTROLLER_BUTTON_TOUCHPAD
        SDL_CONTROLLER_BUTTON_MAX
        */
        static std::unordered_map<u8, bool> m_isControllerButtonDown;
        static std::unordered_map<u8, bool> m_isControllerButtonUp;
        static std::unordered_map<u8, bool> m_isControllerButtonPressed;

        /*
        SDL_CONTROLLER_AXIS_INVALID
        SDL_CONTROLLER_AXIS_LEFTX
        SDL_CONTROLLER_AXIS_LEFTY
        SDL_CONTROLLER_AXIS_RIGHTX
        SDL_CONTROLLER_AXIS_RIGHTY
        SDL_CONTROLLER_AXIS_TRIGGERLEFT
        SDL_CONTROLLER_AXIS_TRIGGERRIGHT
        SDL_CONTROLLER_AXIS_MAX
        */
        static std::unordered_map<u8, f32> m_controllerAxisValues;
    };
}
