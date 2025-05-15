#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>
#include <stdint.h>
#include <ini.h>
#include "rng.h"
#include "game_data.h"
#include "game_config.h"
#include "input_utils.h"

// SDL stuff.
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Gamepad *gamepad = NULL;
static SDL_Texture *block_texture = NULL;
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
block_t field[10][21] = {0};
block_type_t history[4] = {0};
block_type_t next_piece;
live_block_t current_piece;
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
int clears[4] = {-1, -1, -1, -1};
bool mystery = false;
bool section_locked = false;
int previous_clears = 0;
bool first_piece = true;
bool intro = true;
float volume = 1.0f;
section_music_t *last_section = NULL;

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

int32_t button_u_held = 0;
int32_t button_l_held = 0;
int32_t button_d_held = 0;
int32_t button_r_held = 0;
int32_t button_a_held = 0;
int32_t button_b_held = 0;
int32_t button_c_held = 0;
int32_t button_start_held = 0;
int32_t button_reset_held = 0;
int32_t button_quit_held = 0;
int32_t button_scale_up_held = 0;
int32_t button_scale_down_held = 0;
int32_t button_mute_held = 0;
int32_t button_mystery_held = 0;
int32_t button_hard_drop_held = 0;
int32_t button_toggle_rotation_system_held = 0;
int32_t button_toggle_transparency_held = 0;

rotation_state_t get_rotation_state(const block_type_t piece, const int rotation_state) {
    return ROTATION_STATES[piece][rotation_state];
}

float lerp(const float a, const float b, const float f) {
    return (float)(a * (1.0 - f) + (b * f));
}

void apply_scale() {
    SDL_SetWindowSize(window, 12*16*RENDER_SCALE, 26*16*RENDER_SCALE);
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

void update_input_states() {
    const bool* state = SDL_GetKeyboardState(NULL);
    update_input_state(state, BUTTON_LEFT, GAMEPAD_LEFT, &button_l_held);
    update_input_state(state, BUTTON_RIGHT, GAMEPAD_RIGHT, &button_r_held);

    update_input_state(state, BUTTON_SONIC_DROP, GAMEPAD_SONIC_DROP, &button_u_held);
    update_input_state(state, BUTTON_SOFT_DROP, GAMEPAD_SOFT_DROP, &button_d_held);
    update_input_state(state, BUTTON_HARD_DROP, GAMEPAD_HARD_DROP, &button_hard_drop_held);

    update_input_state(state, BUTTON_CCW_1, GAMEPAD_CCW_1, &button_a_held);
    update_input_state(state, BUTTON_CCW_2, GAMEPAD_CCW_2, &button_c_held);
    update_input_state(state, BUTTON_CW, GAMEPAD_CW, &button_b_held);

    update_input_state(state, BUTTON_RESET, GAMEPAD_RESET, &button_reset_held);
    update_input_state(state, BUTTON_START, GAMEPAD_START, &button_start_held);
    update_input_state(state, BUTTON_QUIT, GAMEPAD_QUIT, &button_quit_held);

    update_input_state(state, BUTTON_SCALE_UP, GAMEPAD_SCALE_UP, &button_scale_up_held);
    update_input_state(state, BUTTON_SCALE_DOWN, GAMEPAD_SCALE_DOWN, &button_scale_down_held);
    update_input_state(state, BUTTON_TOGGLE_ROTATION_SYSTEM, GAMEPAD_TOGGLE_ROTATION_SYSTEM, &button_toggle_rotation_system_held);
    update_input_state(state, BUTTON_TOGGLE_TRANSPARENCY, GAMEPAD_TOGGLE_TRANSPARENCY, &button_toggle_transparency_held);
    update_input_state(state, BUTTON_MUTE, GAMEPAD_MUTE, &button_mute_held);

    update_input_state(state, BUTTON_MYSTERY, GAMEPAD_MYSTERY, &button_mystery_held);
}

void play_sound(const sound_t *sound) {
    if (MUTED || SFX_VOLUME == 0.0f) return;
    if (sound->stream == NULL) return;
    SDL_ClearAudioStream(sound->stream);
    SDL_PutAudioStreamData(sound->stream, sound->wave_data, (int)sound->wave_data_len);
}

void increment_level_piece_spawn() {
    if (level % 100 == 99) return;
    level++;
    if ((current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) {
        current_timing = current_timing+1;
    }
}

void increment_level_line_clear(const int lines) {
    if (lines == 0) {
        if (level % 100 == 99) {
            if (!section_locked) {
                play_sound(&section_lock_sound);
                section_locked = true;
            }
        }
        previous_clears = 0;
        return;
    }

    section_locked = false;
    if (lines == 4) {
        if (previous_clears == 4) play_sound(&tetris_b2b_sound);
        else play_sound(&tetris_sound);
    }

    if (previous_clears != 0 && lines >= 2) {
        // Avoid playing tetris_b2b + combo. Tetris + combo is allowed.
        if (lines != 4 || previous_clears != 4) play_sound(&combo_sound);
    }
    previous_clears = lines;
    if ((level / 100) < ((level+lines) / 100)) play_sound(&section_pass_sound);
    level += lines;
    if ((current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) {
        current_timing = current_timing+1;
    }
}

int piece_collides(const live_block_t piece) {
    if (piece.type == BLOCK_VOID) return -1;

    const rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (rotation[4*j + i] != ' ' && piece.y + j + 1 >= 0) {
                if (piece.y + j + 1 > 20 || piece.x + i < 0 || piece.x + i > 9 || field[piece.x + i][piece.y + j + 1].type != BLOCK_VOID) {
                    return 4*j+i;
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
            if (rotation[4*j + i] != ' ' && piece.x + i >= 0 && piece.x + i < 10 && piece.y + j + 1 >= 0 && piece.y + j + 1 < 21) {
                field[piece.x + i][piece.y + j + 1].type = piece.type;
                field[piece.x + i][piece.y + j + 1].lock_param = 1.0f;
                field[piece.x + i][piece.y + j + 1].lock_status = LOCK_LOCKED;
            }
        }
    }
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
        if (!TI_ARS) return;
        if (touching_stack() && (active.rotation_state == 0 || active.rotation_state == 2)) {
            // I -> right
            active.x += 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x += 1;
                return;
            }

            // I -> 2right
            active.x += 2;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x += 2;
                return;
            }

            // I -> left
            active.x -= 3;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.x -= 1;
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
        }
    } else {
        // right
        active.x += 1;
        if (piece_collides(active) == -1) {
            current_piece.rotation_state = active.rotation_state;
            current_piece.x += 1;
            return;
        }

        // left
        active.x -= 2;
        if (piece_collides(active) == -1) {
            current_piece.rotation_state = active.rotation_state;
            current_piece.x -= 1;
            return;
        }

        // T -> up
        if (!TI_ARS) return;
        if (current_piece.type == BLOCK_T && piece_grounded()) {
            active.x += 1;
            active.y -= 1;
            if (piece_collides(active) == -1) {
                current_piece.rotation_state = active.rotation_state;
                current_piece.y -= 1;
                current_piece.floor_kicked = true;
            }
        }
    }
}

void check_clears() {
    clears[0] = -1;
    clears[1] = -1;
    clears[2] = -1;
    clears[3] = -1;

    int ct = 0;

    for (int i = 0; i < 21; i++) {
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
            clears[ct++] = i;
        }
    }

    lines_cleared = ct;
    increment_level_line_clear(ct);
}

void wipe_cleared() {
    for (int i = 0; i < 4; ++i) {
        if (clears[i] == -1) break;
        field[0][clears[i]].type = BLOCK_VOID;
        field[1][clears[i]].type = BLOCK_VOID;
        field[2][clears[i]].type = BLOCK_VOID;
        field[3][clears[i]].type = BLOCK_VOID;
        field[4][clears[i]].type = BLOCK_VOID;
        field[5][clears[i]].type = BLOCK_VOID;
        field[6][clears[i]].type = BLOCK_VOID;
        field[7][clears[i]].type = BLOCK_VOID;
        field[8][clears[i]].type = BLOCK_VOID;
        field[9][clears[i]].type = BLOCK_VOID;
    }
}

void collapse_clears() {
    for (int i = 0; i < 4; ++i) {
        if (clears[i] == -1) break;
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
        field[0][0].type = BLOCK_VOID;
        field[1][0].type = BLOCK_VOID;
        field[2][0].type = BLOCK_VOID;
        field[3][0].type = BLOCK_VOID;
        field[4][0].type = BLOCK_VOID;
        field[5][0].type = BLOCK_VOID;
        field[6][0].type = BLOCK_VOID;
        field[7][0].type = BLOCK_VOID;
        field[8][0].type = BLOCK_VOID;
        field[9][0].type = BLOCK_VOID;
    }
}

block_type_t generate_piece() {
    uint8_t piece = xoroshiro_next() % 8;
    while(piece > 6) piece = xoroshiro_next() % 8;
    return (block_type_t)(piece + 2);
}

void generate_first_piece() {
    history[1] = BLOCK_Z;
    history[2] = BLOCK_S;
    history[3] = BLOCK_S;
    block_type_t result = generate_piece();
    while(result == BLOCK_S || result == BLOCK_Z || result == BLOCK_O) result = generate_piece();
    history[0] = result;
    current_piece = (live_block_t){0};
    current_piece.type = BLOCK_VOID;
    next_piece = result;
}

void generate_next_piece() {
    block_type_t result = generate_piece();
    int tries = 1;
    while(tries++ < 6) {
        if(result != history[0] && result != history[1] && result != history[2] && result != history[3]) {
            break;
        } else {
            result = generate_piece();
        }
    }
    history[0] = history[1];
    history[1] = history[2];
    history[2] = history[3];
    history[3] = result;

    current_piece = (live_block_t){0};
    current_piece.type = next_piece;
    next_piece = result;
    current_piece.x = 3;
    current_piece.y = -1;
    current_piece.lock_param = 1.0f;
    current_piece.lock_status = LOCK_UNLOCKED;
    current_piece.rotation_state = 0;
    current_piece.lock_delay = current_timing->lock;
    current_piece.floor_kicked = false;

    // Play sound.
    if (next_piece == BLOCK_I) play_sound(&i_sound);
    if (next_piece == BLOCK_S) play_sound(&s_sound);
    if (next_piece == BLOCK_Z) play_sound(&z_sound);
    if (next_piece == BLOCK_J) play_sound(&j_sound);
    if (next_piece == BLOCK_L) play_sound(&l_sound);
    if (next_piece == BLOCK_O) play_sound(&o_sound);
    if (next_piece == BLOCK_T) play_sound(&t_sound);

    // Apply IRS.
    if ((IS_HELD(button_a_held) || IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = 1;
    if ((IS_RELEASED(button_a_held)&& IS_RELEASED(button_c_held)) && IS_HELD(button_b_held)) current_piece.rotation_state = 3;
    if ((IS_HELD(button_a_held) && IS_HELD(button_c_held)) && IS_RELEASED(button_b_held)) current_piece.rotation_state = 2;
    if (current_piece.rotation_state != 0) {
        if (piece_collides(current_piece) != -1) current_piece.rotation_state = 0;
        else play_sound(&irs_sound);
    }

    if (current_timing->g == 5120) {
        for (int i = 0; i < 20; i++) try_descend();
        play_sound(&pieceland_sound);
    }
}

void gray_line(const int row) {
    for (int i = 0; i < 10; i++) {
        if (field[i][row].type != BLOCK_VOID) field[i][row].type = BLOCK_X;
    }
}

void render_raw_block(const int col, const int row, const block_type_t block, const lock_status_t lockStatus, const float lockParam, bool voidToLeft, bool voidToRight, bool voidAbove, bool voidBelow) {
    SDL_FRect src = {0};
    const SDL_FRect dest = {
        .x = FIELD_X_OFFSET + ((float)col * 16.0f),
        .y = FIELD_Y_OFFSET + ((float)row * 16.0f),
        .w = 16.0f,
        .h = 16.0f
    };
    float w, h;
    SDL_GetTextureSize(block_texture, &w, &h);
    h /= 2.0f;
    w /= 4.0f;
    switch(block) {
        case BLOCK_X:
            src = (SDL_FRect){.x = 0.0f, .y = 0.0f, .w = w, .h = h};
            break;
        case BLOCK_I:
            src = (SDL_FRect){.x = 16.0f, .y = 0.0f, .w = w, .h = h};
            break;
        case BLOCK_L:
            src = (SDL_FRect){.x = 32.0f, .y = 0.0f, .w = w, .h = h};
            break;
        case BLOCK_O:
            src = (SDL_FRect){.x = 48.0f, .y = 0.0f, .w = w, .h = h};
            break;
        case BLOCK_Z:
            src = (SDL_FRect){.x = 0.0f, .y = 16.0f, .w = w, .h = h};
            break;
        case BLOCK_T:
            src = (SDL_FRect){.x = 16.0f, .y = 16.0f, .w = w, .h = h};
            break;
        case BLOCK_J:
            src = (SDL_FRect){.x = 32.0f, .y = 16.0f, .w = w, .h = h};
            break;
        case BLOCK_S:
            src = (SDL_FRect){.x = 48.0f, .y = 16.0f, .w = w, .h = h};
            break;
        default:
            return;
    }

    if (col == 0) voidToLeft = false;
    if (col == 9) voidToRight = false;
    if (row == 19) voidBelow = false;

    float mod = 1.0f;
    float moda = 1.0f;
    switch(lockStatus) {
        case LOCK_LOCKED:
            mod = 0.6f;
            moda = 0.8f;
            if (game_state == STATE_GAMEOVER && game_state_ctr < 0) {
                moda = lerp(0.8f, 0.0f, ((float)-game_state_ctr/100.0f));
            }
            break;
        case LOCK_FLASH:
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 192);
            SDL_RenderFillRect(renderer, &dest);
            return;
        case LOCK_UNLOCKED:
            mod = 0.5f + (lockParam * 0.5f);
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            break;
        case LOCK_GHOST:
            mod = 0.8f;
            moda = TRANSPARENCY ? 0.125f : 0.25f;
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            if (TRANSPARENCY) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 32);
                SDL_RenderFillRect(renderer, &dest);
            }
            break;
        case LOCK_NEXT:
            moda = 0.8f;
            break;
    }

    SDL_SetTextureColorModFloat(block_texture, mod, mod, mod);
    SDL_SetTextureAlphaModFloat(block_texture, moda);
    if (moda <= 0.0f) return;
    SDL_RenderTexture(renderer, block_texture, &src, &dest);

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

void render_field_block(const int x, const int y) {
    if(x < 0 || x > 9 || y < 1 || y > 20) return;
    const bool voidToLeft = x != 0 && field[x-1][y].type == BLOCK_VOID;
    const bool voidToRight = x != 9 && field[x+1][y].type == BLOCK_VOID;
    const bool voidAbove = y != 1 && field[x][y-1].type == BLOCK_VOID;
    const bool voidBelow = y != 20 && field[x][y+1].type == BLOCK_VOID;
    render_raw_block(x, y-1, field[x][y].type, field[x][y].lock_status, field[x][y].lock_param, voidToLeft, voidToRight, voidAbove, voidBelow);
}

void render_next_block() {
    switch(next_piece) {
        case BLOCK_I:
            render_raw_block(3, -4, BLOCK_I, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_I, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_I, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(6, -4, BLOCK_I, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_L:
            render_raw_block(3, -4, BLOCK_L, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_L, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_L, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(3, -3, BLOCK_L, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_O:
            render_raw_block(4, -4, BLOCK_O, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_O, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_O, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_O, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_Z:
            render_raw_block(3, -4, BLOCK_Z, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_Z, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_Z, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_Z, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_T:
            render_raw_block(3, -4, BLOCK_T, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_T, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_T, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_T, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_J:
            render_raw_block(3, -4, BLOCK_J, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_J, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_J, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_J, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        case BLOCK_S:
            render_raw_block(4, -4, BLOCK_S, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_S, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(3, -3, BLOCK_S, LOCK_NEXT, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_S, LOCK_NEXT, 1.0f, false, false, false, false);
            break;
        default:
            return;
    }
}

void render_current_block() {
    if(current_piece.type == BLOCK_VOID) return;

    const rotation_state_t rotation = get_rotation_state(current_piece.type, current_piece.rotation_state);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotation[4*j + i] != ' ') {
                render_raw_block(current_piece.x + i, current_piece.y + j, current_piece.type, current_piece.lock_status, current_piece.lock_param, false, false, false, false);
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
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotation[4*j + i] != ' ') {
                render_raw_block(active.x + i, active.y + j, active.type, LOCK_GHOST, active.lock_param, false, false, false, false);
            }
        }
    }
}

void render_field() {
    for (int x = 0; x < 10; x++) {
        for (int y = 1; y < 21; y++) {
            render_field_block(x, y);
        }
    }
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
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, TRANSPARENCY ? 0.5f : 1.0f);
    SDL_FRect dst = {.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET, .w = 16.0f * 10.0f, .h = 16.0f * 20.0f};
    SDL_RenderFillRect(renderer, &dst);

    // Field border.
    if (game_state != STATE_PAUSED && game_state != STATE_BEGIN  && game_state != STATE_WAIT) SDL_SetRenderDrawColorFloat(renderer, 0.9f, 0.1f, 0.1f, TRANSPARENCY ? 0.8f : 1.0f);
    else SDL_SetRenderDrawColorFloat(renderer, border_r, border_g, border_b, TRANSPARENCY ? 0.8f : 1.0f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET - 8, .y = FIELD_Y_OFFSET - 8, .w = 8.0f, .h = 16.0f * 21.0f};
    SDL_RenderFillRect(renderer, &dst);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (10.0f * 16.0f), .y = FIELD_Y_OFFSET - 8, .w = 8.0f, .h = 16.0f * 21.0f};
    SDL_RenderFillRect(renderer, &dst);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET - 8, .w = 16.0f * 10.0f, .h = 8.0f};
    SDL_RenderFillRect(renderer, &dst);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET + (20.0f * 16.0f), .w = 16.0f * 10.0f, .h = 8.0f};
    SDL_RenderFillRect(renderer, &dst);

    // Background for next.
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.1f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 8, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 16, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.1f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 6, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 12, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.1f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 4, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 8, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.1f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 2, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 4, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.1f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f), .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);

    // Render the game.
    render_field();
    render_next_block();
    render_tls();
    render_current_block();

    // A bit of info.
    SDL_SetRenderScale(renderer, (float)RENDER_SCALE/2.0f, (float)RENDER_SCALE/2.0f);
    SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1.0f);
    SDL_RenderDebugTextFormat(renderer, 21.0f, (FIELD_Y_OFFSET + (20.0f * 16.0f) + 2) * 2 , "[LVL:%04d][GRV:%06.3f][DAS:%02d][LCK:%02d][%s]", level, (float)current_timing->g/256.0f, current_timing->das, current_timing->lock, TI_ARS ? "Ti " : "TAP");

    if (game_state == STATE_WAIT) {
        SDL_RenderDebugTextFormat(renderer, 80.0f, (FIELD_Y_OFFSET + (4.0f * 16.0f)) * 2 , "Press %s/%s to begin", SDL_GetScancodeName(BUTTON_START), SDL_GetGamepadStringForButton(GAMEPAD_START));

        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (5.0f * 16.0f)) * 2 , "Keyboard:");
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (5.5f * 16.0f)) * 2 , "- %s/%s: Left/right", SDL_GetScancodeName(BUTTON_LEFT), SDL_GetScancodeName(BUTTON_RIGHT));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (6.0f * 16.0f)) * 2 , "- %s: Sonic drop", SDL_GetScancodeName(BUTTON_SONIC_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (6.5f * 16.0f)) * 2 , "- %s: Soft drop", SDL_GetScancodeName(BUTTON_SOFT_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (7.0f * 16.0f)) * 2 , "- %s: Hard drop", SDL_GetScancodeName(BUTTON_HARD_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (7.5f * 16.0f)) * 2 , "- %s/%s: Rotate CCW", SDL_GetScancodeName(BUTTON_CCW_1), SDL_GetScancodeName(BUTTON_CCW_2));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (8.0f * 16.0f)) * 2 , "- %s: Rotate CW", SDL_GetScancodeName(BUTTON_CW));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (8.5f * 16.0f)) * 2 , "- %s: Start game", SDL_GetScancodeName(BUTTON_START));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (9.0f * 16.0f)) * 2 , "- %s: Reset game", SDL_GetScancodeName(BUTTON_RESET));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (9.5f * 16.0f)) * 2 , "- %s: Quit game", SDL_GetScancodeName(BUTTON_QUIT));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (10.0f * 16.0f)) * 2 , "- %s: Toggle rotation system", SDL_GetScancodeName(BUTTON_TOGGLE_ROTATION_SYSTEM));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (10.5f * 16.0f)) * 2 , "- %s: Scale down", SDL_GetScancodeName(BUTTON_SCALE_DOWN));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (11.0f * 16.0f)) * 2 , "- %s: Scale up", SDL_GetScancodeName(BUTTON_SCALE_UP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (11.5f * 16.0f)) * 2 , "- %s: Mute/unmute", SDL_GetScancodeName(BUTTON_MUTE));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (12.0f * 16.0f)) * 2 , "- %s: Toggle background transparency", SDL_GetScancodeName(BUTTON_TOGGLE_TRANSPARENCY));

        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (13.0f * 16.0f)) * 2 , "Gamepad:");
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (13.5f * 16.0f)) * 2 , "- %s/%s: Left/right", SDL_GetGamepadStringForButton(GAMEPAD_LEFT), SDL_GetGamepadStringForButton(GAMEPAD_RIGHT));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (14.0f * 16.0f)) * 2 , "- %s: Sonic drop", SDL_GetGamepadStringForButton(GAMEPAD_SONIC_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (14.5f * 16.0f)) * 2 , "- %s: Soft drop", SDL_GetGamepadStringForButton(GAMEPAD_SOFT_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (15.0f * 16.0f)) * 2 , "- %s: Hard drop", SDL_GetGamepadStringForButton(GAMEPAD_HARD_DROP));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (15.5f * 16.0f)) * 2 , "- %s/%s: Rotate CCW", SDL_GetGamepadStringForButton(GAMEPAD_CCW_1), SDL_GetGamepadStringForButton(GAMEPAD_CCW_2));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (16.0f * 16.0f)) * 2 , "- %s: Rotate CW", SDL_GetGamepadStringForButton(GAMEPAD_CW));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (16.5f * 16.0f)) * 2 , "- %s: Start game", SDL_GetGamepadStringForButton(GAMEPAD_START));
        SDL_RenderDebugTextFormat(renderer, 40.0f, (FIELD_Y_OFFSET + (17.0f * 16.0f)) * 2 , "- %s: Reset game", SDL_GetGamepadStringForButton(GAMEPAD_RESET));
    }

    SDL_SetRenderScale(renderer, (float)RENDER_SCALE, (float)RENDER_SCALE);

    SDL_RenderPresent(renderer);
}

void do_reset() {
    generate_first_piece();
    memset(field, 0, sizeof field);
    game_state = STATE_WAIT;
    game_state_ctr = 60;
    current_timing = &game_timing[0];
    accumulated_g = 0;
    level = 0;
    lines_cleared = 0;
    border_r = 0.1f;
    border_g = 0.1f;
    border_b = 0.9f;
    first_piece = true;
    section_locked = false;
    previous_clears = 0;
}

bool state_machine_tick() {
    // Get input.
    update_input_states();

    // Quit?
    if (IS_JUST_HELD(button_quit_held)) return false;

    // Mute?
    if (IS_JUST_HELD(button_mute_held)) MUTED = !MUTED;

    // Mute?
    if (IS_JUST_HELD(button_toggle_transparency_held)) TRANSPARENCY = !TRANSPARENCY;

    // ARS/Ti ARS?
    if (IS_JUST_HELD(button_toggle_rotation_system_held)) TI_ARS = !TI_ARS;

    // Huh?
    if (IS_JUST_HELD(button_mystery_held)) mystery = !mystery;

    // Reset?
    if (IS_JUST_HELD(button_reset_held)) {
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
        if (IS_JUST_HELD(button_start_held)) {
            game_state = STATE_BEGIN;
            game_state_ctr = 60;
            play_sound(&ready_sound);
        }
    } else if (game_state == STATE_BEGIN) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
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
        game_state_ctr--;
        if (game_state_ctr == 0) {
            if (!first_piece) increment_level_piece_spawn();
            first_piece = false;
            generate_next_piece();
            if (piece_collides(current_piece) != -1) {
                write_piece(current_piece);
                current_piece.type = BLOCK_VOID;
                game_state = STATE_GAMEOVER;
                game_state_ctr = 10 * 21 + 1;
                play_sound(&gameover_sound);
            } else {
                game_state = STATE_ACTIVE;
                game_state_ctr = -1;
            }
        }
    } else if (game_state == STATE_ACTIVE) {
        game_state_ctr++;
        const bool was_grounded = piece_grounded();

        // Rotate.
        const int prev_state = current_piece.rotation_state;
        const bool prev_kicked = current_piece.floor_kicked;
        if ((IS_JUST_HELD(button_a_held) || IS_JUST_HELD(button_c_held)) && !IS_JUST_HELD(button_b_held)) try_rotate(1);
        if ((!IS_JUST_HELD(button_a_held) && !IS_JUST_HELD(button_c_held)) && IS_JUST_HELD(button_b_held)) try_rotate(-1);
        if (prev_state != current_piece.rotation_state && prev_kicked) current_piece.lock_delay = 0;

        // Move.
        if (IS_HELD(button_l_held) && IS_HELD(button_r_held)) {
            button_l_held = 0;
            button_r_held = 0;
        } else if (IS_JUST_HELD(button_l_held) || IS_HELD_FOR_AT_LEAST(button_l_held, current_timing->das)) {
            try_move(-1);
        } else if (IS_JUST_HELD(button_r_held) || IS_HELD_FOR_AT_LEAST(button_r_held, current_timing->das)) {
            try_move(1);
        }

        // Gravity.
        accumulated_g += current_timing->g;

        // Soft drop.
        if (current_timing->g < 256 && IS_HELD(button_d_held)) accumulated_g = 256;

        // Sonic drop.
        if (IS_JUST_HELD(button_u_held) || IS_JUST_HELD(button_hard_drop_held) || ((IS_HELD(button_u_held) || IS_HELD(button_hard_drop_held)) && game_state_ctr > 0)) accumulated_g += 20*256;

        // Apply gravity.
        const int start_y = current_piece.y;
        if (accumulated_g >= 256) {
            while (accumulated_g >= 256) {
                try_descend();
                accumulated_g -= 256;
            }
            if (start_y != current_piece.y && !(current_piece.floor_kicked && current_piece.lock_delay == 0)) {
                current_piece.lock_param = 1.0f;
                current_piece.lock_delay = current_timing->lock;
            }
        }

        // Lock?
        if (piece_grounded()) {
            if (!was_grounded || start_y != current_piece.y) play_sound(&pieceland_sound);
            current_piece.lock_delay--;
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
        game_state_ctr--;
        if (game_state_ctr != 0) return true;
        current_piece.type = BLOCK_VOID;
        check_clears();
        if (lines_cleared != 0) {
            play_sound(&lineclear_sound);
            wipe_cleared();
            game_state_ctr = current_timing->clear;
            game_state = STATE_CLEAR;
        } else {
            game_state = STATE_ARE;
            game_state_ctr = current_timing->are - 3;
        }
    } else if (game_state == STATE_CLEAR) {
        game_state_ctr--;
        if (game_state_ctr != 0) return true;
        play_sound(&linecollapse_sound);
        collapse_clears();
        game_state = STATE_ARE;
        game_state_ctr = current_timing->line_are - 3;
    } else if (game_state == STATE_GAMEOVER) {
        game_state_ctr--;
        if (game_state_ctr < 0) {
            if (game_state_ctr == -120) do_reset();
        } else {
            gray_line(game_state_ctr / 10);
        }
    } else if (game_state == STATE_PAUSED) {
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

SDL_Texture *load_image(const char* file, void* fallback, const size_t fallback_size) {
    SDL_Texture *target = IMG_LoadTexture(renderer, file);
    if (target == NULL) {
        SDL_IOStream *t = SDL_IOFromMem(fallback, fallback_size);
        target = IMG_LoadTextureTyped_IO(renderer, t, true, "PNG");
    }
    SDL_SetTextureScaleMode(target, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    return target;
}

void load_sound(sound_t *target, const char* file, void* fallback, const size_t fallback_size) {
    int channels, sample_rate;
    int16_t *data;
    int32_t data_len = stb_vorbis_decode_filename(file, &channels, &sample_rate, &data);
    if (data_len == -1 && fallback != NULL) {
        data_len = stb_vorbis_decode_memory(fallback, fallback_size, &channels, &sample_rate, &data);
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
    load_config();

    seed_rng();
    generate_first_piece();
    current_timing = &game_timing[0];

    // Create the window.
    SDL_SetAppMetadata("Tinytris", "1.0", "org.villadelfia.tinytris");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    SDL_CreateWindowAndRenderer("Tinytris", 12*16*RENDER_SCALE, 26*16*RENDER_SCALE, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS, &window, &renderer);
    SDL_SetRenderScale(renderer, 1.0f * (float)RENDER_SCALE, 1.0f * (float)RENDER_SCALE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Make the window entirely draggable.
    SDL_SetWindowHitTest(window, window_hit_test, NULL);

    // Load the image for a block.
    block_texture = load_image("data/block.png", block, sizeof block);

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
    if (SDL_GetKeyboardFocus() == window) {
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