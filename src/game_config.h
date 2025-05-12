#pragma once

#include <SDL3/SDL.h>
#include <ini.h>

int32_t BUTTON_LEFT = SDL_SCANCODE_A;
int32_t BUTTON_RIGHT = SDL_SCANCODE_D;
int32_t BUTTON_SONIC_DROP = SDL_SCANCODE_W;
int32_t BUTTON_HARD_DROP = SDL_SCANCODE_U;
int32_t BUTTON_SOFT_DROP = SDL_SCANCODE_S;
int32_t BUTTON_CCW_1 = SDL_SCANCODE_J;
int32_t BUTTON_CCW_2 = SDL_SCANCODE_L;
int32_t BUTTON_CW = SDL_SCANCODE_K;
int32_t BUTTON_RESET = SDL_SCANCODE_R;
int32_t BUTTON_START = SDL_SCANCODE_RETURN;
int32_t BUTTON_QUIT = SDL_SCANCODE_ESCAPE;
int32_t BUTTON_SCALE_UP = SDL_SCANCODE_EQUALS;
int32_t BUTTON_SCALE_DOWN = SDL_SCANCODE_MINUS;
int32_t BUTTON_TOGGLE_ROTATION_SYSTEM = SDL_SCANCODE_0;
int32_t BUTTON_MUTE = SDL_SCANCODE_P;
int32_t BUTTON_MYSTERY = SDL_SCANCODE_KP_MINUS;

int32_t GAMEPAD_LEFT = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
int32_t GAMEPAD_RIGHT = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
int32_t GAMEPAD_SONIC_DROP = SDL_GAMEPAD_BUTTON_DPAD_UP;
int32_t GAMEPAD_SOFT_DROP = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
int32_t GAMEPAD_HARD_DROP = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
int32_t GAMEPAD_CCW_1 = SDL_GAMEPAD_BUTTON_WEST;
int32_t GAMEPAD_CCW_2 = SDL_GAMEPAD_BUTTON_SOUTH;
int32_t GAMEPAD_CW = SDL_GAMEPAD_BUTTON_EAST;
int32_t GAMEPAD_RESET = SDL_GAMEPAD_BUTTON_BACK;
int32_t GAMEPAD_START = SDL_GAMEPAD_BUTTON_START;
int32_t GAMEPAD_QUIT = SDL_GAMEPAD_BUTTON_INVALID;
int32_t GAMEPAD_SCALE_UP = SDL_GAMEPAD_BUTTON_INVALID;
int32_t GAMEPAD_SCALE_DOWN = SDL_GAMEPAD_BUTTON_INVALID;
int32_t GAMEPAD_TOGGLE_ROTATION_SYSTEM = SDL_GAMEPAD_BUTTON_INVALID;
int32_t GAMEPAD_MUTE = SDL_GAMEPAD_BUTTON_INVALID;
int32_t GAMEPAD_MYSTERY = SDL_GAMEPAD_BUTTON_INVALID;

static int parse_config(void* user, const char* section, const char* name, const char* value) {
    if (SDL_strcasecmp(section, "keyboard") == 0) {
        const SDL_Scancode s = SDL_GetScancodeFromName(value);
        if (SDL_strcasecmp(name, "left") == 0) BUTTON_LEFT = s;
        if (SDL_strcasecmp(name, "right") == 0) BUTTON_RIGHT = s;
        if (SDL_strcasecmp(name, "sonic_drop") == 0) BUTTON_SONIC_DROP = s;
        if (SDL_strcasecmp(name, "hard_drop") == 0) BUTTON_HARD_DROP = s;
        if (SDL_strcasecmp(name, "soft_drop") == 0) BUTTON_SOFT_DROP = s;
        if (SDL_strcasecmp(name, "ccw_1") == 0) BUTTON_CCW_1 = s;
        if (SDL_strcasecmp(name, "ccw_2") == 0) BUTTON_CCW_2 = s;
        if (SDL_strcasecmp(name, "cw") == 0) BUTTON_CW = s;
        if (SDL_strcasecmp(name, "reset") == 0) BUTTON_RESET = s;
        if (SDL_strcasecmp(name, "start") == 0) BUTTON_START = s;
        if (SDL_strcasecmp(name, "quit") == 0) BUTTON_QUIT = s;
        if (SDL_strcasecmp(name, "scale_up") == 0) BUTTON_SCALE_UP = s;
        if (SDL_strcasecmp(name, "scale_down") == 0) BUTTON_SCALE_DOWN = s;
        if (SDL_strcasecmp(name, "toggle_rotation_system") == 0) BUTTON_TOGGLE_ROTATION_SYSTEM = s;
        if (SDL_strcasecmp(name, "mute") == 0) BUTTON_MUTE = s;
        if (SDL_strcasecmp(name, "mystery") == 0) BUTTON_MYSTERY = s;
    }

    if (SDL_strcasecmp(section, "gamepad") == 0) {
        const SDL_GamepadButton s = SDL_GetGamepadButtonFromString(value);
        if (SDL_strcasecmp(name, "left") == 0) GAMEPAD_LEFT = s;
        if (SDL_strcasecmp(name, "right") == 0) GAMEPAD_RIGHT = s;
        if (SDL_strcasecmp(name, "sonic_drop") == 0) GAMEPAD_SONIC_DROP = s;
        if (SDL_strcasecmp(name, "hard_drop") == 0) GAMEPAD_HARD_DROP = s;
        if (SDL_strcasecmp(name, "soft_drop") == 0) GAMEPAD_SOFT_DROP = s;
        if (SDL_strcasecmp(name, "ccw_1") == 0) GAMEPAD_CCW_1 = s;
        if (SDL_strcasecmp(name, "ccw_2") == 0) GAMEPAD_CCW_2 = s;
        if (SDL_strcasecmp(name, "cw") == 0) GAMEPAD_CW = s;
        if (SDL_strcasecmp(name, "reset") == 0) GAMEPAD_RESET = s;
        if (SDL_strcasecmp(name, "start") == 0) GAMEPAD_START = s;
        if (SDL_strcasecmp(name, "quit") == 0) GAMEPAD_QUIT = s;
        if (SDL_strcasecmp(name, "scale_up") == 0) GAMEPAD_SCALE_UP = s;
        if (SDL_strcasecmp(name, "scale_down") == 0) GAMEPAD_SCALE_DOWN = s;
        if (SDL_strcasecmp(name, "toggle_rotation_system") == 0) GAMEPAD_TOGGLE_ROTATION_SYSTEM = s;
        if (SDL_strcasecmp(name, "mute") == 0) GAMEPAD_MUTE = s;
        if (SDL_strcasecmp(name, "mystery") == 0) GAMEPAD_MYSTERY = s;
    }

    return 1;
}

inline void load_config() {
    ini_parse("config.ini", parse_config, NULL);
}