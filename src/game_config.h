#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>
#include <ini.h>
#include "stb_vorbis.h"

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
int32_t BUTTON_DETAILS = SDL_SCANCODE_F1;

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
int32_t GAMEPAD_DETAILS = SDL_GAMEPAD_BUTTON_INVALID;

int RENDER_SCALE = 2;
bool TRANSPARENCY = true;
bool MUTED = false;
bool TI_ARS = true;
float BGM_VOLUME = 0.6f;
float SFX_VOLUME = 1.0f;

typedef struct {
    int16_t *wave_data;
    uint32_t wave_data_len;
    SDL_AudioSpec format;
} wave_data_t;
typedef struct {
    int32_t level_start;
    wave_data_t head;
    wave_data_t body;
} section_music_t;

int32_t SECTION_COUNT = 0;
section_music_t *SECTIONS = NULL;
section_music_t TITLE_MUSIC = {0};

static inline bool load_song(const char *file, SDL_AudioSpec *head_format, int16_t **head_data, uint32_t *head_data_len, SDL_AudioSpec *body_format, int16_t **body_data, uint32_t *body_data_len) {
    // Build the new strings.
    const size_t len = SDL_strlen(file) + 20;
    char *with_h_ogg = SDL_calloc(len, sizeof(char));
    char *with_b_ogg = SDL_calloc(len, sizeof(char));
    char *with_ogg = SDL_calloc(len, sizeof(char));
    SDL_strlcpy(with_h_ogg, "data/", len);
    SDL_strlcpy(with_b_ogg, "data/", len);
    SDL_strlcpy(with_ogg, "data/", len);
    SDL_strlcat(with_h_ogg, file, len);
    SDL_strlcat(with_b_ogg, file, len);
    SDL_strlcat(with_ogg, file, len);
    SDL_strlcat(with_h_ogg, "_h.ogg", len);
    SDL_strlcat(with_b_ogg, "_b.ogg", len);
    SDL_strlcat(with_ogg, ".ogg", len);

    // Try and load all possible wavs.
    int16_t *with_h_data;
    SDL_AudioSpec with_h_spec = {0};
    bool with_h_data_should_free = true;
    int16_t *with_b_data;
    SDL_AudioSpec with_b_spec = {0};
    bool with_b_data_should_free = true;
    int16_t *with_ogg_data;
    SDL_AudioSpec with_ogg_spec = {0};
    bool with_ogg_data_should_free = true;

    int channels, sample_rate;

    int32_t with_h_data_len = stb_vorbis_decode_filename(with_h_ogg, &channels, &sample_rate, &with_h_data);
    if (with_h_data_len == -1) {
        with_h_data = NULL;
        with_h_data_len = 0;
        with_h_data_should_free = false;
    } else {
        with_h_spec.channels = channels;
        with_h_spec.freq = sample_rate;
        with_h_spec.format = SDL_AUDIO_S16;
        with_h_data_len *= channels * 2;
    }

    int32_t with_b_data_len = stb_vorbis_decode_filename(with_b_ogg, &channels, &sample_rate, &with_b_data);
    if (with_b_data_len == -1) {
        with_b_data = NULL;
        with_b_data_len = 0;
        with_b_data_should_free = false;
    } else {
        with_b_spec.channels = channels;
        with_b_spec.freq = sample_rate;
        with_b_spec.format = SDL_AUDIO_S16;
        with_b_data_len *= channels * 2;
    }

    int32_t with_ogg_data_len = stb_vorbis_decode_filename(with_ogg, &channels, &sample_rate, &with_ogg_data);
    if (with_ogg_data_len == -1) {
        with_ogg_data = NULL;
        with_ogg_data_len = 0;
        with_ogg_data_should_free = false;
    } else {
        with_ogg_spec.channels = channels;
        with_ogg_spec.freq = sample_rate;
        with_ogg_spec.format = SDL_AUDIO_S16;
        with_ogg_data_len *= channels * 2;
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
        *head_format = with_h_spec;
        *body_data = with_b_data;
        *body_data_len = with_b_data_len;
        *body_format = with_b_spec;
    } else if (with_ogg_data != NULL) {
        // Do we have a bare .ogg?
        // Mark as used.
        with_ogg_data_should_free = false;

        // Success!
        retval = true;

        // Store the data.
        *head_data = with_ogg_data;
        *head_data_len = with_ogg_data_len;
        *head_format = with_ogg_spec;
        *body_data = with_ogg_data;
        *body_data_len = with_ogg_data_len;
        *body_format = with_ogg_spec;
    } else if (with_h_data != NULL || with_b_data != NULL) {
        // Last ditch attempt at doing something useful...
        // Just store whatever we found as both head and body.
        retval = true;
        if (with_h_data != NULL) {
            with_h_data_should_free = false;
            *head_data = with_h_data;
            *head_data_len = with_h_data_len;
            *head_format = with_h_spec;
            *body_data = with_h_data;
            *body_data_len = with_h_data_len;
            *body_format = with_h_spec;
        } else {
            with_b_data_should_free = false;
            *head_data = with_b_data;
            *head_data_len = with_b_data_len;
            *head_format = with_b_spec;
            *body_data = with_b_data;
            *body_data_len = with_b_data_len;
            *body_format = with_b_spec;
        }
    }

    // Free the new strings and buffers that ended up unused.
    SDL_free(with_ogg);
    SDL_free(with_b_ogg);
    SDL_free(with_h_ogg);
    if (with_h_data_should_free) SDL_free(with_h_data);
    if (with_b_data_should_free) SDL_free(with_b_data);
    if (with_ogg_data_should_free) SDL_free(with_ogg_data);
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
        if (SDL_strcasecmp(name, "toggle_details") == 0) BUTTON_DETAILS = s;
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
        if (SDL_strcasecmp(name, "toggle_details") == 0) GAMEPAD_DETAILS = s;
    }

    if (SDL_strcasecmp(section, "music") == 0) {
        if (SDL_strcasecmp(name, "menu") == 0) {
            // Load menu...
            if (load_song(value, &TITLE_MUSIC.head.format, &TITLE_MUSIC.head.wave_data, &TITLE_MUSIC.head.wave_data_len, &TITLE_MUSIC.body.format, &TITLE_MUSIC.body.wave_data, &TITLE_MUSIC.body.wave_data_len)) {
                TITLE_MUSIC.level_start = -1;
            }
        } else {
            // Load section music...
            char *end;
            const long level_start = SDL_strtol(name, &end, 10);
            if (end == name) return 1;
            section_music_t s = {0};
            s.level_start = level_start;
            if (load_song(value, &s.head.format, &s.head.wave_data, &s.head.wave_data_len, &s.body.format, &s.body.wave_data, &s.body.wave_data_len)) {
                SECTION_COUNT++;
                SECTIONS = SDL_realloc(SECTIONS, SECTION_COUNT * sizeof(section_music_t));
                SDL_memcpy(SECTIONS + (SECTION_COUNT-1), &s, sizeof(section_music_t));
            }
        }
    }

    if (SDL_strcasecmp(section, "game") == 0) {
        char *end;
        const long v = SDL_strtol(value, &end, 10);
        if (end == value) return 1;

        if (SDL_strcasecmp(name, "render_scale") == 0) RENDER_SCALE = v;
        if (SDL_strcasecmp(name, "transparent_field") == 0) TRANSPARENCY = v != 0;
        if (SDL_strcasecmp(name, "audio_enabled") == 0) MUTED = v == 0;
        if (SDL_strcasecmp(name, "tgm3_kicks") == 0) TI_ARS = v != 0;
        if (SDL_strcasecmp(name, "bgm_volume") == 0) BGM_VOLUME = (float)v/100.0f;
        if (SDL_strcasecmp(name, "sfx_volume") == 0) SFX_VOLUME = (float)v/100.0f;
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
