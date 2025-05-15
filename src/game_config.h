#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>
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
int32_t BUTTON_TOGGLE_TRANSPARENCY = SDL_SCANCODE_T;

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
int32_t GAMEPAD_TOGGLE_TRANSPARENCY = SDL_GAMEPAD_BUTTON_INVALID;

typedef struct {
    uint8_t *wave_data;
    uint32_t wave_data_len;
} wave_data_t;
typedef struct {
    int32_t level_start;
    wave_data_t head;
    wave_data_t body;
} section_music_t;

SDL_AudioSpec MUSIC_SPEC;
int32_t SECTION_COUNT = 0;
section_music_t *SECTIONS = NULL;
section_music_t TITLE_MUSIC = {0};

static inline bool load_song(const char *file, uint8_t **head_data, uint32_t *head_data_len, uint8_t **body_data, uint32_t *body_data_len) {
    // Build the new strings.
    const size_t len = SDL_strlen(file) + 20;
    char *with_h_wav = calloc(len, sizeof(char));
    char *with_b_wav = calloc(len, sizeof(char));
    char *with_wav = calloc(len, sizeof(char));
    SDL_strlcpy(with_h_wav, "data/", len);
    SDL_strlcpy(with_b_wav, "data/", len);
    SDL_strlcpy(with_wav, "data/", len);
    SDL_strlcat(with_h_wav, file, len);
    SDL_strlcat(with_b_wav, file, len);
    SDL_strlcat(with_wav, file, len);
    SDL_strlcat(with_h_wav, "_h.wav", len);
    SDL_strlcat(with_b_wav, "_b.wav", len);
    SDL_strlcat(with_wav, ".wav", len);

    // Try and load all possible wavs.
    uint8_t *with_h_data;
    uint32_t with_h_data_len;
    bool with_h_data_should_free = true;
    uint8_t *with_b_data;
    uint32_t with_b_data_len;
    bool with_b_data_should_free = true;
    uint8_t *with_wav_data;
    uint32_t with_wav_data_len;
    bool with_wav_data_should_free = true;
    if (!SDL_LoadWAV(with_h_wav, &MUSIC_SPEC, &with_h_data, &with_h_data_len)) {
        with_h_data = NULL;
        with_h_data_len = 0;
        with_h_data_should_free = false;
    }
    if (!SDL_LoadWAV(with_b_wav, &MUSIC_SPEC, &with_b_data, &with_b_data_len)) {
        with_b_data = NULL;
        with_b_data_len = 0;
        with_b_data_should_free = false;
    }
    if (!SDL_LoadWAV(with_wav, &MUSIC_SPEC, &with_wav_data, &with_wav_data_len)) {
        with_wav_data = NULL;
        with_wav_data_len = 0;
        with_wav_data_should_free = false;
    }

    // Try and assign these to slots.
    bool retval = false;
    if (with_h_data != NULL && with_b_data != NULL) {
        // Do we have both head and body?
        // Mark as used.
        with_h_data_should_free = false;
        with_b_data_should_free = false;

        // Success!
        retval = true;

        // Store the data.
        *head_data = with_h_data;
        *head_data_len = with_h_data_len;
        *body_data = with_b_data;
        *body_data_len = with_b_data_len;
    } else if (with_wav_data != NULL) {
        // Do we have a bare .wav?
        // Mark as used.
        with_wav_data_should_free = false;

        // Success!
        retval = true;

        // Store the data.
        *head_data = with_wav_data;
        *head_data_len = with_wav_data_len;
        *body_data = with_wav_data;
        *body_data_len = with_wav_data_len;
    } else if (with_h_data != NULL || with_b_data != NULL) {
        // Last ditch attempt at doing something useful...
        // Just store whatever we found as both head and body.
        retval = true;
        if (with_h_data != NULL) {
            with_h_data_should_free = false;
            *head_data = with_h_data;
            *head_data_len = with_h_data_len;
            *body_data = with_h_data;
            *body_data_len = with_h_data_len;
        } else {
            with_b_data_should_free = false;
            *head_data = with_b_data;
            *head_data_len = with_b_data_len;
            *body_data = with_b_data;
            *body_data_len = with_b_data_len;
        }
    }

    // Free the new strings and buffers that ended up unused.
    SDL_free(with_wav);
    SDL_free(with_b_wav);
    SDL_free(with_h_wav);
    if (with_h_data_should_free) SDL_free(with_h_data);
    if (with_b_data_should_free) SDL_free(with_b_data);
    if (with_wav_data_should_free) SDL_free(with_wav_data);
    return retval;
}

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
        if (SDL_strcasecmp(name, "toggle_transparency") == 0) BUTTON_TOGGLE_TRANSPARENCY = s;
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
        if (SDL_strcasecmp(name, "toggle_transparency") == 0) GAMEPAD_TOGGLE_TRANSPARENCY = s;
    }

    if (SDL_strcasecmp(section, "music") == 0) {
        if (SDL_strcasecmp(name, "menu") == 0) {
            // Load menu...
            if (load_song(value, &TITLE_MUSIC.head.wave_data, &TITLE_MUSIC.head.wave_data_len, &TITLE_MUSIC.body.wave_data, &TITLE_MUSIC.body.wave_data_len)) {
                TITLE_MUSIC.level_start = -1;
            }
        } else {
            // Load section music...
            char *end;
            const long level_start = SDL_strtol(name, &end, 10);
            if (end == name) return 1;
            section_music_t s = {0};
            s.level_start = level_start;
            if (load_song(value, &s.head.wave_data, &s.head.wave_data_len, &s.body.wave_data, &s.body.wave_data_len)) {
                SECTION_COUNT++;
                SECTIONS = SDL_realloc(SECTIONS, SECTION_COUNT * sizeof(section_music_t));
                SDL_memcpy(SECTIONS + (SECTION_COUNT-1), &s, sizeof(section_music_t));
            }
        }
    }

    return 1;
}

static int SDLCALL compare_section(const void *a, const void *b) {
    const section_music_t *A = (const section_music_t *)a;
    const section_music_t *B = (const section_music_t *)b;

    if (A->level_start < B->level_start) {
        return -1;
    } else if (B->level_start < A->level_start) {
        return 1;
    } else {
        return 0;
    }
}

static inline void load_config() {
    ini_parse("config.ini", parse_config, NULL);
    if (SECTIONS != NULL) SDL_qsort(SECTIONS, SECTION_COUNT, sizeof(section_music_t), compare_section);
}
