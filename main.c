#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>
#include <stdint.h>
#include "rng.h"
#include "game_data.h"
#include "game_config.h"
#include "input_utils.h"

// SDL stuff.
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *block_texture = NULL;
static SDL_AudioDeviceID audio_device = 0;
#define FRAME_TIME (SDL_NS_PER_SECOND / SDL_SINT64_C(60))
static Uint64 last_time = 0;
static Sint64 accumulated_time = 0;
int render_scale = 2;

typedef struct {
    uint8_t *wave_data;
    uint32_t wave_data_len;
    SDL_AudioStream *stream;
} sound_t;

// Global data.
block_t field[10][21] = {0};
block_type_t history[4] = {0};
block_type_t next_piece;
live_block_t current_piece;
game_state_t game_state = STATE_BEGIN;
game_state_t game_state_old = STATE_BEGIN;
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
bool muted = false;
bool mystery = false;

sound_t lineclear_sound;
sound_t linecollapse_sound;
sound_t pieceland_sound;
sound_t piecelock_sound;
sound_t irs_sound;
sound_t ready_sound;
sound_t go_sound;

int32_t button_u_held = 0;
int32_t button_l_held = 0;
int32_t button_d_held = 0;
int32_t button_r_held = 0;
int32_t button_a_held = 0;
int32_t button_b_held = 0;
int32_t button_c_held = 0;
int32_t button_reset_held = 0;
int32_t button_quit_held = 0;
int32_t button_scale_up_held = 0;
int32_t button_scale_down_held = 0;
int32_t button_mute_held = 0;
int32_t button_mystery_held = 0;
int32_t button_hard_drop_held = 0;

rotation_state_t get_rotation_state(const block_type_t piece, const int rotation_state) {
    return ROTATION_STATES[piece][rotation_state];
}

void apply_scale() {
    SDL_SetWindowSize(window, 12*16*render_scale, 26*16*render_scale);
    SDL_SetRenderScale(renderer, 1.0f * (float)render_scale, 1.0f * (float)render_scale);
}

void update_input_state(const bool *state, const int32_t scan_code, int32_t *input_data) {
    if (state[scan_code]) {
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
    update_input_state(state, BUTTON_U, &button_u_held);
    update_input_state(state, BUTTON_D, &button_d_held);
    update_input_state(state, BUTTON_L, &button_l_held);
    update_input_state(state, BUTTON_R, &button_r_held);
    update_input_state(state, BUTTON_A, &button_a_held);
    update_input_state(state, BUTTON_B, &button_b_held);
    update_input_state(state, BUTTON_C, &button_c_held);
    update_input_state(state, BUTTON_RESET, &button_reset_held);
    update_input_state(state, BUTTON_QUIT, &button_quit_held);
    update_input_state(state, BUTTON_SCALE_DOWN, &button_scale_down_held);
    update_input_state(state, BUTTON_SCALE_UP, &button_scale_up_held);
    update_input_state(state, BUTTON_MUTE, &button_mute_held);
    update_input_state(state, BUTTON_MYSTERY, &button_mystery_held);
    update_input_state(state, BUTTON_HARD_DROP, &button_hard_drop_held);
}

void play_sound(const sound_t *sound) {
    if (muted) return;
    SDL_ClearAudioStream(sound->stream);
    SDL_PutAudioStreamData(sound->stream, sound->wave_data, (int)sound->wave_data_len);
}

void increment_level(const int lines) {
    if (lines == 0) {
        if (level % 100 == 99) return;
        level++;
    } else {
        level += lines;
    }
    if ((current_timing + 1)->level != -1 && (current_timing + 1)->level <= level) {
        current_timing = current_timing+1;
    }
}

int piece_collides(const live_block_t piece) {
    if (piece.type == BLOCK_VOID) return -1;

    rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (rotation[4*j + i] != ' ') {
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

    rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

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
            SDL_SetWindowPosition(window, x+(direction*(render_scale*16)), y);
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

void try_rotate(const int direction) {
    if (current_piece.type == BLOCK_VOID) return;

    live_block_t active = current_piece;
    active.rotation_state += direction;
    if (active.rotation_state < 0) active.rotation_state = 3;
    if (active.rotation_state > 3) active.rotation_state = 0;

    int collision = piece_collides(active);
    if (collision == -1) {
        current_piece.rotation_state = active.rotation_state;
        return;
    }

    if (active.type == BLOCK_I || active.type == BLOCK_O) return;

    if (active.type == BLOCK_J || active.type == BLOCK_L || active.type == BLOCK_T) {
        if (collision == 1 || collision == 5 || collision == 9) return;
    }

    active.x += 1;
    if (piece_collides(active) == -1) {
        current_piece.rotation_state = active.rotation_state;
        current_piece.x += 1;
        return;
    }

    active.x -= 2;
    if (piece_collides(active) == -1) {
        current_piece.rotation_state = active.rotation_state;
        current_piece.x -= 1;
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
    SDL_FRect dest = {
        .x = FIELD_X_OFFSET + ((float)col * 16.0f),
        .y = FIELD_Y_OFFSET + ((float)row * 16.0f),
        .w = 16.0f,
        .h = 16.0f
    };
    switch(block) {
        case BLOCK_X:
            src = (SDL_FRect){.x = 0.0f, .y = 0.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_I:
            src = (SDL_FRect){.x = 16.0f, .y = 0.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_L:
            src = (SDL_FRect){.x = 32.0f, .y = 0.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_O:
            src = (SDL_FRect){.x = 48.0f, .y = 0.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_Z:
            src = (SDL_FRect){.x = 0.0f, .y = 16.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_T:
            src = (SDL_FRect){.x = 16.0f, .y = 16.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_J:
            src = (SDL_FRect){.x = 32.0f, .y = 16.0f, .w = 16.0f, .h = 16.0f};
            break;
        case BLOCK_S:
            src = (SDL_FRect){.x = 48.0f, .y = 16.0f, .w = 16.0f, .h = 16.0f};
            break;
        default:
            return;
    }

    if (col == 0) voidToLeft = false;
    if (col == 9) voidToRight = false;
    if (row == 19) voidBelow = false;

    float mod = 1.0f;
    float moda = 1.0f;
    Uint8 r, g, b, a;
    switch(lockStatus) {
        case LOCK_LOCKED:
            mod = 0.6f;
            moda = 0.8f;
            break;
        case LOCK_FLASH:
            SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 192);
            SDL_RenderFillRect(renderer, &dest);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            return;
        case LOCK_UNLOCKED:
            mod = 0.5f + (lockParam * 0.5f);
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            break;
        case LOCK_GHOST:
            mod = 0.8f;
            moda = 0.125f;
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 32);
            SDL_RenderFillRect(renderer, &dest);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            break;
        case LOCK_NEXT:
            moda = 0.8f;
            break;
    }

    SDL_SetTextureColorModFloat(block_texture, mod, mod, mod);
    SDL_SetTextureAlphaModFloat(block_texture, moda);
    SDL_RenderTexture(renderer, block_texture, &src, &dest);

    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);

    if(voidToLeft) SDL_RenderLine(renderer, dest.x, dest.y, dest.x, dest.y + dest.h - 1);
    if(voidToRight) SDL_RenderLine(renderer, dest.x + dest.w - 1, dest.y, dest.x + dest.w - 1, dest.y + dest.h - 1);
    if(voidAbove) SDL_RenderLine(renderer, dest.x, dest.y, dest.x + dest.w - 1, dest.y);
    if(voidBelow) SDL_RenderLine(renderer, dest.x, dest.y + dest.h - 1, dest.x + dest.w - 1, dest.y + dest.h - 1);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void render_field_block(const int x, const int y) {
    if(x < 0 || x > 9 || y < 1 || y > 20) return;
    bool voidToLeft = x != 0 && field[x-1][y].type == BLOCK_VOID;
    bool voidToRight = x != 9 && field[x+1][y].type == BLOCK_VOID;
    bool voidAbove = y != 1 && field[x][y-1].type == BLOCK_VOID;
    bool voidBelow = y != 20 && field[x][y+1].type == BLOCK_VOID;
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

    rotation_state_t rotation = get_rotation_state(current_piece.type, current_piece.rotation_state);
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

void render_game() {
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // Background.
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.5f);
    SDL_FRect dst = {.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET, .w = 16.0f * 10.0f, .h = 16.0f * 20.0f};
    SDL_RenderFillRect(renderer, &dst);

    // Field border.
    if (game_state != STATE_PAUSED && game_state != STATE_BEGIN) SDL_SetRenderDrawColorFloat(renderer, 0.9f, 0.1f, 0.1f, 0.8f);
    else SDL_SetRenderDrawColorFloat(renderer, border_r, border_g, border_b, 0.8f);
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
    SDL_SetRenderScale(renderer, (float)render_scale/2.0f, (float)render_scale/2.0f);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1.0f);
    SDL_RenderDebugTextFormat(renderer, 30.0f, (FIELD_Y_OFFSET + (20.0f * 16.0f) + 3) * 2 , "[LVL:%04d] [GRV:%06.3f] [DAS:%02d] [LCK:%02d]", level, (float)current_timing->g/256.0f, current_timing->das, current_timing->lock);
    SDL_SetRenderScale(renderer, (float)render_scale, (float)render_scale);

    SDL_RenderPresent(renderer);
}

float lerp(const float a, const float b, const float f) {
    return (float)(a * (1.0 - f) + (b * f));
}

bool state_machine_tick() {
    // Get input.
    update_input_states();

    // Quit?
    if (IS_JUST_HELD(button_quit_held)) return false;

    // Mute?
    if (IS_JUST_HELD(button_mute_held)) muted = !muted;

    // Huh?
    if (IS_JUST_HELD(button_mystery_held)) mystery = !mystery;

    // Reset?
    if (IS_JUST_HELD(button_reset_held)) {
        generate_first_piece();
        memset(field, 0, sizeof field);
        game_state = STATE_BEGIN;
        game_state_ctr = 60;
        current_timing = &game_timing[0];
        accumulated_g = 0;
        level = 0;
        lines_cleared = 0;
        border_r = 0.1f;
        border_g = 0.1f;
        border_b = 0.9f;
        play_sound(&ready_sound);
        return true;
    }

    // Rescale?
    if (IS_JUST_HELD(button_scale_down_held) && render_scale != 1) {
        render_scale--;
        apply_scale();
    } else if (IS_JUST_HELD(button_scale_up_held)) {
        render_scale++;
        apply_scale();
    }

    // Do all the logic.
    if (game_state == STATE_BEGIN) {
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
            increment_level(lines_cleared);
            generate_next_piece();
            if (piece_collides(current_piece) != -1) {
                write_piece(current_piece);
                current_piece.type = BLOCK_VOID;
                game_state = STATE_GAMEOVER;
                game_state_ctr = 10 * 21 + 1;
            } else {
                game_state = STATE_ACTIVE;
                game_state_ctr = -1;
            }
        }
    } else if (game_state == STATE_ACTIVE) {
        game_state_ctr++;
        const bool was_grounded = piece_grounded();

        // Rotate.
        if ((IS_JUST_HELD(button_a_held) || IS_JUST_HELD(button_c_held)) && !IS_JUST_HELD(button_b_held)) try_rotate(1);
        if ((!IS_JUST_HELD(button_a_held) && !IS_JUST_HELD(button_c_held)) && IS_JUST_HELD(button_b_held)) try_rotate(-1);

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

        const int start_y = current_piece.y;
        if (accumulated_g >= 256) {
            while (accumulated_g >= 256) {
                try_descend();
                accumulated_g -= 256;
            }
            if (start_y != current_piece.y) {
                current_piece.lock_param = 1.0f;
                current_piece.lock_delay = current_timing->lock;
            }
        }

        // Lock?
        if (piece_grounded()) {
            if (!was_grounded || start_y != current_piece.y) play_sound(&pieceland_sound);
            current_piece.lock_delay--;
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
        if (game_state_ctr == -1) return true;
        gray_line(game_state_ctr / 10);
    } else if (game_state == STATE_PAUSED) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
            game_state = game_state_old;
            game_state_ctr = game_state_ctr_old;
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    seed_rng();
    generate_first_piece();
    current_timing = &game_timing[0];

    // Create the window.
    SDL_SetAppMetadata("Tinytris", "1.0", "org.villadelfia.tinytris");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_CreateWindowAndRenderer("Tinytris", 12*16*render_scale, 26*16*render_scale, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS, &window, &renderer);
    SDL_SetRenderScale(renderer, 1.0f * (float)render_scale, 1.0f * (float)render_scale);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Make the window entirely draggable.
    SDL_SetWindowHitTest(window, window_hit_test, NULL);

    // Load the image for a block.
    SDL_IOStream *t = SDL_IOFromMem(block, sizeof block);
    block_texture = IMG_LoadTexture_IO(renderer, t, true);
    SDL_SetTextureScaleMode(block_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(block_texture, SDL_BLENDMODE_BLEND);

    // Make audio device.
    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    // Load the sounds.
    SDL_AudioSpec spec;
    t = SDL_IOFromMem(lineclear, sizeof lineclear);
    SDL_LoadWAV_IO(t, true, &spec, &lineclear_sound.wave_data, &lineclear_sound.wave_data_len);
    lineclear_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, lineclear_sound.stream);

    t = SDL_IOFromMem(linecollapse, sizeof linecollapse);
    SDL_LoadWAV_IO(t, true, &spec, &linecollapse_sound.wave_data, &linecollapse_sound.wave_data_len);
    linecollapse_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, linecollapse_sound.stream);

    t = SDL_IOFromMem(pieceland, sizeof pieceland);
    SDL_LoadWAV_IO(t, true, &spec, &pieceland_sound.wave_data, &pieceland_sound.wave_data_len);
    pieceland_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, pieceland_sound.stream);

    t = SDL_IOFromMem(piecelock, sizeof piecelock);
    SDL_LoadWAV_IO(t, true, &spec, &piecelock_sound.wave_data, &piecelock_sound.wave_data_len);
    piecelock_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, piecelock_sound.stream);

    t = SDL_IOFromMem(irs, sizeof irs);
    SDL_LoadWAV_IO(t, true, &spec, &irs_sound.wave_data, &irs_sound.wave_data_len);
    irs_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, irs_sound.stream);

    t = SDL_IOFromMem(ready, sizeof ready);
    SDL_LoadWAV_IO(t, true, &spec, &ready_sound.wave_data, &ready_sound.wave_data_len);
    ready_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, ready_sound.stream);

    t = SDL_IOFromMem(go, sizeof go);
    SDL_LoadWAV_IO(t, true, &spec, &go_sound.wave_data, &go_sound.wave_data_len);
    go_sound.stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_device, go_sound.stream);

    play_sound(&ready_sound);

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
    } else if (game_state != STATE_PAUSED) {
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
        accumulated_time += (now - last_time) - FRAME_TIME;
        last_time = now;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}