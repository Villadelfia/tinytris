#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>
#include <stdint.h>
#include "ini.h"
#include "rng.h"
#include "game_data.h"
#include "game_config.h"
#include "input_utils.h"
#include "stb_vorbis.h"

// SDL stuff.
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Gamepad *gamepad = NULL;
static SDL_Texture *atlas_texture = NULL;
static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioStream *music = NULL;
#define FRAME_TIME (SDL_NS_PER_SECOND / SDL_SINT64_C(60))
static Uint64 last_time = 0;
static Sint64 accumulated_time = 0;

typedef struct {
    int16_t *wave_data;
    uint32_t wave_data_len;
    SDL_AudioStream *stream;
} sound_t;

// Global data.
block_t field[10][24] = {0};
block_type_t history[4] = {0};
block_type_t bag[35] = {0};
int histogram[7] = {0};
live_block_t next_piece[3] = {0};
live_block_t held_piece = {0};
live_block_t current_piece = {0};
game_state_t game_state = STATE_WAIT;
game_state_t game_state_old = STATE_WAIT;
int game_state_ctr = 60;
int game_state_ctr_old = 60;
float border_r = 0.1f;
float border_g = 0.1f;
float border_b = 0.9f;
float border_r_old = 0.1f;
float border_g_old = 0.1f;
float border_b_old = 0.9f;
int level = 0;
timing_t *current_timing;
int accumulated_g = 0;
int lines_cleared = 0;
int clears[24] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
bool jingle_played[24] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
bool mystery = false;
bool section_locked = false;
int previous_clears = 0;
bool first_piece = true;
bool intro = true;
bool in_roll = false;
bool can_hold = true;
int in_roll_remaining = 0;
float volume = 1.0f;
int garbage_ctr = -1;
section_music_t *last_section = NULL;
game_details_t game_details = {0};
int32_t time_spent_at_level[0xFFFF] = {0};
bool show_edge_during_lockflash = false;
int32_t show_edge_timer = 0;
bool use_tgm2_plus_sequence = false;
int sequence_index = 0;
int locked_settings[SETTINGS_COUNT] = {0};
int frozen_rows = 0;
bool frozen_ignore_next = false;
bool bones = false;
bool ti_garbage_quota = false;
bool big_mode = false;
bool big_mode_half_columns = false;
bool next_big_piece = false;
block_type_t next_subtype = BLOCK_VOID;
bool selective_gravity = false;
bool selective_gravity_by_item = false;
int old_w = 0;
int old_h = 0;
bool disable_selective_gravity_after_clear = false;
bool item_mode = false;
int items = 0;
int item_bag[32] = {0};
int item_hist[2] = {ITEM_NEGATE, ITEM_SHOTGUN};
char effect_overlay[10][30] = {0};
int effect_overlay_ctr = 0;
int xray_ctr = 0;
bool paused = false;
uint64_t id = 0;

sound_t lineclear_sound;
sound_t linecollapse_sound;
sound_t pieceland_sound;
sound_t piecelock_sound;
sound_t irs_sound;
sound_t ready_sound;
sound_t go_sound;
sound_t i_sound;
sound_t s_sound;
sound_t z_sound;
sound_t j_sound;
sound_t l_sound;
sound_t o_sound;
sound_t t_sound;
sound_t section_lock_sound;
sound_t section_pass_sound;
sound_t tetris_sound;
sound_t combo_sound;
sound_t tetris_b2b_sound;
sound_t gameover_sound;
sound_t complete_sound;
sound_t hold_sound;

int32_t button_u_held = 0;
int32_t button_l_held = 0;
int32_t button_d_held = 0;
int32_t button_r_held = 0;
int32_t button_a_held = 0;
int32_t button_b_held = 0;
int32_t button_c_held = 0;
int32_t button_hold_held = 0;
int32_t button_start_held = 0;
int32_t button_pause_held = 0;
int32_t button_reset_held = 0;
int32_t button_scale_up_held = 0;
int32_t button_scale_down_held = 0;
int32_t button_mute_held = 0;
int32_t button_mystery_held = 0;
int32_t button_hard_drop_held = 0;
int32_t button_toggle_transparency_held = 0;
int32_t button_details_held = 0;

void check_clears();
void wipe_clears();
void collapse_clears(bool sound);
void do_shotgun();
void do_laser();
void do_negate();
void do_del_lower();
void do_del_upper();
void do_del_even();
void do_push_left();
void do_push_right();
void do_push_down();
void do_180();
void do_big_block();
void do_antigravity();
void do_random_effect();
void spawn_random_effect();
void do_hard_block();
void do_roll_block();
void do_xray();
void do_heavy_block();

static int SDLCALL compare_int(const void *a, const void *b) {
    const int A = *(const int *)a;
    const int B = *(const int *)b;

    if (A < B) {
        return -1;
    } else if (B < A) {
        return 1;
    } else {
        return 0;
    }
}

void add_overlay(const char* what) {
    if (effect_overlay_ctr == 0) {
        SDL_memset(effect_overlay, 0, 10*30);
    }

    for (int i = 0; i < 10; ++i) {
        if (effect_overlay[i][0] == 0) {
            SDL_snprintf(effect_overlay[i], sizeof(effect_overlay[i])-1, what);
            effect_overlay_ctr = 120;
            break;
        }
    }
}

rotation_state_t get_rotation_state(const block_type_t piece, const int rotation_state) {
    return ROTATION_STATES[piece][rotation_state];
}

float lerp(const float a, const float b, const float f) {
    return (float)(a * (1.0 - f) + (b * f));
}

void apply_scale() {
    SDL_SetWindowSize(window, DETAILS ? 26*16*RENDER_SCALE : 13*16*RENDER_SCALE, 26*16*RENDER_SCALE);
    if (old_w != 0 && old_h != 0) {
        int diff_x = old_w - (DETAILS ? 26*16*RENDER_SCALE : 13*16*RENDER_SCALE);
        int diff_y = old_h - 26*16*RENDER_SCALE;
        diff_x /= 2;
        diff_y /= 2;
        if (diff_x != 0 || diff_y != 0) {
            int x, y;
            SDL_GetWindowPosition(window, &x, &y);
            SDL_SetWindowPosition(window, x+diff_x, y+diff_y);
        }
    }
    old_w = DETAILS ? 26*16*RENDER_SCALE : 13*16*RENDER_SCALE;
    old_h = 26*16*RENDER_SCALE;
    SDL_SetRenderScale(renderer, 1.0f * (float)RENDER_SCALE, 1.0f * (float)RENDER_SCALE);
}

void check_gamepad() {
    if (gamepad != NULL) {
        if (!SDL_GamepadConnected(gamepad)) {
            SDL_CloseGamepad(gamepad);
            gamepad = NULL;
        }
    }
    if (SDL_HasGamepad()) {
        gamepad = SDL_OpenGamepad(SDL_GetGamepads(NULL)[0]);
    }
}

void update_input_state(const bool *state, const int32_t scan_code, const SDL_GamepadButton button, int32_t *input_data) {
    check_gamepad();
    bool pressed = false;
    if (gamepad != NULL) {
        if (SDL_GetGamepadButton(gamepad, button)) pressed = true;
        if (button == GAMEPAD_SONIC_DROP && SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) < -10000) pressed = true;
        if (button == GAMEPAD_SOFT_DROP && SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) > 10000) pressed = true;
        if (button == GAMEPAD_LEFT && SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) < -10000) pressed = true;
        if (button == GAMEPAD_RIGHT && SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) > 10000) pressed = true;
    }
    if (state[scan_code]) pressed = true;

    if (pressed) {
        if (*input_data < INT32_MAX) {
            if (*input_data < 0) *input_data = 1;
            else (*input_data)++;
        }
    } else {
        if (*input_data > INT32_MIN + 1) {
            if (*input_data > 0) *input_data = -1;
            else (*input_data)--;
        }
    }
}

void update_input_states(bool p) {
    const bool* state = SDL_GetKeyboardState(NULL);
    if (!p) {
        update_input_state(state, BUTTON_LEFT, GAMEPAD_LEFT, &button_l_held);
        update_input_state(state, BUTTON_RIGHT, GAMEPAD_RIGHT, &button_r_held);

        update_input_state(state, BUTTON_SONIC_DROP, GAMEPAD_SONIC_DROP, &button_u_held);
        update_input_state(state, BUTTON_SOFT_DROP, GAMEPAD_SOFT_DROP, &button_d_held);
        update_input_state(state, BUTTON_HARD_DROP, GAMEPAD_HARD_DROP, &button_hard_drop_held);

        update_input_state(state, BUTTON_CCW_1, GAMEPAD_CCW_1, &button_a_held);
        update_input_state(state, BUTTON_CCW_2, GAMEPAD_CCW_2, &button_c_held);
        update_input_state(state, BUTTON_CW, GAMEPAD_CW, &button_b_held);
        update_input_state(state, BUTTON_HOLD, GAMEPAD_HOLD, &button_hold_held);

        update_input_state(state, BUTTON_RESET, GAMEPAD_RESET, &button_reset_held);
        update_input_state(state, BUTTON_START, GAMEPAD_START, &button_start_held);

        update_input_state(state, BUTTON_SCALE_UP, GAMEPAD_SCALE_UP, &button_scale_up_held);
        update_input_state(state, BUTTON_SCALE_DOWN, GAMEPAD_SCALE_DOWN, &button_scale_down_held);
        update_input_state(state, BUTTON_TOGGLE_TRANSPARENCY, GAMEPAD_TOGGLE_TRANSPARENCY, &button_toggle_transparency_held);
        update_input_state(state, BUTTON_MUTE, GAMEPAD_MUTE, &button_mute_held);
        update_input_state(state, BUTTON_DETAILS, GAMEPAD_DETAILS, &button_details_held);

        update_input_state(state, BUTTON_MYSTERY, GAMEPAD_MYSTERY, &button_mystery_held);
    }
    update_input_state(state, BUTTON_PAUSE, GAMEPAD_PAUSE, &button_pause_held);
}

void play_sound(const sound_t *sound) {
    if (MUTED || SFX_VOLUME == 0.0f) return;
    if (sound->stream == NULL) return;
    SDL_ClearAudioStream(sound->stream);
    SDL_PutAudioStreamData(sound->stream, sound->wave_data, (int)sound->wave_data_len);
}

void check_garbage() {
    if (current_timing->garbage == 0) {
        garbage_ctr = -1;
        return;
    } else {
        if (garbage_ctr >= 0) return;
        garbage_ctr = current_timing->garbage;
        if (garbage_ctr < 0) garbage_ctr = -garbage_ctr;
        garbage_ctr--;
    }
}

void add_garbage() {
    garbage_ctr--;
    if (item_mode) {
        spawn_random_effect();
        check_garbage();
        return;
    }
    bool all_empty =
        (field[0][23].type == BLOCK_VOID) &&
        (field[1][23].type == BLOCK_VOID) &&
        (field[2][23].type == BLOCK_VOID) &&
        (field[3][23].type == BLOCK_VOID) &&
        (field[4][23].type == BLOCK_VOID) &&
        (field[5][23].type == BLOCK_VOID) &&
        (field[6][23].type == BLOCK_VOID) &&
        (field[7][23].type == BLOCK_VOID) &&
        (field[8][23].type == BLOCK_VOID) &&
        (field[9][23].type == BLOCK_VOID);
    if (all_empty) return;

    // Copy everything up.
    for (int i = 0; i < 23; ++i) {
        field[0][i] = field[0][i+1];
        field[1][i] = field[1][i+1];
        field[2][i] = field[2][i+1];
        field[3][i] = field[3][i+1];
        field[4][i] = field[4][i+1];
        field[5][i] = field[5][i+1];
        field[6][i] = field[6][i+1];
        field[7][i] = field[7][i+1];
        field[8][i] = field[8][i+1];
        field[9][i] = field[9][i+1];
    }

    block_t garbage = {
        .type = bones ? BLOCK_BONE : BLOCK_X,
        .subtype = BLOCK_VOID,
        .lock_status = LOCK_LOCKED,
        .lock_param = 0,
        .locked_at = game_details.total_frames,
        .fading = false,
        .fade_state = 1.0f
    };
    block_t empty = {0};

    if (use_tgm2_plus_sequence) {
        char *garbage_ptr = tgm2_plus_sequence[sequence_index++];
        sequence_index %= 24;

        for (int i = 0; i < 10; ++i) {
            if (garbage_ptr[i] == ' ') field[i][23] = empty;
            else field[i][23] = garbage;
        }

        check_garbage();
        return;
    }

    bool all_garbage =
        (field[0][23].type == BLOCK_VOID || field[0][23].type == BLOCK_X) &&
        (field[1][23].type == BLOCK_VOID || field[1][23].type == BLOCK_X) &&
        (field[2][23].type == BLOCK_VOID || field[2][23].type == BLOCK_X) &&
        (field[3][23].type == BLOCK_VOID || field[3][23].type == BLOCK_X) &&
        (field[4][23].type == BLOCK_VOID || field[4][23].type == BLOCK_X) &&
        (field[5][23].type == BLOCK_VOID || field[5][23].type == BLOCK_X) &&
        (field[6][23].type == BLOCK_VOID || field[6][23].type == BLOCK_X) &&
        (field[7][23].type == BLOCK_VOID || field[7][23].type == BLOCK_X) &&
        (field[8][23].type == BLOCK_VOID || field[8][23].type == BLOCK_X) &&
        (field[9][23].type == BLOCK_VOID || field[9][23].type == BLOCK_X);

    // Make bottom row garbage.
    for (int i = 0; i < 10; ++i) {
        if (current_timing->garbage < 0 && !all_garbage) {
            if (field[i][23].type != BLOCK_VOID) field[i][23] = empty;
            else field[i][23] = garbage;
        } else {
            if (field[i][23].type != BLOCK_VOID) field[i][23] = garbage;
        }
    }

    check_garbage();
}

int32_t time_spent(int32_t from, int32_t to) {
    if (from < 0) from = 0;
    if (to <= from) return 0;
    int32_t sum = 0;
    for (; from < to; from++) {
        sum += time_spent_at_level[from];
    }
    return sum;
}

void fill_item_bag() {
    items = 32;

    item_bag[0] = ITEM_SHOTGUN;

    item_bag[1] = ITEM_ROLL;
    item_bag[2] = ITEM_ROLL;

    item_bag[3] = ITEM_XRAY;
    item_bag[4] = ITEM_XRAY;
    item_bag[5] = ITEM_XRAY;

    item_bag[6] = ITEM_LASER;
    item_bag[7] = ITEM_LASER;
    item_bag[8] = ITEM_LASER;

    item_bag[9] = ITEM_180;

    item_bag[10] = ITEM_NEGATE;

    item_bag[11] = ITEM_ANTIGRAVITY;
    item_bag[12] = ITEM_ANTIGRAVITY;
    item_bag[13] = ITEM_ANTIGRAVITY;

    item_bag[14] = ITEM_HEAVY;
    item_bag[15] = ITEM_HEAVY;
    item_bag[16] = ITEM_HEAVY;

    item_bag[17] = ITEM_BIG_BLOCK;
    item_bag[18] = ITEM_BIG_BLOCK;

    item_bag[19] = ITEM_HARD;
    item_bag[20] = ITEM_HARD;
    item_bag[21] = ITEM_HARD;

    item_bag[22] = ITEM_PUSH_LEFT;
    item_bag[23] = ITEM_PUSH_LEFT;
    item_bag[24] = ITEM_PUSH_LEFT;

    item_bag[25] = ITEM_PUSH_RIGHT;
    item_bag[26] = ITEM_PUSH_RIGHT;
    item_bag[27] = ITEM_PUSH_RIGHT;

    item_bag[28] = ITEM_PUSH_DOWN;

    item_bag[29] = ITEM_DEL_UPPER;

    item_bag[30] = ITEM_DEL_LOWER;

    item_bag[31] = ITEM_DEL_EVEN;
}

int generate_random_effect() {
    int divisor = 32;
    if (items <= 16) divisor = 16;
    if (items <= 8) divisor = 8;
    if (items <= 4) divisor = 4;
    if (items <= 2) divisor = 2;
    int selection = (int)(uint8_t)xoroshiro_next() % divisor;
    while (selection >= items) selection = (int)(uint8_t)xoroshiro_next() % divisor;
    int result = item_bag[selection];
    int tries = 0;
    while ((item_hist[0] == result || item_hist[1] == result) && tries < 3) {
        selection = (int)(uint8_t)xoroshiro_next() % divisor;
        while (selection >= items) selection = (int)(uint8_t)xoroshiro_next() % divisor;
        result = item_bag[selection];
        tries++;
    }
    item_hist[0] = item_hist[1];
    item_hist[1] = result;
    item_bag[selection] = INT32_MAX;
    SDL_qsort(item_bag, 32, sizeof(int), compare_int);

    items--;
    if (items == 0) fill_item_bag();

    return result;
}

void spawn_random_effect() {
    next_subtype = generate_random_effect();
}

void do_random_effect() {
    int result = generate_random_effect();

    if (result == ITEM_SHOTGUN) do_shotgun();
    if (result == ITEM_LASER) do_laser();
    if (result == ITEM_NEGATE) do_negate();
    if (result == ITEM_DEL_UPPER) do_del_upper();
    if (result == ITEM_DEL_LOWER) do_del_lower();
    if (result == ITEM_DEL_EVEN) do_del_even();
    if (result == ITEM_PUSH_LEFT) do_push_left();
    if (result == ITEM_PUSH_RIGHT) do_push_right();
    if (result == ITEM_PUSH_DOWN) do_push_down();
    if (result == ITEM_180) do_180();
    if (result == ITEM_BIG_BLOCK) do_big_block();
    if (result == ITEM_HARD) do_hard_block();
    if (result == ITEM_ROLL) do_roll_block();
    if (result == ITEM_ANTIGRAVITY) do_antigravity();
    if (result == ITEM_HEAVY) do_heavy_block();
    if (result == ITEM_XRAY) do_xray();
}

void check_effect() {
    int32_t effect = current_timing->effect;
    const int32_t torikan = current_timing->torikan;
    bool torikan_scope = (effect & TORIKAN_SCOPE_MASK) != 0;
    bool torikan_effect = (effect & TORIKAN_EFFECT_MASK) != 0;
    bool clear_field = (effect & CLEAR_FIELD_MASK) != 0;
    bool reset_visibility = (effect & RESET_VISIBILITY_MASK) != 0;
    bool reset_visibility_timer = (effect & RESET_VISIBILITY_TIMER_MASK) != 0;
    bool invisibility_hint_once = (effect & INVISIBILITY_HINT_ONCE_MASK) != 0;
    bool invisibility_hint_flash = (effect & INVISIBILITY_HINT_FLASH_MASK) != 0;
    bool tgm2_plus_sequence = (effect & TGM2_PLUS_SEQUENCE_MASK) != 0;
    bool should_do_shotgun = (effect & FIELD_SHOTGUN_MASK) != 0;
    bool should_do_laser = (effect & FIELD_LASER_MASK) != 0;
    bool should_do_negate = (effect & FIELD_NEGATE_MASK) != 0;
    bool should_do_del_upper = (effect & FIELD_DEL_UPPER_MASK) != 0;
    bool should_do_del_lower = (effect & FIELD_DEL_LOWER_MASK) != 0;
    bool should_do_del_even = (effect & FIELD_DEL_EVEN_MASK) != 0;
    bool should_do_push_left = (effect & FIELD_PUSH_LEFT_MASK) != 0;
    bool should_do_push_right = (effect & FIELD_PUSH_RIGHT_MASK) != 0;
    bool should_do_push_down = (effect & FIELD_PUSH_DOWN_MASK) != 0;
    bool should_do_180 = (effect & FIELD_180_MASK) != 0;
    bool should_do_heavy = (effect & HEAVY_BLOCK_MASK) != 0;
    bool should_do_xray = (effect & XRAY_MASK) != 0;
    selective_gravity = (effect & SELECTIVE_GRAVITY_MASK) != 0;
    if (selective_gravity) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) {
            next_subtype = ITEM_ANTIGRAVITY;
            selective_gravity = false;
        } else {
            do_antigravity();
            disable_selective_gravity_after_clear = false;
        }
    }

    int prev_frozen = frozen_rows;
    frozen_rows = current_timing->freeze;
    if (frozen_rows < 0) frozen_rows = 0;
    if (frozen_rows > 21) frozen_rows = 21;
    for (int i = 0; i < 24 - frozen_rows; ++i) jingle_played[i] = false;
    frozen_ignore_next = (effect & FROZEN_RESET_MASK) != 0;
    if (frozen_ignore_next) SDL_memset(jingle_played, 0, sizeof jingle_played);
    bones = (effect & BONES_MASK) != 0;
    ti_garbage_quota = (effect & TI_GARBAGE_QUOTA) != 0;
    big_mode = ((effect & BIG_MODE_MASK) != 0 || get_setting_value(BIG_MODE_SETTING_IDX));
    if ((effect & BIG_PIECE_MASK) != 0) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_BIG_BLOCK;
        else do_big_block();
    }
    if ((effect & HARD_BLOCK_MASK) != 0) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_HARD;
        else do_hard_block();
    }
    if ((effect & ROLL_BLOCK_MASK) != 0) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_ROLL;
        else do_roll_block();
    }
    big_mode_half_columns = (effect & BIG_MODE_HALF_PIECE_MASK) != 0;
    item_mode = (effect & ITEM_MODE_MASK) != 0;

    if ((effect & RANDOM_EFFECT_MASK) != 0) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) spawn_random_effect();
        else do_random_effect();
    }

    if (torikan != 0) {
        bool hit_torikan = game_details.total_frames > torikan;
        if (torikan_scope) hit_torikan = time_spent(level-100, level) > torikan;
        if (hit_torikan) {
            if (torikan_effect) {
                // Gameover
                current_piece.type = BLOCK_VOID;
                game_state = STATE_GAMEOVER;
                game_state_ctr = 10 * 24 + 1;
                play_sound(&gameover_sound);
                return;
            } else {
                // Roll
                current_timing = &CREDITS_ROLL_TIMING;
                garbage_ctr = 0;
                check_garbage();
                effect = current_timing->effect;
                clear_field = (effect & CLEAR_FIELD_MASK) != 0;
                reset_visibility = (effect & RESET_VISIBILITY_MASK) != 0;
                reset_visibility_timer = (effect & RESET_VISIBILITY_TIMER_MASK) != 0;
                invisibility_hint_once = (effect & INVISIBILITY_HINT_ONCE_MASK) != 0;
                invisibility_hint_flash = (effect & INVISIBILITY_HINT_FLASH_MASK) != 0;
                in_roll = true;
                game_state_ctr = 0;
                game_state = STATE_FADEOUT;
                play_sound(&complete_sound);
            }
        }
    }

    if (clear_field) {
        SDL_memset(field, 0, sizeof field);
    }

    if (reset_visibility || reset_visibility_timer) {
        for (int x = 0; x < 10; x++) {
            for (int y = 0; y < 24; y++) {
                if (reset_visibility || field[x][y].fading == false) {
                    field[x][y].fade_state = 1.0f;
                    field[x][y].fading = false;
                    field[x][y].locked_at = game_details.total_frames;
                }
            }
        }
    }

    if (should_do_shotgun) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_SHOTGUN;
        else do_shotgun();
    }
    if (should_do_laser) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_LASER;
        else do_laser();
    }
    if (should_do_negate) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_NEGATE;
        else do_negate();
    }
    if (should_do_del_even) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_DEL_EVEN;
        else do_del_even();
    }
    if (should_do_del_lower) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_DEL_LOWER;
        else do_del_lower();
    }
    if (should_do_del_upper) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_DEL_UPPER;
        else do_del_upper();
    }
    if (should_do_push_left) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_PUSH_LEFT;
        else do_push_left();
    }
    if (should_do_push_right) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_PUSH_RIGHT;
        else do_push_right();
    }
    if (should_do_push_down) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_PUSH_DOWN;
        else do_push_down();
    }
    if (should_do_180) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_180;
        else do_180();
    }
    if (should_do_heavy) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_HEAVY;
        else do_heavy_block();
    }
    if (should_do_xray) {
        if ((effect & EFFECT_AS_ITEM_MASK) != 0) next_subtype = ITEM_XRAY;
        else do_xray();
    }

    if (invisibility_hint_once) {
        show_edge_timer = 60;
    }

    show_edge_during_lockflash = invisibility_hint_flash;

    if (tgm2_plus_sequence && !use_tgm2_plus_sequence) sequence_index = 0;
    use_tgm2_plus_sequence = tgm2_plus_sequence;

    if ((frozen_rows != 0 && frozen_ignore_next) || (frozen_rows != 0 && prev_frozen > frozen_rows) || (prev_frozen != 0 && frozen_rows == 0)) {
        int old_clears = lines_cleared;
        if (old_clears != 0) {
            wipe_clears();
            collapse_clears(false);
        }
        check_clears();
        if (old_clears == 0 && lines_cleared != 0) {
            wipe_clears();
            collapse_clears(true);
        }
    }
}

void increment_level_piece_spawn() {
    if (section_locked) return;
    level++;
    if (!in_roll && (current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) {
        current_timing = current_timing+1;
        check_garbage();
        check_effect();
        if (lines_cleared != 0) {
            game_state = STATE_LOCKFLASH;
            game_state_ctr = 3;
        }
    }
}

void increment_level_line_clear(int lines) {
    if (lines == 0) {
        if (level % 100 == 99 || level == CREDITS_ROLL_TIMING.level-1) {
            if (!section_locked) {
                play_sound(&section_lock_sound);
                section_locked = true;
            }
        }
        previous_clears = 0;

        if (current_timing->garbage != 0) {
            if (garbage_ctr <= 0) add_garbage();
            else garbage_ctr--;
        }
        return;
    }

    if (big_mode && lines > 1) lines /= 2;

    int actual_lines = lines;
    if (get_setting_value(RNG_SETTING_IDX) == 2 && get_setting_value(ROTATION_SETTING_IDX) == 1 && lines > 2) {
        // Generalized the TGM3 line clear formula to work with frozen lines.
        actual_lines = lines + (lines - 2);
    }

    if (current_timing->garbage != 0 && ti_garbage_quota) {
        garbage_ctr += actual_lines-1;
        if (garbage_ctr > current_timing->garbage) garbage_ctr = current_timing->garbage;
    }

    section_locked = false;
    if (lines > 4) {
        if (previous_clears > 4) play_sound(&tetris_b2b_sound);
        else play_sound(&tetris_sound);
    }

    if (previous_clears != 0 && lines >= 2) {
        // Avoid playing tetris_b2b + combo. Tetris + combo is allowed.
        if (lines < 4 || previous_clears < 4) play_sound(&combo_sound);
    }
    previous_clears = lines;

    if ((level / 100) < ((level+actual_lines) / 100)) play_sound(&section_pass_sound);
    level += actual_lines;
    if (level > CREDITS_ROLL_TIMING.level) level = CREDITS_ROLL_TIMING.level;
    if (!in_roll && (current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) {
        current_timing = current_timing+1;
        check_garbage();
        check_effect();
    }

    if (!in_roll && CREDITS_ROLL_TIMING.level == level) {
        current_timing = &CREDITS_ROLL_TIMING;
        garbage_ctr = 0;
        check_garbage();
        check_effect();
        in_roll = true;
        game_state_ctr = 0;
        game_state = STATE_FADEOUT;
        play_sound(&complete_sound);
    }
}

bool is_empty(int x, int y) {
    if (x < 0) return false;
    if (x > 9) return false;
    if (y > 23) return false;
    if (y < 0) return true;
    return field[x][y].type == BLOCK_VOID;
}

int piece_collides(const live_block_t piece) {
    if (piece.type == BLOCK_VOID) return -1;

    const rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (rotation[4*j + i] != ' ' && piece.y + j + 1 >= 0) {
                if (big_mode || piece.is_big) {
                    if (!is_empty(piece.x + 2*i, piece.y + 2*j) || !is_empty(piece.x + 2*i + 1, piece.y + 2*j) || !is_empty(piece.x + 2*i, piece.y + 2*j + 1) || !is_empty(piece.x + 2*i + 1, piece.y + 2*j + 1)) {
                        return 4*j+i;
                    }
                } else {
                    if (!is_empty(piece.x + i, piece.y + j + 1)) {
                        return 4*j+i;
                    }
                }
            }
        }
    }

    return -1;
}

void write_piece(const live_block_t piece) {
    if (piece.type == BLOCK_VOID) return;

    const rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (big_mode || piece.is_big) {
                if (rotation[4*j + i] != ' ' && piece.x + 2*i >= 0 && piece.x + 2*i < 10 && piece.y + 2*j >= 0 && piece.y + 2*j < 24) {
                    field[piece.x + 2*i][piece.y + 2*j].type = piece.is_bone ? BLOCK_BONE : piece.type;
                    field[piece.x + 2*i][piece.y + 2*j].subtype = piece.subtype;
                    field[piece.x + 2*i][piece.y + 2*j].lock_param = 1.0f;
                    field[piece.x + 2*i][piece.y + 2*j].lock_status = LOCK_LOCKED;
                    field[piece.x + 2*i][piece.y + 2*j].locked_at = game_details.total_frames;
                    field[piece.x + 2*i][piece.y + 2*j].fading = false;
                    field[piece.x + 2*i][piece.y + 2*j].fade_state = 1.0f;
                    field[piece.x + 2*i][piece.y + 2*j].id = id;
                }
                if (rotation[4*j + i] != ' ' && piece.x + 2*i >= 0 && piece.x + 2*i < 10 && piece.y + 2*j + 1 >= 0 && piece.y + 2*j + 1 < 24) {
                    field[piece.x + 2*i][piece.y + 2*j + 1].type = piece.is_bone ? BLOCK_BONE : piece.type;
                    field[piece.x + 2*i][piece.y + 2*j + 1].subtype = piece.subtype;
                    field[piece.x + 2*i][piece.y + 2*j + 1].lock_param = 1.0f;
                    field[piece.x + 2*i][piece.y + 2*j + 1].lock_status = LOCK_LOCKED;
                    field[piece.x + 2*i][piece.y + 2*j + 1].locked_at = game_details.total_frames;
                    field[piece.x + 2*i][piece.y + 2*j + 1].fading = false;
                    field[piece.x + 2*i][piece.y + 2*j + 1].fade_state = 1.0f;
                    field[piece.x + 2*i][piece.y + 2*j + 1].id = id;
                }
                if (rotation[4*j + i] != ' ' && piece.x + 2*i >= 0 && piece.x + 2*i + 1 < 10 && piece.y + 2*j >= 0 && piece.y + 2*j < 24) {
                    field[piece.x + 2*i + 1][piece.y + 2*j].type = piece.is_bone ? BLOCK_BONE : piece.type;
                    field[piece.x + 2*i + 1][piece.y + 2*j].subtype = piece.subtype;
                    field[piece.x + 2*i + 1][piece.y + 2*j].lock_param = 1.0f;
                    field[piece.x + 2*i + 1][piece.y + 2*j].lock_status = LOCK_LOCKED;
                    field[piece.x + 2*i + 1][piece.y + 2*j].locked_at = game_details.total_frames;
                    field[piece.x + 2*i + 1][piece.y + 2*j].fading = false;
                    field[piece.x + 2*i + 1][piece.y + 2*j].fade_state = 1.0f;
                    field[piece.x + 2*i + 1][piece.y + 2*j].id = id;
                }
                if (rotation[4*j + i] != ' ' && piece.x + 2*i >= 0 && piece.x + 2*i + 1 < 10 && piece.y + 2*j + 1 >= 0 && piece.y + 2*j + 1 < 24) {
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].type = piece.is_bone ? BLOCK_BONE : piece.type;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].subtype = piece.subtype;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].lock_param = 1.0f;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].lock_status = LOCK_LOCKED;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].locked_at = game_details.total_frames;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].fading = false;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].fade_state = 1.0f;
                    field[piece.x + 2*i + 1][piece.y + 2*j + 1].id = id;
                }
            } else {
                if (rotation[4*j + i] != ' ' && piece.x + i >= 0 && piece.x + i < 10 && piece.y + j + 1 >= 0 && piece.y + j + 1 < 24) {
                    field[piece.x + i][piece.y + j + 1].type = piece.is_bone ? BLOCK_BONE : piece.type;
                    field[piece.x + i][piece.y + j + 1].subtype = piece.subtype;
                    field[piece.x + i][piece.y + j + 1].lock_param = 1.0f;
                    field[piece.x + i][piece.y + j + 1].lock_status = LOCK_LOCKED;
                    field[piece.x + i][piece.y + j + 1].locked_at = game_details.total_frames;
                    field[piece.x + i][piece.y + j + 1].fading = false;
                    field[piece.x + i][piece.y + j + 1].fade_state = 1.0f;
                    field[piece.x + i][piece.y + j + 1].id = id;
                }
            }
        }
    }

    id++;
}

void try_move(const int direction) {
    if (current_piece.type == BLOCK_VOID) return;

    live_block_t active = current_piece;
    active.x += direction;
    if (piece_collides(active) == -1) {
        current_piece.x += direction;
    } else if (mystery) {
        int x, y;
        if (SDL_GetWindowPosition(window, &x, &y)) {
            SDL_SetWindowPosition(window, x+(direction*(RENDER_SCALE*16)), y);
        }
    }
}

void try_descend() {
    if (current_piece.type == BLOCK_VOID) return;

    live_block_t active = current_piece;
    active.y++;
    if (piece_collides(active) == -1) current_piece.y++;
}

bool piece_grounded() {
    if (current_piece.type == BLOCK_VOID) return false;

    live_block_t active = current_piece;
    active.y++;
    return piece_collides(active) != -1;
}

bool touching_stack() {
    if (current_piece.type == BLOCK_VOID) return false;

    live_block_t active = current_piece;
    active.y++;
    if (piece_collides(active) != -1) return true;
    active = current_piece;
    active.y--;
    if (piece_collides(active) != -1) return true;
    active = current_piece;
    active.x++;
    if (piece_collides(active) != -1) return true;
    active = current_piece;
    active.x--;
    if (piece_collides(active) != -1) return true;

    return false;
}

void try_rotate(const int direction) {
    if (current_piece.type == BLOCK_VOID) return;

    live_block_t active = current_piece;
    active.rotation_state += direction;
    if (active.rotation_state < 0) active.rotation_state = 3;
    if (active.rotation_state > 3) active.rotation_state = 0;

    const int collision = piece_collides(active);
    if (collision == -1) {
        current_piece.rotation_state = active.rotation_state;
        return;
    }

    if (active.type == BLOCK_O) return;

    if (active.type == BLOCK_J || active.type == BLOCK_L || active.type == BLOCK_T) {
        if (collision == 1 || collision == 5 || collision == 9) return;
    }

    if (active.type == BLOCK_I) {
        if (get_setting_value(ROTATION_SETTING_IDX) == 0) return;
        if (touching_stack() && (active.rotation_state == 0 || active.rotation_state == 2)) {
            // I -> right
            active.x += 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x += 1;
                return;
            }

            // I -> 2right
            active.x += 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x += 2;
                return;
            }

            // Try 3 and 4 for big mode.
            if (big_mode || current_piece.is_big) {
                active.x += 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.x += 3;
                    return;
                }

                active.x += 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.x += 4;
                    return;
                }
            }

            // I -> left
            active.x = current_piece.x - 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x -= 1;
            }

            if (big_mode || current_piece.is_big) {
                active.x -= 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.x -= 2;
                }
            }
        } else if (piece_grounded() && (active.rotation_state == 1 || active.rotation_state == 3)) {
            // I -> up
            active.y -= 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.y -= 1;
                current_piece.floor_kicked = true;
                return;
            }

            // I -> 2up
            active.y -= 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.y -= 2;
                current_piece.floor_kicked = true;
            }

            // Try 3 and 4 for big mode.
            if (big_mode || current_piece.is_big) {
                active.y -= 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.y -= 3;
                    current_piece.floor_kicked = true;
                    return;
                }

                active.y -= 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.y -= 4;
                    current_piece.floor_kicked = true;
                    return;
                }
            }
        }
    } else {
        // right
        active.x += 1;
        if (piece_collides(active) == -1) {
            current_piece.rotation_state = active.rotation_state;
            current_piece.x += 1;
            return;
        }

        if (big_mode || current_piece.is_big) {
            active.x += 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x += 2;
                return;
            }
        }

        // left
        active.x = current_piece.x - 1;
        if (piece_collides(active) == -1) {
            current_piece.rotation_state = active.rotation_state;
            current_piece.x -= 1;
            return;
        }

        if (big_mode || current_piece.is_big) {
            active.x -= 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x -= 2;
            }
        }

        // T -> up
        if (get_setting_value(ROTATION_SETTING_IDX) == 0) return;
        if (current_piece.type == BLOCK_T && piece_grounded()) {
            active.x = current_piece.x;
            active.y -= 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.y -= 1;
                current_piece.floor_kicked = true;
            }

            if (big_mode || current_piece.is_big) {
                active.y -= 1;
                if (piece_collides(active) == -1) {
                    current_piece.rotation_state = active.rotation_state;
                    current_piece.y -= 2;
                    current_piece.floor_kicked = true;
                }
            }
        }
    }
}

int get_first_occupied_row() {
    int first_occupied = -1;

    for (int i = 0; i < 24; ++i) {
        if (
            field[0][i].type != BLOCK_VOID ||
            field[1][i].type != BLOCK_VOID ||
            field[2][i].type != BLOCK_VOID ||
            field[3][i].type != BLOCK_VOID ||
            field[4][i].type != BLOCK_VOID ||
            field[5][i].type != BLOCK_VOID ||
            field[6][i].type != BLOCK_VOID ||
            field[7][i].type != BLOCK_VOID ||
            field[8][i].type != BLOCK_VOID ||
            field[9][i].type != BLOCK_VOID) {
            first_occupied = i;
            break;
            }
    }

    return first_occupied;
}

void do_shotgun() {
    add_overlay("Shotgun!");

    int block_cnt = 0;
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 24; ++y) {
            if (field[x][y].type != BLOCK_VOID) {
                block_cnt++;
            }
        }
    }

    block_cnt /= 6;
    while (block_cnt > 0) {
        int x = (int)(uint8_t)xoroshiro_next() % 16;
        int y = (int)(uint8_t)xoroshiro_next() % 32;
        while (x >= 10) x = (int)(uint8_t)xoroshiro_next() % 16;
        while (y >= 24) y = (int)(uint8_t)xoroshiro_next() % 32;
        if (field[x][y].type != BLOCK_VOID) {
            field[x][y].type = BLOCK_VOID;
            field[x][y].subtype = BLOCK_VOID;
            block_cnt--;
        }
    }
}


typedef struct {
    int cells;
    int column;
} laser_t;

static int SDLCALL compare_laser(const void *a, const void *b) {
    const laser_t *A = (const laser_t *)a;
    const laser_t *B = (const laser_t *)b;

    if (A->cells < B->cells) {
        return -1;
    } else if (B->cells < A->cells) {
        return 1;
    } else {
        return 0;
    }
}

void do_laser() {
    add_overlay("Laser!");
    laser_t data[10] = {
        {(uint16_t)xoroshiro_next(), 0},
        {(uint16_t)xoroshiro_next(), 1},
        {(uint16_t)xoroshiro_next(), 2},
        {(uint16_t)xoroshiro_next(), 3},
        {(uint16_t)xoroshiro_next(), 4},
        {(uint16_t)xoroshiro_next(), 5},
        {(uint16_t)xoroshiro_next(), 6},
        {(uint16_t)xoroshiro_next(), 7},
        {(uint16_t)xoroshiro_next(), 8},
        {(uint16_t)xoroshiro_next(), 9},
    };

    SDL_qsort(data, 10, sizeof(laser_t), compare_laser);
    for (int i = 0; i < 10; ++i) {
        int ct = 0;
        for (int y = 0; y < 24; ++y) {
            if (field[data[i].column][y].type != BLOCK_VOID) ct++;
        }
        data[i].cells = ct;
    }
    SDL_qsort(data, 10, sizeof(laser_t), compare_laser);

    int column = data[2].column;
    for (int y = 0; y < 24; ++y) {
        field[column][y].type = BLOCK_VOID;
        field[column][y].subtype = BLOCK_VOID;
    }
}

void do_negate() {
    add_overlay("Negate Field");
    int first_occupied = get_first_occupied_row();

    if (first_occupied == -1) return;

    for (int x = 0; x < 10; ++x) {
        for (int y = first_occupied; y < 24; ++y) {
            if (field[x][y].type != BLOCK_VOID) {
                field[x][y].type = BLOCK_VOID;
                field[x][y].subtype = BLOCK_VOID;
            } else {
                field[x][y].type = BLOCK_X;
                field[x][y].subtype = BLOCK_VOID;
            }
        }
    }
}

void wipe_subtype_by_id(uint64_t block_id) {
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 24; ++y) {
            if (field[x][y].id == block_id) field[x][y].subtype = BLOCK_VOID;
        }
    }
}

void check_clears() {
    for (int i = 0; i < 24; ++i) clears[i] = -1;

    int should_do_hard_block = -1;
    int should_do_shotgun = -1;
    int should_do_laser = -1;
    int should_do_negate = -1;
    int should_do_del_upper = -1;
    int should_do_del_lower = -1;
    int should_do_del_even = -1;
    int should_do_push_left = -1;
    int should_do_push_right = -1;
    int should_do_push_down = -1;
    int should_do_180 = -1;
    int should_do_big_block = -1;
    int should_do_antigravity = -1;
    int should_do_roll_block = -1;
    int should_do_heavy_block = -1;
    int should_do_xray = -1;

    int ct = 0;
    for (int i = 0; i < (24 - (frozen_ignore_next ? 0 : frozen_rows)); i++) {
        if (
            field[0][i].type != BLOCK_VOID &&
            field[1][i].type != BLOCK_VOID &&
            field[2][i].type != BLOCK_VOID &&
            field[3][i].type != BLOCK_VOID &&
            field[4][i].type != BLOCK_VOID &&
            field[5][i].type != BLOCK_VOID &&
            field[6][i].type != BLOCK_VOID &&
            field[7][i].type != BLOCK_VOID &&
            field[8][i].type != BLOCK_VOID &&
            field[9][i].type != BLOCK_VOID) {
            for (int x = 0; x < 10; ++x) {
                if (field[x][i].subtype != BLOCK_VOID && field[x][i].subtype != BLOCK_HARD) {
                    block_type_t subtype = field[x][i].subtype;

                    if (subtype == ITEM_HARD && should_do_hard_block == -1) should_do_hard_block = (10*i)+x;
                    if (subtype == ITEM_SHOTGUN && should_do_shotgun == -1) should_do_shotgun = (10*i)+x;
                    if (subtype == ITEM_LASER && should_do_laser == -1) should_do_laser = (10*i)+x;
                    if (subtype == ITEM_NEGATE && should_do_negate == -1) should_do_negate = (10*i)+x;
                    if (subtype == ITEM_DEL_UPPER && should_do_del_upper == -1) should_do_del_upper = (10*i)+x;
                    if (subtype == ITEM_DEL_LOWER && should_do_del_lower == -1) should_do_del_lower = (10*i)+x;
                    if (subtype == ITEM_DEL_EVEN && should_do_del_even == -1) should_do_del_even = (10*i)+x;
                    if (subtype == ITEM_PUSH_LEFT && should_do_push_left == -1) should_do_push_left = (10*i)+x;
                    if (subtype == ITEM_PUSH_RIGHT && should_do_push_right == -1) should_do_push_right = (10*i)+x;
                    if (subtype == ITEM_PUSH_DOWN && should_do_push_down == -1) should_do_push_down = (10*i)+x;
                    if (subtype == ITEM_180 && should_do_180 == -1) should_do_180 = (10*i)+x;
                    if (subtype == ITEM_BIG_BLOCK && should_do_big_block == -1) should_do_big_block = (10*i)+x;
                    if (subtype == ITEM_ANTIGRAVITY && should_do_antigravity == -1) should_do_antigravity = (10*i)+x;
                    if (subtype == ITEM_ROLL && should_do_roll_block == -1) should_do_roll_block = (10*i)+x;
                    if (subtype == ITEM_HEAVY && should_do_heavy_block == -1) should_do_heavy_block = (10*i)+x;
                    if (subtype == ITEM_XRAY && should_do_xray == -1) should_do_xray = (10*i)+x;

                    wipe_subtype_by_id(field[x][i].id);
                }
            }
            clears[ct++] = i;
        }
    }

    lines_cleared = ct;

    for (int i = 0; i < 24*10; ++i) {
        if (should_do_hard_block == i) do_hard_block();
        if (should_do_roll_block == i) do_roll_block();
        if (should_do_big_block == i) do_big_block();
        if (should_do_antigravity == i) do_antigravity();
        if (should_do_shotgun == i) do_shotgun();
        if (should_do_laser == i) do_laser();
        if (should_do_negate == i) do_negate();
        if (should_do_del_lower == i) do_del_lower();
        if (should_do_del_upper == i) do_del_upper();
        if (should_do_del_even == i) do_del_even();
        if (should_do_push_left == i) do_push_left();
        if (should_do_push_right == i) do_push_right();
        if (should_do_push_down == i) do_push_down();
        if (should_do_180 == i) do_180();
        if (should_do_xray == i) do_xray();
        if (should_do_heavy_block == i) do_heavy_block();
    }

    if (frozen_rows > 0 && ct == 0 && !frozen_ignore_next) {
        for (int i = 24 - frozen_rows; i < 24; i++) {
            if (
                field[0][i].type != BLOCK_VOID &&
                field[1][i].type != BLOCK_VOID &&
                field[2][i].type != BLOCK_VOID &&
                field[3][i].type != BLOCK_VOID &&
                field[4][i].type != BLOCK_VOID &&
                field[5][i].type != BLOCK_VOID &&
                field[6][i].type != BLOCK_VOID &&
                field[7][i].type != BLOCK_VOID &&
                field[8][i].type != BLOCK_VOID &&
                field[9][i].type != BLOCK_VOID) {
                    if (!jingle_played[i]) {
                        jingle_played[i] = true;
                        play_sound(&lineclear_sound);
                        break;
                    }
                }
        }
    }

    frozen_ignore_next = false;
    increment_level_line_clear(ct);
}

void wipe_clears() {
    for (int i = 0; i < 24; ++i) {
        if (clears[i] == -1) continue;
        bool should_not_drop = false;
        for (int x = 0; x < 10; ++x) {
            if (field[x][clears[i]].subtype == BLOCK_HARD) {
                field[x][clears[i]].subtype = BLOCK_VOID;
                should_not_drop = true;
            } else {
                field[x][clears[i]].type = BLOCK_VOID;
                field[x][clears[i]].subtype = BLOCK_VOID;
            }
        }
        if (should_not_drop) clears[i] = -1;
    }
}

void collapse_clears(bool sound) {
    if (selective_gravity || selective_gravity_by_item) {
        if (lines_cleared != 0 && disable_selective_gravity_after_clear) {
            selective_gravity = false;
            selective_gravity_by_item = false;
        }
        lines_cleared = 0;
        return;
    }
    SDL_qsort(clears, 24, sizeof(int), compare_int);

    for (int i = 0; i < 24; ++i) {
        if (clears[i] == -1) continue;
        for (int j = clears[i]; j > 0; j--) {
            field[0][j] = field[0][j-1];
            field[1][j] = field[1][j-1];
            field[2][j] = field[2][j-1];
            field[3][j] = field[3][j-1];
            field[4][j] = field[4][j-1];
            field[5][j] = field[5][j-1];
            field[6][j] = field[6][j-1];
            field[7][j] = field[7][j-1];
            field[8][j] = field[8][j-1];
            field[9][j] = field[9][j-1];
        }
        field[0][0] = (block_t){0};
        field[1][0] = (block_t){0};
        field[2][0] = (block_t){0};
        field[3][0] = (block_t){0};
        field[4][0] = (block_t){0};
        field[5][0] = (block_t){0};
        field[6][0] = (block_t){0};
        field[7][0] = (block_t){0};
        field[8][0] = (block_t){0};
        field[9][0] = (block_t){0};
    }
    play_sound(&linecollapse_sound);
    lines_cleared = 0;
}

bool is_in_cleared_lines_list(int line) {
    for (int i = 0; i < 24; ++i) {
        if (clears[i] == line) return true;
    }
    return false;
}

void do_del_lower() {
    add_overlay("Del Lower");
    int first_occupied = get_first_occupied_row();
    if (first_occupied == -1) return;
    if (lines_cleared == 0) {
        for (int i = 0; i < 24; ++i) clears[i] = -1;
        for (int i = (24 + first_occupied) / 2; i < 24; ++i) clears[i] = i;
        play_sound(&lineclear_sound);
        wipe_clears();
        game_state = STATE_CLEAR;
        game_state_ctr = current_timing->clear;
    } else {
        int current_line = (24 + first_occupied) / 2;
        for (int i = 0; i < 24; ++i) {
            if (clears[i] != -1) continue;
            if (!is_in_cleared_lines_list(current_line)) {
                clears[i] = current_line;
            }
            current_line++;
            if (current_line == 24) break;
        }
    }
}

void do_del_upper() {
    add_overlay("Del Upper");
    int first_occupied = get_first_occupied_row();
    if (first_occupied == -1) return;
    if (lines_cleared == 0) {
        for (int i = 0; i < 24; ++i) clears[i] = -1;
        for (int i = first_occupied; i < (24 + first_occupied) / 2; ++i) clears[i] = i;
        play_sound(&lineclear_sound);
        wipe_clears();
        game_state = STATE_CLEAR;
        game_state_ctr = current_timing->clear;
    } else {
        int current_line = first_occupied;
        for (int i = 0; i < 24; ++i) {
            if (clears[i] != -1) continue;
            if (!is_in_cleared_lines_list(current_line)) {
                clears[i] = current_line;
            }
            current_line++;
            if (current_line >= (24 + first_occupied) / 2) break;
        }
    }
}

void do_del_even() {
    add_overlay("Del Even");
    if (lines_cleared == 0) {
        for (int i = 0; i < 24; ++i) clears[i] = -1;
        for (int i = 0; i < 12; ++i) clears[i] = i*2;
        play_sound(&lineclear_sound);
        wipe_clears();
        game_state = STATE_CLEAR;
        game_state_ctr = current_timing->clear;
    } else {
        int current_line = 0;
        for (int i = 0; i < 24; ++i) {
            if (clears[i] != -1) continue;
            if (!is_in_cleared_lines_list(current_line)) {
                clears[i] = current_line;
            }
            current_line += 2;
            if (current_line >= 24) break;
        }
    }
}

void do_push_left() {
    add_overlay("Push Left");
    for (int y = 0; y < 24; ++y) {
        int left_most_taken = 0;
        for (int x = 0; x < 10; ++x) {
            if (field[x][y].type != BLOCK_VOID) {
                if (x != left_most_taken) {
                    field[left_most_taken][y] = field[x][y];
                    field[x][y].type = BLOCK_VOID;
                    field[x][y].subtype = BLOCK_VOID;
                }
                left_most_taken++;
            }
        }
    }
}

void do_push_right() {
    add_overlay("Push Right");
    for (int y = 0; y < 24; ++y) {
        int right_most_taken = 9;
        for (int x = 9; x >= 0; --x) {
            if (field[x][y].type != BLOCK_VOID) {
                if (x != right_most_taken) {
                    field[right_most_taken][y] = field[x][y];
                    field[x][y].type = BLOCK_VOID;
                    field[x][y].subtype = BLOCK_VOID;
                }
                right_most_taken--;
            }
        }
    }
}

void do_push_down() {
    add_overlay("Push Down");
    for (int x = 0; x < 10; ++x) {
        int bottom_most_taken = 23;
        for (int y = 23; y >= 0; --y) {
            if (field[x][y].type != BLOCK_VOID) {
                if (y != bottom_most_taken) {
                    field[x][bottom_most_taken] = field[x][y];
                    field[x][y].type = BLOCK_VOID;
                    field[x][y].subtype = BLOCK_VOID;
                }
                bottom_most_taken--;
            }
        }
    }
    check_clears();
}

void do_180() {
    add_overlay("Field 180");
    int first_occupied = get_first_occupied_row();
    if (first_occupied == -1) return;

    block_t field_copy[10][24] = {0};

    int new_clears[24];
    SDL_memcpy(new_clears, clears, sizeof clears);

    int dest_y = 23;
    for (int y = first_occupied; y < 24; ++y) {
        int dest_x = 9;
        for (int x = 0; x < 10; ++x) {
            field_copy[dest_x][dest_y] = field[x][y];
            dest_x--;
        }
        for (int i = 0; i < 24; ++i) {
            if (clears[i] == y) new_clears[i] = dest_y;
        }
        dest_y--;
    }


    SDL_memcpy(clears, new_clears, sizeof clears);
    SDL_memcpy(field, field_copy, sizeof field);
}

void do_big_block() {
    add_overlay("Big Block!");
    next_piece[0].is_big = true;
}

void do_antigravity() {
    add_overlay("Antigravity!");
    selective_gravity_by_item = true;
    disable_selective_gravity_after_clear = true;
}

void do_hard_block() {
    add_overlay("Hard Block!");
    next_piece[0].subtype = BLOCK_HARD;
}

void do_heavy_block() {
    add_overlay("Heavy Block!");
    next_piece[0].is_heavy = true;
}

void do_xray() {
    add_overlay("X-RAY");
    xray_ctr = 7;
}

void do_roll_block() {
    add_overlay("Rolling Block!");
    next_piece[0].is_rolling = true;
    next_piece[1].is_rolling = true;
    next_piece[2].is_rolling = true;
}

int get_gravity() {
    if (get_setting_value(GRAVITY_SETTING_IDX) == 1) return 5120;
    if (current_piece.is_heavy) {
        if (current_timing->g < 128) return current_timing->g + 128;
        return current_timing->g * 2;
    }
    return current_timing->g;
}

block_type_t generate_piece() {
    uint8_t piece = xoroshiro_next() % 8;
    while(piece > 6) piece = xoroshiro_next() % 8;
    return (block_type_t)(piece + 2);
}

live_block_t generate_valid_piece() {
    block_type_t result = BLOCK_VOID;
    if (get_setting_value(RNG_SETTING_IDX) == 2) {
        block_type_t droughted_piece = BLOCK_VOID;
        int bagpos = 0;
        int longest_drought = 0;

        for(int i = 0; i < 6; i++) {
            // Pick a piece from the bag.
            bagpos = (int)(uint8_t)xoroshiro_next() % 64;
            while (bagpos > 34) bagpos = (int)(uint8_t)xoroshiro_next() % 64;
            result = bag[bagpos];

            // Not in history, gg!
            if(result != history[0] && result != history[1] && result != history[2] && result != history[3]) {
                break;
            }

            // Calculate longest drought.
            longest_drought = 0;
            for(int j = 0; j < 7; j++) {
                if(longest_drought < histogram[j]) {
                    longest_drought = histogram[j];
                    droughted_piece = j+2;
                }
            }

            // Churn the bag.
            bag[bagpos] = droughted_piece;

            // Get a piece in case of last loop.
            bagpos = (int)(uint8_t)xoroshiro_next() % 64;
            while (bagpos > 34) bagpos = (int)(uint8_t)xoroshiro_next() % 64;
            result = bag[bagpos];
        }

        for (int i = 0; i < 7; i++) histogram[i]++;
        histogram[result-2] = 0;

        longest_drought = 0;
        for(int i = 0; i < 7; i++) {
            if(longest_drought < histogram[i]) {
                longest_drought = histogram[i];
                droughted_piece = i+2;
            }
        }

        bag[bagpos] = droughted_piece;
        int hist[7] = {0};
        for (int i = 0; i < 35; i++) {
            hist[bag[i]-2]++;
        }
    } else {
        result = generate_piece();
        int tries = 1;
        while(tries++ < (get_setting_value(RNG_SETTING_IDX) == 0 ? 4 : 6)) {
            if(result != history[0] && result != history[1] && result != history[2] && result != history[3]) {
                break;
            } else {
                result = generate_piece();
            }
        }
    }

    history[3] = history[2];
    history[2] = history[1];
    history[1] = history[0];
    history[0] = result;

    live_block_t ret = {0};
    ret.type = result;
    ret.is_bone = bones;
    ret.is_big = big_mode || next_big_piece;
    ret.is_rolling = false;
    ret.is_heavy = false;
    ret.subtype = next_subtype;
    next_subtype = BLOCK_VOID;
    next_big_piece = false;
    return ret;
}

void do_hold(bool ihs) {
    if (!can_hold) return;
    can_hold = false;

    if (held_piece.type == BLOCK_VOID) {
        live_block_t result = generate_valid_piece();
        held_piece.type = current_piece.type;
        held_piece.subtype = current_piece.subtype;
        held_piece.is_bone = current_piece.is_bone;
        held_piece.is_big = current_piece.is_big;
        held_piece.is_heavy = current_piece.is_heavy;
        held_piece.is_rolling = current_piece.is_rolling;
        current_piece = (live_block_t){0};
        current_piece = next_piece[0];
        next_piece[0] = next_piece[1];
        next_piece[1] = next_piece[2];
        next_piece[2] = result;

        if (!ihs) {
            if (next_piece[0].type == BLOCK_I) play_sound(&i_sound);
            if (next_piece[0].type == BLOCK_S) play_sound(&s_sound);
            if (next_piece[0].type == BLOCK_Z) play_sound(&z_sound);
            if (next_piece[0].type == BLOCK_J) play_sound(&j_sound);
            if (next_piece[0].type == BLOCK_L) play_sound(&l_sound);
            if (next_piece[0].type == BLOCK_O) play_sound(&o_sound);
            if (next_piece[0].type == BLOCK_T) play_sound(&t_sound);
        }
    } else {
        live_block_t temp = current_piece;
        current_piece = held_piece;
        held_piece = temp;
    }

    current_piece.x = 3;
    if (big_mode || current_piece.is_big) current_piece.x = 2;
    current_piece.y = 2;
    current_piece.lock_param = 1.0f;
    current_piece.lock_status = LOCK_UNLOCKED;
    current_piece.rotation_state = 0;
    current_piece.lock_delay = current_timing->lock;
    current_piece.floor_kicked = false;

    play_sound(&hold_sound);

    if (ihs) return;

    // Apply IRS.
    if ((IS_HELD(button_a_held) || IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 2 : 1;
    else if ((IS_RELEASED(button_a_held)&& IS_RELEASED(button_c_held)) && IS_HELD(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 0 : 3;
    else if ((IS_HELD(button_a_held) && IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 3 : 2;
    else if (current_piece.is_rolling) current_piece.rotation_state = 1;
    if (current_piece.rotation_state != 0) {
        if (piece_collides(current_piece) != -1) current_piece.rotation_state = 0;
        else if (!current_piece.is_rolling || current_piece.rotation_state != 1) play_sound(&irs_sound);
    }

    if (get_gravity() == 5120) {
        while (!piece_grounded()) try_descend();
    }

    if (piece_grounded() && piece_collides(current_piece) == -1) {
        play_sound(&pieceland_sound);
    }

    accumulated_g = 0;
}

void generate_next_piece() {
    can_hold = true;
    current_piece = next_piece[0];
    next_piece[0] = next_piece[1];
    next_piece[1] = next_piece[2];
    next_piece[2] = generate_valid_piece();;
    current_piece.x = 3;
    if (big_mode || current_piece.is_big) current_piece.x = 2;
    current_piece.y = 2;
    current_piece.lock_param = 1.0f;
    current_piece.lock_status = LOCK_UNLOCKED;
    current_piece.rotation_state = 0;
    current_piece.lock_delay = current_timing->lock;
    current_piece.floor_kicked = false;

    // Apply IHS.
    if (IS_HELD(button_hold_held) && get_setting_value(HOLD_SETTING_IDX) == 1) do_hold(true);

    // Apply IRS.
    if ((IS_HELD(button_a_held) || IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 2 : 1;
    else if ((IS_RELEASED(button_a_held)&& IS_RELEASED(button_c_held)) && IS_HELD(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 0 : 3;
    else if ((IS_HELD(button_a_held) && IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = current_piece.is_rolling ? 3 : 2;
    else if (current_piece.is_rolling) current_piece.rotation_state = 1;
    if (current_piece.rotation_state != 0) {
        if (piece_collides(current_piece) != -1) current_piece.rotation_state = 0;
        else if (!current_piece.is_rolling || current_piece.rotation_state != 1) play_sound(&irs_sound);
    }

    // Play sound.
    if (next_piece[0].type == BLOCK_I) play_sound(&i_sound);
    if (next_piece[0].type == BLOCK_S) play_sound(&s_sound);
    if (next_piece[0].type == BLOCK_Z) play_sound(&z_sound);
    if (next_piece[0].type == BLOCK_J) play_sound(&j_sound);
    if (next_piece[0].type == BLOCK_L) play_sound(&l_sound);
    if (next_piece[0].type == BLOCK_O) play_sound(&o_sound);
    if (next_piece[0].type == BLOCK_T) play_sound(&t_sound);

    if (get_gravity() == 5120) {
        while (!piece_grounded()) try_descend();
    }

    if (piece_grounded() && piece_collides(current_piece) == -1) {
        play_sound(&pieceland_sound);
    }

    accumulated_g = 0;
}

void generate_first_piece() {
    history[1] = BLOCK_Z;
    history[2] = get_setting_value(RNG_SETTING_IDX) == 0 ? BLOCK_Z : BLOCK_S;
    history[3] = get_setting_value(RNG_SETTING_IDX) == 0 ? BLOCK_Z : BLOCK_S;
    for (int i = 0; i < 35; i++) bag[i] = (i/5)+2;

    histogram[0] = 4;
    histogram[1] = 4;
    histogram[2] = 4;
    histogram[3] = 4;
    histogram[4] = 4;
    histogram[5] = 4;
    histogram[6] = 4;
    block_type_t result = generate_piece();
    while(result == BLOCK_S || result == BLOCK_Z || result == BLOCK_O) result = generate_piece();
    history[0] = result;

    current_piece = (live_block_t){0};
    current_piece.type = BLOCK_VOID;

    held_piece = (live_block_t){0};
    can_hold = true;

    next_piece[0] = (live_block_t){0};
    next_piece[1] = (live_block_t){0};
    next_piece[2] = (live_block_t){0};

    next_piece[0].type = result;
    next_piece[0].subtype = next_subtype;
    next_piece[0].is_bone = bones;
    next_piece[0].is_big = big_mode || next_big_piece;
    next_piece[0].is_rolling = false;

    next_big_piece = false;
    next_subtype = BLOCK_VOID;

    next_piece[1] = generate_valid_piece();
    next_piece[2] = generate_valid_piece();
}

void gray_line(const int row) {
    for (int i = 0; i < 10; i++) {
        if (field[i][row].type != BLOCK_VOID) {
            field[i][row].type = BLOCK_X;
            field[i][row].subtype = BLOCK_VOID;
        }
    }
}

void render_raw_block(const int col, const int row, const block_type_t block, const lock_status_t lockStatus, const float lockParam, bool voidToLeft, bool voidToRight, bool voidAbove, bool voidBelow, float fade_state, bool big) {
    if (block == BLOCK_VOID) return;

    SDL_FRect src = texture_atlas[block];
    SDL_FRect dest = {
        .x = FIELD_X_OFFSET + ((float)col * 16.0f),
        .y = FIELD_Y_OFFSET + ((float)row * 16.0f) - (big ? 16.0f : 0.0f),
        .w = big ? 32.0f : 16.0f,
        .h = big ? 32.0f : 16.0f
    };

    if (col == 0) voidToLeft = false;
    if (col == 9) voidToRight = false;
    if (row == 19) voidBelow = false;

    if (xray_ctr > 0) {
        if (lockStatus == LOCK_GHOST) return;
        if (lockStatus == LOCK_LOCKED || lockStatus == LOCK_FLASH) {
            if (game_details.total_frames % 30 != col) return;
        }
    }

    float mod = 1.0f;
    float moda = 1.0f;
    switch(lockStatus) {
        case LOCK_LOCKED:
            mod = block == BLOCK_BONE ? 1.0f : 0.6f;
            moda = 0.8f;
            if (game_state == STATE_FADEOUT || game_state == STATE_GAMEOVER) {
                if (game_state_ctr < 0) moda = lerp(0.8f, 0.0f, ((float)-game_state_ctr/100.0f));
            } else {
                moda = 0.8f * fade_state;
            }
            break;
        case LOCK_FLASH:
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 192);
            SDL_RenderFillRect(renderer, &dest);
            return;
        case LOCK_UNLOCKED:
            mod = 0.5f + (lockParam * 0.5f);
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            if (SMOOTH_GRAVITY && !piece_grounded()) dest.y += ((float)accumulated_g/256.0f) * 16.0f;
            break;
        case LOCK_GHOST:
            mod = 0.8f;
            moda = 0.25f;
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            if (TRANSPARENCY) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 32);
                SDL_RenderFillRect(renderer, &dest);
            }
            break;
        case LOCK_NEXT:
            moda = 0.8f;
            if (lockParam != 0) {
                moda = 0.6f;
                dest.x = FIELD_X_OFFSET + ((float)col * 8.0f);
                dest.x += 16.0f*6.0f;
                if (lockParam == 2) dest.x += 8.0f*4.5f;
                dest.y = FIELD_Y_OFFSET + ((float)row * 8.0f);
                dest.y -= 24.0f;
                dest.w = 8.0f;
                dest.h = 8.0f;
            }
            break;
        case LOCK_HOLD:
            moda = 0.8f;
            dest.x = FIELD_X_OFFSET + ((float)col * 8.0f);
            dest.x -= 32.0f;
            dest.y = FIELD_Y_OFFSET + ((float)row * 8.0f);
            dest.y -= 24.0f;
            dest.w = 8.0f;
            dest.h = 8.0f;
            if (!can_hold) {
                mod = 0.3f;
                moda = 0.3f;
            }
            break;
        default:
            return;
    }

    SDL_SetTextureColorModFloat(atlas_texture, mod, mod, mod);
    SDL_SetTextureAlphaModFloat(atlas_texture, moda);
    if (moda > 0.0f) SDL_RenderTexture(renderer, atlas_texture, &src, &dest);
    SDL_SetTextureColorModFloat(atlas_texture, 1, 1, 1);
    SDL_SetTextureAlphaModFloat(atlas_texture, 1);

    bool should_draw_edges = false;
    if (current_timing->fade == 0 && get_setting_value(INVISIBILITY_SETTING_IDX) == 0) should_draw_edges = true;
    if (!should_draw_edges && game_state == STATE_GAMEOVER) should_draw_edges = true;
    if (!should_draw_edges && game_state == STATE_FADEOUT) should_draw_edges = true;
    if (!should_draw_edges && show_edge_during_lockflash && game_state == STATE_LOCKFLASH) should_draw_edges = true;
    if (!should_draw_edges && show_edge_timer > 0) should_draw_edges = true;
    if (!should_draw_edges) return;

    if (show_edge_timer > 0) show_edge_timer--;
    SDL_SetRenderDrawColor(renderer, 161, 161, 151, 255);
    if(voidToLeft) {
        SDL_RenderLine(renderer, dest.x, dest.y, dest.x, dest.y + dest.h - 1);
        SDL_RenderLine(renderer, dest.x+1, dest.y, dest.x+1, dest.y + dest.h - 1);
    }
    if(voidToRight) {
        SDL_RenderLine(renderer, dest.x + dest.w - 1, dest.y, dest.x + dest.w - 1, dest.y + dest.h - 1);
        SDL_RenderLine(renderer, dest.x + dest.w - 2, dest.y, dest.x + dest.w - 2, dest.y + dest.h - 1);
    }
    if(voidAbove) {
        SDL_RenderLine(renderer, dest.x, dest.y, dest.x + dest.w - 1, dest.y);
        SDL_RenderLine(renderer, dest.x, dest.y+1, dest.x + dest.w - 1, dest.y+1);
    }
    if(voidBelow) {
        SDL_RenderLine(renderer, dest.x, dest.y + dest.h - 1, dest.x + dest.w - 1, dest.y + dest.h - 1);
        SDL_RenderLine(renderer, dest.x, dest.y + dest.h - 2, dest.x + dest.w - 1, dest.y + dest.h - 2);
    }
}

float update_and_get_fade_state(block_t *block) {
    if (get_setting_value(INVISIBILITY_SETTING_IDX) == 0 && current_timing->fade == 0) {
        block->fade_state = 1.0f;
        block->fading = false;
    } else if (get_setting_value(INVISIBILITY_SETTING_IDX) == 2 || current_timing->fade == 1) {
        block->fade_state = 0.0f;
        block->fading = true;
    } else if (get_setting_value(INVISIBILITY_SETTING_IDX) == 1 || current_timing->fade > 1) {
        int delay = 256;
        if (current_timing->fade > 1 && current_timing->fade < 256) delay = current_timing->fade;
        if (game_details.total_frames - block->locked_at > delay) block->fading = true;
    }

    if (block->fading) block->fade_state -= 1.0f/FADE_TIME;
    if (block->fade_state < 0) block->fade_state = 0.0f;

    return block->fade_state;
}

void render_field_block(const int x, const int y) {
    if(x < 0 || x > 9 || y < 1 || y > 23) return;
    const bool voidToLeft = x != 0 && field[x-1][y].type == BLOCK_VOID;
    const bool voidToRight = x != 9 && field[x+1][y].type == BLOCK_VOID;
    const bool voidAbove = y != 4 && field[x][y-1].type == BLOCK_VOID;
    const bool voidBelow = y != 23 && field[x][y+1].type == BLOCK_VOID;
    block_type_t to_draw = field[x][y].type;
    if (field[x][y].subtype != BLOCK_VOID) to_draw = field[x][y].subtype;
    render_raw_block(x, y-4, to_draw, field[x][y].lock_status, field[x][y].lock_param, voidToLeft, voidToRight, voidAbove, voidBelow, update_and_get_fade_state(&field[x][y]), false);
}

void render_held_block() {
    block_type_t to_draw = held_piece.type;
    if (held_piece.subtype != BLOCK_VOID) to_draw = held_piece.subtype;
    else if (held_piece.is_bone) to_draw = BLOCK_BONE;
    switch(held_piece.type) {
        case BLOCK_I:
            render_raw_block(3, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(6, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_L:
            render_raw_block(3, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(3, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_O:
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_Z:
            render_raw_block(3, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_T:
            render_raw_block(3, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_J:
            render_raw_block(3, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        case BLOCK_S:
            render_raw_block(4, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(3, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_HOLD, 1.0f, false, false, false, false, 1, false);
            break;
        default:
            return;
    }
}

void render_next_block(int index) {
    block_type_t to_draw = next_piece[index].type;
    if (next_piece[index].subtype != BLOCK_VOID) to_draw = next_piece[index].subtype;
    else if (next_piece[index].is_bone) to_draw = BLOCK_BONE;
    switch(next_piece[index].type) {
        case BLOCK_I:
            render_raw_block(3, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(6, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_L:
            render_raw_block(3, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(3, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_O:
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_Z:
            render_raw_block(3, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_T:
            render_raw_block(3, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_J:
            render_raw_block(3, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        case BLOCK_S:
            render_raw_block(4, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(5, -4, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(3, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            render_raw_block(4, -3, to_draw, LOCK_NEXT, (float)index, false, false, false, false, 1, false);
            break;
        default:
            return;
    }
}

void render_current_block() {
    if(current_piece.type == BLOCK_VOID) return;

    const rotation_state_t rotation = get_rotation_state(current_piece.type, current_piece.rotation_state);

    block_type_t to_draw = current_piece.type;
    if (current_piece.subtype != BLOCK_VOID) to_draw = current_piece.subtype;
    else if (current_piece.is_bone) to_draw = BLOCK_BONE;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotation[4*j + i] != ' ') {
                render_raw_block(current_piece.x + i + (current_piece.is_big ? i : 0), current_piece.y + j + (current_piece.is_big ? j : 0) - 3, to_draw, current_piece.lock_status, current_piece.lock_param, false, false, false, false, 1, current_piece.is_big);
            }
        }
    }
}

void render_tls() {
    if(current_piece.type == BLOCK_VOID) return;

    live_block_t active = current_piece;
    active.y++;
    while (piece_collides(active) == -1) active.y++;
    active.y--;
    if (active.y == current_piece.y) return;

    rotation_state_t rotation = get_rotation_state(current_piece.type, current_piece.rotation_state);
    block_type_t to_draw = current_piece.type;
    if (current_piece.subtype != BLOCK_VOID) to_draw = current_piece.subtype;
    else if (current_piece.is_bone) to_draw = BLOCK_BONE;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotation[4*j + i] != ' ') {
                render_raw_block(active.x + i + (current_piece.is_big ? i : 0), active.y + j + (current_piece.is_big ? j : 0) - 3, to_draw, LOCK_GHOST, active.lock_param, false, false, false, false, 1, current_piece.is_big);
            }
        }
    }
}

void render_field() {
    for (int x = 0; x < 10; x++) {
        for (int y = 4; y < 24; y++) {
            render_field_block(x, y);
        }
    }
}

void toggle_details() {
    DETAILS = !DETAILS;
    apply_scale();
}

void render_details() {
    if (!DETAILS) return;
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, TRANSPARENCY ? FIELD_TRANSPARENCY: 1.0f);
    SDL_FRect dst = {.x = FIELD_X_OFFSET+(12*16), .y = 0.0f, .w = 16.0f * 12.0f, .h = 16.0f * 26.0f};
    SDL_RenderFillRect(renderer, &dst);

    SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1);

    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 4,  "LEVEL  GRAVITY  LOCKDEL");
    if (get_gravity() < 256) SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 14, " %04d   %05.3fG      %02df", level, (float)get_gravity()/256.0f, current_timing->lock);
    else SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 14, " %04d      %2dG      %02df", level, get_gravity()/256, current_timing->lock);
    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 30, "DAS  ARE  LN-ARE  CLEAR");
    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 40, "%2df  %2df     %2df    %2df", current_timing->das, current_timing->are, current_timing->line_are, current_timing->clear);
    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 56, "Mode: [%dk%dr%dp%ch%sg%4s]",
        locked_settings[ROTATION_SETTING_IDX] == 0 ? 1 : 3,
        locked_settings[RNG_SETTING_IDX] + 1,
        locked_settings[PREVIEWS_SETTING_IDX],
        locked_settings[HOLD_SETTING_IDX] == 0 ? 'N' : 'Y',
        locked_settings[GRAVITY_SETTING_IDX] == 0 ? "--" : "20",
        locked_settings[INVISIBILITY_SETTING_IDX] == 0 ? "----" : locked_settings[INVISIBILITY_SETTING_IDX] == 1 ? "fade" : "invi"
    );
    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), 66, "Hash: %s", MODE_HASH);

    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), (FIELD_Y_OFFSET+4), "SECTION            TIME");

    // Room for 13 sections.
    int i = 0;
    if (game_details.current_section > 13) i = game_details.current_section - 13;
    for (int j = 0; i < game_details.current_section+1; ++i, j += 2) {
        int section_start = i * 100;
        int section_end = section_start + 99;
        int lap_cs = game_details.splits[i].lap_frames;
        int lap_seconds = lap_cs / 60;
        int lap_minutes = lap_seconds / 60;
        lap_cs = (int)(((float)lap_cs/60.0f)*100);
        lap_cs %= 100;
        lap_seconds %= 60;
        int split_cs = game_details.splits[i].split_frames;
        int split_seconds = split_cs / 60;
        int split_minutes = split_seconds / 60;
        split_cs = (int)(((float)split_cs/60.0f)*100);
        split_cs %= 100;
        split_seconds %= 60;

        if (i == game_details.highest_regular_section + 1) {
            SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), (FIELD_Y_OFFSET+22) + (float)(j*10),  "FINALROLL (L)  %02d:%02d.%02d", lap_minutes, lap_seconds, lap_cs);
        } else {
            SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), (FIELD_Y_OFFSET+22) + (float)(j*10),  "%04d-%04d (L)  %02d:%02d.%02d", section_start, section_end, lap_minutes, lap_seconds, lap_cs);
        }
        SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+4), (FIELD_Y_OFFSET+32) + (float)(j*10),  "          (S) %03d:%02d.%02d", split_minutes, split_seconds, split_cs);
    }

    int total_cs = game_details.total_frames;
    if (in_roll && game_state != STATE_FADEOUT) total_cs = in_roll_remaining;
    int total_seconds = total_cs / 60;
    int total_minutes = total_seconds / 60;
    total_cs = (int)(((float)total_cs/60.0f)*100);
    total_cs %= 100;
    total_seconds %= 60;

    SDL_SetRenderScale(renderer, (float)RENDER_SCALE*2.0f, (float)RENDER_SCALE*2.0f);
    if ((current_timing->g >= 5120 || get_setting_value(GRAVITY_SETTING_IDX) == 1) && (game_details.total_frames & 8) != 0) SDL_SetRenderDrawColor(renderer, 239, 191, 4, 255);
    else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(renderer, (FIELD_X_OFFSET+(12*16)+24)/2, (FIELD_Y_OFFSET+310)/2, "%03d:%02d.%02d", total_minutes, total_seconds, total_cs);
    SDL_SetRenderScale(renderer, (float)RENDER_SCALE, (float)RENDER_SCALE);
}

void get_music_data(section_music_t **section) {
    // Title screen.
    if (game_state == STATE_WAIT) {
        if (TITLE_MUSIC.level_start == -1) {
            *section = &TITLE_MUSIC;
        } else {
            *section = NULL;
        }
        return;
    }

    // Intro and game over.
    if (game_state == STATE_BEGIN || game_state == STATE_GAMEOVER) {
        *section = NULL;
        return;
    }

    if (in_roll && CREDITS_ROLL_MUSIC.level_start == -1) {
        if (game_state == STATE_FADEOUT) *section = NULL;
        else *section = &CREDITS_ROLL_MUSIC;
        return;
    }

    // Otherwise we're in game, get the most appropriate section.
    section_music_t *current = NULL;
    section_music_t *next = NULL;
    for (int i = 0; i < SECTION_COUNT; ++i) {
        if (SECTIONS[i].level_start <= level) {
            current = SECTIONS + i;
            if (i < SECTION_COUNT - 1) next = current + 1;
            else next = NULL;
        } else {
            break;
        }
    }

    *section = current;
    if (CREDITS_ROLL_TIMING.level - 20 <= level) *section = NULL;
    if (next == NULL) return;
    if (next->level_start - 20 <= level) *section = NULL;
}

void play_music() {
    if (music == NULL) return;
    if (MUTED || BGM_VOLUME == 0.0f) {
        SDL_ClearAudioStream(music);
        return;
    }

    // Pause on pause.
    if (game_state == STATE_PAUSED) {
        SDL_PauseAudioStreamDevice(music);
        return;
    }
    if (SDL_AudioStreamDevicePaused(music)) SDL_ResumeAudioStreamDevice(music);

    // Get the appropriate section.
    section_music_t *section = NULL;
    get_music_data(&section);

    // If the section isn't the same as the previous section, we need to fade!
    if (section != last_section) {
        if (last_section != NULL) {
            volume -= 0.025f;
            if (volume < 0.0f) {
                volume = 1.0f;
                SDL_ClearAudioStream(music);
                last_section = section;
                intro = true;
            }
            section = last_section;
        } else {
            volume = 1.0f;
            SDL_ClearAudioStream(music);
            last_section = section;
            intro = true;
        }
    }
    SDL_SetAudioStreamGain(music, volume * BGM_VOLUME);

    // If there's no audio to play, return;
    if (section == NULL) {
        SDL_ClearAudioStream(music);
        return;
    }

    // Queue up.
    if (intro) {
        SDL_SetAudioStreamFormat(music, &section->head.format, NULL);
        SDL_PutAudioStreamData(music, section->head.wave_data, (int)section->head.wave_data_len);
        intro = false;
    } else if (SDL_GetAudioStreamAvailable(music) < section->body.wave_data_len) {
        SDL_SetAudioStreamFormat(music, &section->body.format, NULL);
        SDL_PutAudioStreamData(music, section->body.wave_data, (int)section->body.wave_data_len);
    }
}

void render_game() {
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // Background.
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, TRANSPARENCY ? FIELD_TRANSPARENCY : 1.0f);
    SDL_FRect dst = {.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET, .w = 16.0f * 10.0f, .h = 16.0f * 20.0f};
    SDL_RenderFillRect(renderer, &dst);

    // Field border.
    if (game_state != STATE_PAUSED && game_state != STATE_BEGIN  && game_state != STATE_WAIT) SDL_SetTextureColorModFloat(atlas_texture, 0.9f, 0.1f, 0.1f);
    else SDL_SetTextureColorModFloat(atlas_texture, border_r, border_g, border_b);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET - 16, .y = FIELD_Y_OFFSET - 16, .w = 96.0f*2, .h = 176.0f*2};
    SDL_FRect src = texture_atlas[TEXTURE_FIELD];
    SDL_RenderTexture(renderer, atlas_texture, &src, &dst);
    SDL_SetTextureColorModFloat(atlas_texture, 1, 1, 1);
    SDL_SetTextureAlphaModFloat(atlas_texture, 1);

    // Background for next.
    if (get_setting_value(PREVIEWS_SETTING_IDX) > 0 || get_setting_value(HOLD_SETTING_IDX) > 0) {
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE/2.0f, (float)RENDER_SCALE/2.0f);
        const float mid_point = FIELD_X_OFFSET + (5.5f * 32.0f);
        dst.x = mid_point - 3.0f*32.0f;
        dst.y = 2.0f * (FIELD_Y_OFFSET - (4.0f * 16.0f) - 4);
        dst.w = 6.0f * 32.0f;
        dst.h = 2.0f * (2.0f * 16.0f + 8);
        SDL_FRect dst2 = dst;
        dst2.h = 1.0f;
        SDL_FRect dst3 = dst2;
        dst3.y += dst.h - 1;
        for (int i = 0; i < 80; i++) {
            SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.015f);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.05f);
            SDL_RenderFillRect(renderer, &dst2);
            SDL_RenderFillRect(renderer, &dst3);
            dst.x -= 1.0f;
            dst.w += 2.0f;
            dst2.x -= 1.0f;
            dst2.w += 2.0f;
            dst3.x -= 1.0f;
            dst3.w += 2.0f;
        }
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE, (float)RENDER_SCALE);
    }

    // Next/hold text.
    src = texture_atlas[TEXTURE_NEXT];
    dst.x = FIELD_X_OFFSET + (3.0f * 16.0f);
    dst.y = (FIELD_Y_OFFSET - (4.0f * 16.0f) - 12);
    dst.w = src.w;
    dst.h = src.h;
    SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);
    if (get_setting_value(PREVIEWS_SETTING_IDX) > 0) SDL_RenderTexture(renderer, atlas_texture, &src, &dst);
    src = texture_atlas[TEXTURE_HOLD];
    dst.x -= 3.5f*16.0f;
    if (get_setting_value(HOLD_SETTING_IDX) > 0) SDL_RenderTexture(renderer, atlas_texture, &src, &dst);

    // Render the game.
    render_field();
    render_held_block();
    if (get_setting_value(PREVIEWS_SETTING_IDX) > 0) render_next_block(0);
    if (get_setting_value(PREVIEWS_SETTING_IDX) > 1) render_next_block(1);
    if (get_setting_value(PREVIEWS_SETTING_IDX) > 2) render_next_block(2);
    render_tls();
    render_current_block();
    render_details();

    // Pikii?
    if (frozen_rows > 0) {
        SDL_SetRenderDrawColorFloat(renderer, 0.45f, 0.61f, 0.82f, 0.4f);
        dst.w = 10.0f*16.0f;
        dst.h = (float)frozen_rows*16.0f;
        dst.x = FIELD_X_OFFSET;
        dst.y = FIELD_Y_OFFSET + ((20.0f-(float)((frozen_rows > 20) ? 20 : frozen_rows))*16.0f);
        SDL_RenderFillRect(renderer, &dst);
        SDL_SetRenderDrawColorFloat(renderer, 0.45f, 0.61f, 0.82f, 0.8f);
        dst.h = 2.0f;
        SDL_RenderFillRect(renderer, &dst);
    }

    // Overlay?
    if (effect_overlay_ctr > 0) {
        effect_overlay_ctr--;

        SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);
        for (int i = 0; i < 10; ++i) {
            size_t length = (SDL_strlen(effect_overlay[i]) / 2) * 8;
            if (length == 0) continue;
            SDL_RenderDebugText(renderer, FIELD_X_OFFSET + (5*16) - length, FIELD_Y_OFFSET+80.0f+(i*10.0f), effect_overlay[i]);
        }
    }

    // Garbage / G?
    dst.x = FIELD_X_OFFSET + 10*16 + 12;
    dst.y = FIELD_Y_OFFSET;
    dst.w = 7;
    dst.h = 20*16;
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, TRANSPARENCY ? FIELD_TRANSPARENCY : 1.0f);
    SDL_RenderFillRect(renderer, &dst);

    dst.x += 1;
    dst.y += 1;
    dst.w = 2;
    dst.h -= 2;

    float total = dst.h;
    float fraction = 0;

    if (current_timing->garbage != 0) fraction = ((float)current_timing->garbage - (float)garbage_ctr - 1) / ((float)current_timing->garbage - 1);
    else fraction = (float)get_gravity() / 5120.0f;
    if (fraction > 1) fraction = 1;
    if (fraction < 0) fraction = 0;

    dst.y += (1-fraction) * total;
    dst.h = fraction * total;
    if (((current_timing->garbage == 0 && get_gravity() >= 5120) || (current_timing->garbage != 0 && garbage_ctr == 0)) && (game_details.total_frames & 8) != 0) SDL_SetRenderDrawColor(renderer, 239, 191, 4, TRANSPARENCY ? (FIELD_TRANSPARENCY * 255) : 255);
    else SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, TRANSPARENCY ? FIELD_TRANSPARENCY : 1.0f);
    SDL_RenderFillRect(renderer, &dst);

    dst.y = FIELD_Y_OFFSET + 1;
    dst.x += 3;
    fraction = (float)(level % 100) / 100.0f;
    dst.y += (1-fraction) * total;
    dst.h = fraction * total;
    if (section_locked && (game_details.total_frames & 8) != 0) SDL_SetRenderDrawColor(renderer, 239, 191, 4, TRANSPARENCY ? (FIELD_TRANSPARENCY * 255) : 255);
    else SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, TRANSPARENCY ? FIELD_TRANSPARENCY : 1.0f);
    SDL_RenderFillRect(renderer, &dst);

    // Title screen.
    if (game_state == STATE_WAIT) {
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE/2.0f, (float)RENDER_SCALE/2.0f);
        SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);

        float y_offset = FIELD_Y_OFFSET+4.0f;
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE, (float)RENDER_SCALE);
        for (int i = 0; i < SETTINGS_COUNT; ++i) {
            if (selected_setting == i) {
                SDL_SetRenderDrawColorFloat(renderer, 1, 1, 0, 1.0f);
                SDL_RenderDebugTextFormat(renderer, 21.0f, y_offset, "< %-15s >", get_settings_description(i));
            } else {
                SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);
                SDL_RenderDebugTextFormat(renderer, 21.0f, y_offset, "  %-15s  ", get_settings_description(i));
            }
            y_offset += 9.0f;
        }
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE/2.0f, (float)RENDER_SCALE/2.0f);
        SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);

        y_offset = 12.0f;
        SDL_RenderDebugTextFormat(renderer, 80.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "Press %s/%s to begin", SDL_GetScancodeName(BUTTON_START), SDL_GetGamepadStringForButton(GAMEPAD_START));

        y_offset += 1.0f;
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "Keyboard (Gamepad):");

        for (int i = 0; i < controls_list_len; ++i) {
            int32_t *button = controls_list[i].button;
            int32_t *gamepad = controls_list[i].gamepad;
            char *desc = controls_list[i].description;

            if (*button != SDL_SCANCODE_UNKNOWN && *gamepad != SDL_GAMEPAD_BUTTON_INVALID) {
                y_offset += 0.5f;
                if (button == &BUTTON_LEFT) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s (%s/%s): %s", SDL_GetScancodeName(BUTTON_LEFT), SDL_GetScancodeName(BUTTON_RIGHT), SDL_GetGamepadStringForButton(GAMEPAD_LEFT), SDL_GetGamepadStringForButton(GAMEPAD_RIGHT), desc);
                } else if (button == &BUTTON_CCW_1) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s (%s/%s): %s", SDL_GetScancodeName(BUTTON_CCW_1), SDL_GetScancodeName(BUTTON_CCW_2), SDL_GetGamepadStringForButton(GAMEPAD_CCW_1), SDL_GetGamepadStringForButton(GAMEPAD_CCW_2), desc);
                } else if (button == &BUTTON_SCALE_DOWN) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s (%s/%s): %s", SDL_GetScancodeName(BUTTON_SCALE_DOWN), SDL_GetScancodeName(BUTTON_SCALE_UP), SDL_GetGamepadStringForButton(GAMEPAD_SCALE_DOWN), SDL_GetGamepadStringForButton(GAMEPAD_SCALE_UP), desc);
                } else {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s (%s): %s", SDL_GetScancodeName(*button), SDL_GetGamepadStringForButton(*gamepad), desc);
                }
            } else if (*button != SDL_SCANCODE_UNKNOWN) {
                y_offset += 0.5f;
                if (button == &BUTTON_LEFT) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s: %s", SDL_GetScancodeName(BUTTON_LEFT), SDL_GetScancodeName(BUTTON_RIGHT), desc);
                } else if (button == &BUTTON_CCW_1) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s: %s", SDL_GetScancodeName(BUTTON_CCW_1), SDL_GetScancodeName(BUTTON_CCW_2), desc);
                } else if (button == &BUTTON_SCALE_DOWN) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s/%s: %s", SDL_GetScancodeName(BUTTON_SCALE_DOWN), SDL_GetScancodeName(BUTTON_SCALE_UP), desc);
                } else {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- %s: %s", SDL_GetScancodeName(*button), desc);
                }
            } else if (*gamepad != SDL_GAMEPAD_BUTTON_INVALID) {
                y_offset += 0.5f;
                if (button == &BUTTON_LEFT) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- (%s/%s): %s", SDL_GetGamepadStringForButton(GAMEPAD_LEFT), SDL_GetGamepadStringForButton(GAMEPAD_RIGHT), desc);
                } else if (button == &BUTTON_CCW_1) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- (%s/%s): %s", SDL_GetGamepadStringForButton(GAMEPAD_CCW_1), SDL_GetGamepadStringForButton(GAMEPAD_CCW_2), desc);
                } else if (button == &BUTTON_SCALE_DOWN) {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- (%s/%s): %s", SDL_GetGamepadStringForButton(GAMEPAD_SCALE_DOWN), SDL_GetGamepadStringForButton(GAMEPAD_SCALE_UP), desc);
                } else {
                    SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (y_offset * 16.0f)) * 2 , "- (%s): %s", SDL_GetGamepadStringForButton(*gamepad), desc);
                }
            }
        }
        SDL_SetRenderScale(renderer, (float)RENDER_SCALE, (float)RENDER_SCALE);
    }

    SDL_RenderPresent(renderer);
}

void do_reset() {
    item_hist[0] = ITEM_NEGATE;
    item_hist[1] = ITEM_SHOTGUN;
    fill_item_bag();
    generate_first_piece();
    SDL_memset(time_spent_at_level, 0, sizeof time_spent_at_level);
    SDL_memset(field, 0, sizeof field);
    SDL_memset(jingle_played, 0, sizeof jingle_played);
    game_state = STATE_WAIT;
    game_state_ctr = 60;
    lines_cleared = 0;
    border_r = 0.1f;
    border_g = 0.1f;
    border_b = 0.9f;
    first_piece = true;
    section_locked = false;
    previous_clears = 0;
    in_roll = false;
    in_roll_remaining = 0;
    frozen_rows = 0;
    frozen_ignore_next = false;
    bones = false;
    ti_garbage_quota = false;
    big_mode = false;
    big_mode_half_columns = false;
    selective_gravity = false;
    disable_selective_gravity_after_clear = false;
    item_mode = false;
    SDL_memset(effect_overlay, 0, sizeof(effect_overlay));
    effect_overlay_ctr = 0;
    xray_ctr = 0;
    garbage_ctr = -1;
}

void update_details() {
    if (level < 0xFFFF) time_spent_at_level[level]++;
    game_details.total_frames++;
    if (!in_roll && level >= (game_details.current_section + 1) * 100) {
        game_details.current_section++;
        game_details.highest_regular_section = game_details.current_section;
    } else if (in_roll) {
        in_roll_remaining--;
        if (in_roll_remaining == 0) {
            game_state = STATE_GAMEOVER;
            game_state_ctr = 10 * 24 + 1;
            play_sound(&complete_sound);
        }
        game_details.current_section = game_details.highest_regular_section + 1;
    }
    game_details.splits[game_details.current_section].lap_frames++;
    game_details.splits[game_details.current_section].split_frames = game_details.total_frames;
}

bool state_machine_tick() {
    // Quit?
    if (IS_JUST_HELD(button_reset_held) && game_state == STATE_WAIT) return false;

    // Details?
    if (IS_JUST_HELD(button_details_held)) toggle_details();

    // Mute?
    if (IS_JUST_HELD(button_mute_held)) MUTED = !MUTED;

    // Transparency?
    if (IS_JUST_HELD(button_toggle_transparency_held)) TRANSPARENCY = !TRANSPARENCY;

    // Huh?
    if (IS_JUST_HELD(button_mystery_held)) mystery = !mystery;

    // Reset?
    if (IS_JUST_HELD(button_reset_held) && game_state != STATE_WAIT) {
        do_reset();
        return true;
    }

    // Rescale?
    if (IS_JUST_HELD(button_scale_down_held) && RENDER_SCALE != 1) {
        RENDER_SCALE--;
        apply_scale();
    } else if (IS_JUST_HELD(button_scale_up_held)) {
        RENDER_SCALE++;
        apply_scale();
    }

    // Do all the logic.
    if (game_state == STATE_WAIT) {
        if (IS_JUST_HELD(button_l_held)) change_setting(selected_setting, -1);
        if (IS_JUST_HELD(button_r_held)) change_setting(selected_setting, 1);

        // Specifically for start level...
        if (selected_setting == STARTING_LEVEL_SETTING_IDX) {
            if (IS_HELD_FOR_AT_LEAST(button_l_held, 600)) change_setting(selected_setting, -100);
            else if (IS_HELD_FOR_AT_LEAST(button_l_held, 120)) change_setting(selected_setting, -10);
            else if (IS_HELD_FOR_AT_LEAST(button_l_held, 30)) change_setting(selected_setting, -1);
            if (IS_HELD_FOR_AT_LEAST(button_r_held, 600)) change_setting(selected_setting, 100);
            else if (IS_HELD_FOR_AT_LEAST(button_r_held, 120)) change_setting(selected_setting, 10);
            else if (IS_HELD_FOR_AT_LEAST(button_r_held, 30)) change_setting(selected_setting, 1);
        }

        if (IS_JUST_HELD(button_u_held)) change_selected_setting(-1);
        if (IS_JUST_HELD(button_d_held)) change_selected_setting(1);

        if (IS_JUST_HELD(button_start_held)) {
            game_state = STATE_BEGIN;
            game_state_ctr = 60;
            play_sound(&ready_sound);
            current_timing = GAME_TIMINGS;
            level = get_setting_value(STARTING_LEVEL_SETTING_IDX);
            while ((current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) current_timing = current_timing+1;
            check_garbage();
            check_effect();
            if (big_mode) {
                next_piece[0].is_big = true;
                next_piece[1].is_big = true;
                next_piece[2].is_big = true;
            }
            if (bones) {
                next_piece[0].is_bone = true;
                next_piece[1].is_bone = true;
                next_piece[2].is_bone = true;
            }
            for (int i = 0; i < SETTINGS_COUNT; ++i) locked_settings[i] = settings_values[i];
            calculate_mode_hash();
        }
    } else if (game_state == STATE_BEGIN) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
            SDL_memset(&game_details, 0, sizeof(game_details_t));
            while (level >= (game_details.current_section + 1) * 100) {
                game_details.current_section++;
                game_details.highest_regular_section = game_details.current_section;
            }
            game_state = STATE_ARE;
            game_state_ctr = 1;
            play_sound(&go_sound);
        }else if (game_state_ctr <= 3) {
            border_r = border_g = border_b = 1.0f;
        } else {
            border_r = lerp(0.9f, 0.1f, (float)(game_state_ctr-3)/57.0f);
            border_g = lerp(0.1f, 0.1f, (float)(game_state_ctr-3)/57.0f);
            border_b = lerp(0.1f, 0.9f, (float)(game_state_ctr-3)/57.0f);
        }
    } else if (game_state == STATE_ARE) {
        update_details();
        game_state_ctr--;
        if (game_state_ctr <= 0) {
            if (!first_piece) increment_level_piece_spawn();
            if (game_state != STATE_ARE) return true;
            first_piece = false;
            generate_next_piece();
            if (piece_collides(current_piece) != -1) {
                write_piece(current_piece);
                current_piece.type = BLOCK_VOID;
                game_state = STATE_GAMEOVER;
                game_state_ctr = 10 * 24 + 1;
                play_sound(&gameover_sound);
            } else {
                game_state = STATE_ACTIVE;
                game_state_ctr = 0;
                if (xray_ctr > 0) xray_ctr--;
            }
        }
    } else if (game_state == STATE_ACTIVE) {
        update_details();
        game_state_ctr++;
        const bool was_grounded = piece_grounded();

        // Hold.
        if (can_hold && get_setting_value(HOLD_SETTING_IDX) == 1 && IS_JUST_HELD(button_hold_held)) {
            do_hold(false);
            return true;
        }

        // Rotate.
        const int prev_state = current_piece.rotation_state;
        const bool prev_kicked = current_piece.floor_kicked;
        if ((IS_JUST_HELD(button_a_held) || IS_JUST_HELD(button_c_held)) && !IS_JUST_HELD(button_b_held)) try_rotate(1);
        if ((!IS_JUST_HELD(button_a_held) && !IS_JUST_HELD(button_c_held)) && IS_JUST_HELD(button_b_held)) try_rotate(-1);
        if (current_piece.is_rolling && (game_state_ctr % 30 == 0)) try_rotate(1);
        if (prev_state != current_piece.rotation_state && prev_kicked) current_piece.lock_delay = 0;

        // Move.
        if (IS_HELD(button_l_held) && IS_HELD(button_r_held)) {
            button_l_held = 0;
            button_r_held = 0;
        } else if (IS_JUST_HELD(button_l_held) || IS_HELD_FOR_AT_LEAST(button_l_held, current_timing->das)) {
            try_move(-1);
            if (big_mode && !big_mode_half_columns && current_piece.x % 2 == 1) try_move(-1);
        } else if (IS_JUST_HELD(button_r_held) || IS_HELD_FOR_AT_LEAST(button_r_held, current_timing->das)) {
            try_move(1);
            if (big_mode && !big_mode_half_columns && current_piece.x % 2 == 1) try_move(1);
        }

        // Gravity.
        accumulated_g += get_gravity();

        // Soft drop.
        if (get_gravity() < 256 && IS_HELD(button_d_held)) accumulated_g = 256;

        // Sonic drop.
        if (IS_JUST_HELD(button_u_held) || IS_JUST_HELD(button_hard_drop_held) || ((IS_HELD(button_u_held) || IS_HELD(button_hard_drop_held)) && game_state_ctr > 0)) accumulated_g += 20*256;

        // Apply gravity.
        const int start_y = current_piece.y;
        if (accumulated_g >= 256) {
            if (accumulated_g >= 20*256) {
                while (!piece_grounded()) {
                    try_descend();
                }
                accumulated_g = 0;
            } else {
                while (accumulated_g >= 256) {
                    try_descend();
                    accumulated_g -= 256;
                }
            }
            if (start_y != current_piece.y && !(current_piece.floor_kicked && current_piece.lock_delay == 0) && !current_piece.is_heavy) {
                current_piece.lock_param = 1.0f;
                current_piece.lock_delay = current_timing->lock;
            }
        }

        // Lock?
        if (piece_grounded()) {
            if (!was_grounded || start_y != current_piece.y) play_sound(&pieceland_sound);
            current_piece.lock_delay--;
            if (current_piece.is_heavy) current_piece.lock_delay--;
            if (current_piece.lock_delay < 0) current_piece.lock_delay = 0;
            // Soft lock.
            if (IS_HELD(button_d_held) || IS_HELD(button_hard_drop_held)) current_piece.lock_delay = 0;
            current_piece.lock_param = (float)current_piece.lock_delay / (float)current_timing->lock;
            if (current_piece.lock_delay == 0) {
                play_sound(&piecelock_sound);
                current_piece.lock_status = LOCK_FLASH;
                game_state = STATE_LOCKFLASH;
                write_piece(current_piece);
                game_state_ctr = 3;
            }
        }
    } else if (game_state == STATE_LOCKFLASH) {
        update_details();
        game_state_ctr--;
        if (game_state_ctr != 0) return true;
        current_piece.type = BLOCK_VOID;
        check_clears();
        if (game_state != STATE_LOCKFLASH) return true;
        if (lines_cleared != 0) {
            play_sound(&lineclear_sound);
            wipe_clears();
            game_state_ctr = current_timing->clear;
            game_state = STATE_CLEAR;
        } else {
            game_state = STATE_ARE;
            game_state_ctr = current_timing->are - 3;
        }
    } else if (game_state == STATE_CLEAR) {
        update_details();
        game_state_ctr--;
        if (game_state_ctr > 0) return true;
        collapse_clears(true);
        game_state = STATE_ARE;
        game_state_ctr = current_timing->line_are - 3;
    } else if (game_state == STATE_GAMEOVER) {
        effect_overlay_ctr = 0;
        in_roll = false;
        game_state_ctr--;
        if (game_state_ctr < 0) {
            if (game_state_ctr == -120) do_reset();
        } else {
            gray_line(game_state_ctr / 10);
        }
    } else if (game_state == STATE_FADEOUT) {
        game_state_ctr--;
        if (game_state_ctr == -120) {
            game_state = STATE_ARE;
            game_state_ctr = current_timing->are;
            in_roll_remaining = current_timing->duration;
            SDL_memset(field, 0, sizeof field);
        }
    }  else if (game_state == STATE_PAUSED) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
            game_state = game_state_old;
            game_state_ctr = game_state_ctr_old;
            // Allow IRS after pause end.
            button_a_held = button_b_held = button_c_held = -1;
        } else if (game_state_ctr <= 3) {
            border_r = border_g = border_b = 1.0f;
        } else {
            border_r = lerp(border_r_old, 0.4f, (float)(game_state_ctr-3)/57.0f);
            border_g = lerp(border_g_old, 0.4f, (float)(game_state_ctr-3)/57.0f);
            border_b = lerp(border_b_old, 0.4f, (float)(game_state_ctr-3)/57.0f);
        }
    }

    return true;
}

SDL_HitTestResult window_hit_test(SDL_Window *win, const SDL_Point *area, void *data) {
    return SDL_HITTEST_DRAGGABLE;
}

SDL_Texture *load_image(const char* file, const void* fallback, const size_t fallback_size) {
    SDL_Texture *target = IMG_LoadTexture(renderer, file);
    if (target == NULL) {
        SDL_IOStream *t = SDL_IOFromConstMem(fallback, fallback_size);
        target = IMG_LoadTextureTyped_IO(renderer, t, true, "PNG");
    }
    SDL_SetTextureScaleMode(target, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    return target;
}

void load_sound(sound_t *target, const char* file, const void* fallback, const size_t fallback_size) {
    int channels, sample_rate;
    int16_t *data;
    int32_t data_len = stb_vorbis_decode_filename(file, &channels, &sample_rate, &data);
    if (data_len == -1 && fallback != NULL) {
        data_len = stb_vorbis_decode_memory(fallback, (int)fallback_size, &channels, &sample_rate, &data);
    }

    if (data_len == -1) {
        target->stream = NULL;
        target->wave_data = NULL;
        target->wave_data_len = 0;
    } else {
        target->wave_data = data;
        target->wave_data_len = data_len * channels * 2;
        SDL_AudioSpec spec = {.format = SDL_AUDIO_S16, .channels = channels, .freq = sample_rate};
        target->stream = SDL_CreateAudioStream(&spec, NULL);
        SDL_SetAudioStreamGain(target->stream, SFX_VOLUME);
        SDL_BindAudioStream(audio_device, target->stream);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    fill_item_bag();
    load_config();
    for (int i = 0; i < SETTINGS_COUNT; ++i) locked_settings[i] = settings_values[i];
    calculate_mode_hash();

    seed_rng();
    generate_first_piece();
    current_timing = GAME_TIMINGS;

    // Create the window.
    SDL_SetAppMetadata("Tinytris", "1.0", "org.villadelfia.tinytris");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    SDL_CreateWindowAndRenderer("Tinytris", 12*16*RENDER_SCALE, 26*16*RENDER_SCALE, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS, &window, &renderer);
    old_w = 0;
    old_h = 0;
    SDL_SetRenderScale(renderer, 1.0f * (float)RENDER_SCALE, 1.0f * (float)RENDER_SCALE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    apply_scale();

    // Make the window entirely draggable.
    SDL_SetWindowHitTest(window, window_hit_test, NULL);

    // Load the image for a block.
    atlas_texture = load_image("data/atlas.png", atlas, sizeof atlas);

    // Make audio device.
    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    // Load the sounds.
    load_sound(&lineclear_sound, "data/lineclear.ogg", lineclear, sizeof lineclear);
    load_sound(&linecollapse_sound, "data/linecollapse.ogg", linecollapse, sizeof linecollapse);
    load_sound(&pieceland_sound, "data/pieceland.ogg", pieceland, sizeof pieceland);
    load_sound(&piecelock_sound, "data/piecelock.ogg", piecelock, sizeof piecelock);
    load_sound(&irs_sound, "data/irs.ogg", irs, sizeof irs);
    load_sound(&ready_sound, "data/ready.ogg", ready, sizeof ready);
    load_sound(&go_sound, "data/go.ogg", go, sizeof go);
    load_sound(&hold_sound, "data/hold.ogg", hold, sizeof hold);

    // Optional sounds
    load_sound(&i_sound, "data/i_mino.ogg", NULL, 0);
    load_sound(&s_sound, "data/s_mino.ogg", NULL, 0);
    load_sound(&z_sound, "data/z_mino.ogg", NULL, 0);
    load_sound(&j_sound, "data/j_mino.ogg", NULL, 0);
    load_sound(&l_sound, "data/l_mino.ogg", NULL, 0);
    load_sound(&o_sound, "data/o_mino.ogg", NULL, 0);
    load_sound(&t_sound, "data/t_mino.ogg", NULL, 0);
    load_sound(&section_lock_sound, "data/section_lock.ogg", NULL, 0);
    load_sound(&section_pass_sound, "data/section_pass.ogg", NULL, 0);
    load_sound(&combo_sound, "data/combo.ogg", NULL, 0);
    load_sound(&tetris_sound, "data/tetris.ogg", NULL, 0);
    load_sound(&tetris_b2b_sound, "data/tetris_b2b.ogg", NULL, 0);
    load_sound(&gameover_sound, "data/gameover.ogg", NULL, 0);
    load_sound(&complete_sound, "data/complete.ogg", NULL, 0);

    // Setup music audio stream if needed.
    if (TITLE_MUSIC.level_start == -1 || SECTION_COUNT != 0) {
        SDL_AudioSpec s = {.format = SDL_AUDIO_S16, .channels = 2, .freq = 44100};
        music = SDL_CreateAudioStream(&s, NULL);
        SDL_BindAudioStream(audio_device, music);
    }

    last_time = SDL_GetTicksNS();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    // Get input.
    update_input_states(paused);

    // Pause?
    if (IS_JUST_HELD(button_pause_held)) paused = !paused;

    if (SDL_GetKeyboardFocus() == window && !paused) {
        if (!state_machine_tick()) return SDL_APP_SUCCESS;
    } else if (game_state != STATE_PAUSED && game_state != STATE_WAIT && game_state != STATE_GAMEOVER) {
        game_state_old = game_state;
        game_state_ctr_old = game_state_ctr;
        game_state = STATE_PAUSED;
        game_state_ctr = 60;
        if (game_state_old == STATE_BEGIN) {
            border_r_old = border_r;
            border_g_old = border_g;
            border_b_old = border_b;
        } else {
            border_r_old = 0.9f;
            border_g_old = 0.1f;
            border_b_old = 0.1f;
        }
        border_r = border_g = border_b = 0.4f;
    } else if (game_state == STATE_PAUSED) {
        game_state_ctr = 60;
        border_r = border_g = border_b = 0.4f;
    }

    // Render the game.
    render_game();

    // Pump the music engine. Choo choo.
    play_music();

    // Delay until next frame.
    if (accumulated_time > FRAME_TIME + SDL_NS_PER_MS * SDL_SINT64_C(100)) {
        // Reset if we're way off target (windowing system stuff can cause
        // that).
        accumulated_time = SDL_SINT64_C(0);
        last_time = SDL_GetTicksNS();
    } else {
        const Uint64 initial_time = (SDL_GetTicksNS() - last_time) + accumulated_time;
        if (initial_time < FRAME_TIME) {
            SDL_DelayPrecise(FRAME_TIME - initial_time);
        }
        const Uint64 now = SDL_GetTicksNS();
        accumulated_time += (Sint64)(now - last_time) - FRAME_TIME;
        last_time = now;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}
