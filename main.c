#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>
#include <stdint.h>
#include <time.h>

// Offset of the field from the top left of the window.
const float FIELD_X_OFFSET = 16.0f;
const float FIELD_Y_OFFSET = 16.0f * 5.0f;

// Render scale.
const int RENDER_SCALE = 2;

// Buttons.
const int BUTTON_U = SDL_SCANCODE_W;
const int BUTTON_L = SDL_SCANCODE_A;
const int BUTTON_D = SDL_SCANCODE_S;
const int BUTTON_R = SDL_SCANCODE_D;
const int BUTTON_A = SDL_SCANCODE_J;
const int BUTTON_B = SDL_SCANCODE_K;
const int BUTTON_C = SDL_SCANCODE_L;
const int BUTTON_RESET = SDL_SCANCODE_R;
const int BUTTON_QUIT = SDL_SCANCODE_ESCAPE;
int button_u_held = 0;
int button_l_held = 0;
int button_d_held = 0;
int button_r_held = 0;
int button_a_held = 0;
int button_b_held = 0;
int button_c_held = 0;
int button_reset_held = 0;
int button_quit_held = 0;

// Renderer and target.
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *blockTexture = NULL;
static Uint64 last_time = 0;
static Uint64 frame_ctr = 0;

// Types for game state.
typedef enum {BLOCK_VOID, BLOCK_X, BLOCK_I, BLOCK_L, BLOCK_O, BLOCK_Z, BLOCK_T, BLOCK_J, BLOCK_S} block_type_t;
typedef enum {LOCK_LOCKED, LOCK_FLASH, LOCK_UNLOCKED, LOCK_GHOST} lock_status_t;
typedef struct {
    block_type_t type;
    lock_status_t lock_status;
    float lock_param;
} block_t;
typedef struct {
    block_type_t type;
    uint8_t rotation_state;
    uint8_t x;
    int8_t y;
    lock_status_t lock_status;
    float lock_param;
} live_block_t;
typedef char* rotation_state_t;
typedef rotation_state_t piece_def_t[4];
typedef piece_def_t piece_defs_t[9];
typedef enum {STATE_ARE, STATE_ACTIVE, STATE_LOCKFLASH} game_state_t;
typedef struct {
    int level;
    int g;
    int are;
    int line_are;
    int das;
    int lock;
    int clear;
} timing_t;

piece_defs_t ROTATION_STATES = {
    // VOID and X
    {"", "", "", ""},
    {"", "", "", ""},

    // BLOCK_I
    {
        "    "
        "IIII"
        "    "
        "    ",

        "  I "
        "  I "
        "  I "
        "  I ",

        "    "
        "IIII"
        "    "
        "    ",

        "  I "
        "  I "
        "  I "
        "  I ",
    },

    // BLOCK_L
    {
        "    "
        "LLL "
        "L   "
        "    ",

        " L  "
        " L  "
        " LL "
        "    ",

        "    "
        "  L "
        "LLL "
        "    ",

        "LL  "
        " L  "
        " L  "
        "    ",
    },

    // BLOCK_O
    {
        "    "
        " OO "
        " OO "
        "    ",

        "    "
        " OO "
        " OO "
        "    ",

        "    "
        " OO "
        " OO "
        "    ",

        "    "
        " OO "
        " OO "
        "    ",
    },

    // BLOCK_Z
    {
        "    "
        "ZZ  "
        " ZZ "
        "    ",

        "  Z "
        " ZZ "
        " Z  "
        "    ",

        "    "
        "ZZ  "
        " ZZ "
        "    ",

        "  Z "
        " ZZ "
        " Z  "
        "    "
    },

    // BLOCK_T
    {
        "    "
        "TTT "
        " T  "
        "    ",

        " T  "
        " TT "
        " T  "
        "    ",

        "    "
        " T  "
        "TTT "
        "    ",

        " T  "
        "TT  "
        " T  "
        "    "
    },

    // BLOCK_J
    {
        "    "
        "JJJ "
        "  J "
        "    ",

        " JJ "
        " J  "
        " J  "
        "    ",

        "    "
        "J   "
        "JJJ "
        "    ",

        " J  "
        " J  "
        "JJ  "
        "    "
    },


    // BLOCK_S
    {
        "    "
        " SS "
        "SS  "
        "    ",

        "S   "
        "SS  "
        " S  "
        "    ",

        "    "
        " SS "
        "SS  "
        "    ",

        "S   "
        "SS  "
        " S  "
        "    "
    }
};

timing_t game_timing[] = {
    {0,    4,    27, 27, 16, 30, 40},
    {30,   6,    27, 27, 16, 30, 40},
    {35,   8,    27, 27, 16, 30, 40},
    {40,   10,   27, 27, 16, 30, 40},
    {50,   12,   27, 27, 16, 30, 40},
    {60,   16,   27, 27, 16, 30, 40},
    {70,   32,   27, 27, 16, 30, 40},
    {80,   48,   27, 27, 16, 30, 40},
    {90,   64,   27, 27, 16, 30, 40},
    {100,  80,   27, 27, 16, 30, 40},
    {120,  96,   27, 27, 16, 30, 40},
    {140,  112,  27, 27, 16, 30, 40},
    {160,  128,  27, 27, 16, 30, 40},
    {170,  144,  27, 27, 16, 30, 40},
    {200,  4,    27, 27, 16, 30, 40},
    {220,  32,   27, 27, 16, 30, 40},
    {230,  64,   27, 27, 16, 30, 40},
    {233,  96,   27, 27, 16, 30, 40},
    {236,  128,  27, 27, 16, 30, 40},
    {239,  160,  27, 27, 16, 30, 40},
    {243,  192,  27, 27, 16, 30, 40},
    {247,  224,  27, 27, 16, 30, 40},
    {251,  256,  27, 27, 16, 30, 40},
    {300,  512,  27, 27, 16, 30, 40},
    {330,  768,  27, 27, 16, 30, 40},
    {360,  1024, 27, 27, 16, 30, 40},
    {400,  1280, 27, 27, 16, 30, 40},
    {420,  1024, 27, 27, 16, 30, 40},
    {450,  768,  27, 27, 16, 30, 40},
    {500,  5120, 27, 27, 10, 30, 25},
    {601,  5120, 27, 18, 10, 30, 16},
    {701,  5120, 18, 14, 10, 30, 12},
    {801,  5120, 14,  8, 10, 30,  6},
    {901,  5120, 14,  8,  8, 17,  6},
    {1001, 5120,  8,  8,  8, 16,  6},
    {1201, 5120,  7,  7,  8, 15,  5},
    {1501, 5120,  6,  6,  8, 15,  4},
    {2001, 5120,  5,  5,  7, 12,  3},
    {2501, 5120,  5,  5,  6, 10,  3},
    {3001, 5120,  5,  5,  6,  8,  3},
    {-1,   0,     0,  0,  0,  0,  0}
};

rotation_state_t get_rotation_state(block_type_t piece, int rotation_state) {
    return ROTATION_STATES[piece][rotation_state];
}

// The current field of locked pieces.
block_t field[10][21] = {0};
block_type_t history[4] = {0};
block_type_t next_piece;
live_block_t current_piece;
game_state_t game_state = STATE_ARE;
int game_state_ctr = 120;

void update_inputs() {
    const bool* state = SDL_GetKeyboardState(NULL);
    if (state[BUTTON_U]) button_u_held++;
    else button_u_held = 0;
    if (state[BUTTON_D]) button_d_held++;
    else button_d_held = 0;
    if (state[BUTTON_L]) button_l_held++;
    else button_l_held = 0;
    if (state[BUTTON_R]) button_r_held++;
    else button_r_held = 0;
    if (state[BUTTON_A]) button_a_held++;
    else button_a_held = 0;
    if (state[BUTTON_B]) button_b_held++;
    else button_b_held = 0;
    if (state[BUTTON_C]) button_c_held++;
    else button_c_held = 0;
    if (state[BUTTON_RESET]) button_reset_held++;
    else button_reset_held = 0;
    if (state[BUTTON_QUIT]) button_quit_held++;
    else button_quit_held = 0;
}

// SDL hit-test that marks the entire window as draggable.
SDL_HitTestResult HitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    return SDL_HITTEST_DRAGGABLE;
}

// Splitmix64 generator for seeding.
uint64_t splitmix_state;
static uint64_t xoroshiro_state[4];
uint64_t splitmix_next() {
    splitmix_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = splitmix_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t xoroshiro_next() {
    const uint64_t result = rotl(xoroshiro_state[1] * 5, 7) * 9;
    const uint64_t t = xoroshiro_state[1] << 17;
    xoroshiro_state[2] ^= xoroshiro_state[0];
    xoroshiro_state[3] ^= xoroshiro_state[1];
    xoroshiro_state[1] ^= xoroshiro_state[2];
    xoroshiro_state[0] ^= xoroshiro_state[3];
    xoroshiro_state[2] ^= t;
    xoroshiro_state[3] = rotl(xoroshiro_state[3], 45);
    return result;
}

void seed_rng() {
    splitmix_state = time(NULL);
    xoroshiro_state[0] = splitmix_next();
    xoroshiro_state[1] = splitmix_next();
    xoroshiro_state[2] = splitmix_next();
    xoroshiro_state[3] = splitmix_next();
}

int piece_collides(live_block_t piece) {
    if (piece.type == BLOCK_VOID) return -1;

    rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (rotation[4*j + i] != ' ') {
                if (field[piece.x + i][piece.y + j + 1].type != BLOCK_VOID || piece.y + j + 1 > 20) {
                    return 4*j+i;
                }
            }
        }
    }

    return -1;
}

void write_piece(live_block_t piece) {
    if (piece.type == BLOCK_VOID) return;

    rotation_state_t rotation = get_rotation_state(piece.type, piece.rotation_state);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (rotation[4*j + i] != ' ') {
                field[piece.x + i][piece.y + j + 1].type = piece.type;
                field[piece.x + i][piece.y + j + 1].lock_param = 1.0f;
                field[piece.x + i][piece.y + j + 1].lock_status = LOCK_LOCKED;
            }
        }
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

    // Apply IRS.
    if ((button_a_held > 0 || button_c_held > 0) && button_b_held == 0) current_piece.rotation_state = 1;
    if ((button_a_held == 0 && button_c_held == 0) && button_b_held > 0) current_piece.rotation_state = 3;
    if (current_piece.rotation_state != 0 && piece_collides(current_piece) != -1) current_piece.rotation_state = 0;
}

void render_raw_block(int col, int row, block_type_t block, lock_status_t lockStatus, float lockParam, bool voidToLeft, bool voidToRight, bool voidAbove, bool voidBelow) {
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
    switch(lockStatus) {
        case LOCK_LOCKED:
            mod = 0.6f;
            break;
        case LOCK_FLASH:
            Uint8 r, g, b, a;
            SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &dest);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            return;
        case LOCK_UNLOCKED:
            mod = 0.2f + (lockParam * 0.8f);
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            break;
        case LOCK_GHOST:
            mod = 0.3f;
            moda = 0.0f;
            voidToLeft = voidToRight = voidAbove = voidBelow = false;
            break;
    }

    SDL_SetTextureColorModFloat(blockTexture, mod, mod, mod);
    SDL_SetTextureAlphaModFloat(blockTexture, moda);
    SDL_RenderTexture(renderer, blockTexture, &src, &dest);

    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    if(voidToLeft) SDL_RenderLine(renderer, dest.x, dest.y, dest.x, dest.y + dest.h - 1);
    if(voidToRight) SDL_RenderLine(renderer, dest.x + dest.w - 1, dest.y, dest.x + dest.w - 1, dest.y + dest.h - 1);
    if(voidAbove) SDL_RenderLine(renderer, dest.x, dest.y, dest.x + dest.w - 1, dest.y);
    if(voidBelow) SDL_RenderLine(renderer, dest.x, dest.y + dest.h - 1, dest.x + dest.w - 1, dest.y + dest.h - 1);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void render_field_block(int x, int y) {
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
            render_raw_block(3, -4, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(6, -4, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_L:
            render_raw_block(3, -4, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(3, -3, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_O:
            render_raw_block(4, -4, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_Z:
            render_raw_block(3, -4, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_T:
            render_raw_block(3, -4, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_J:
            render_raw_block(3, -4, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -4, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_S:
            render_raw_block(4, -4, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -4, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(3, -3, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
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
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.3f);
    SDL_FRect dst = {.x = FIELD_X_OFFSET, .y = FIELD_Y_OFFSET, .w = 16.0f * 10.0f, .h = 16.0f * 20.0f};
    SDL_RenderFillRect(renderer, &dst);

    // Field border.
    SDL_SetRenderDrawColorFloat(renderer, 0.9f, 0.1f, 0.1f, 0.4f);
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
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.2f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 6, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 12, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.3f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 4, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 8, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.4f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 2, .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 4, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.5f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f), .y = FIELD_Y_OFFSET - (4.0f * 16.0f) - 4, .w = 6.0f * 16.0f, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);

    // Render the game.
    render_field();
    render_next_block();
    render_tls();
    render_current_block();

    SDL_RenderPresent(renderer);
}

bool state_machine_tick() {
    // Get input.
    update_inputs();

    // Quit?
    if (button_quit_held != 0) return false;

    if (button_reset_held == 1) {
        generate_first_piece();
        memset(field, 0, sizeof field);
        game_state = STATE_ARE;
        game_state_ctr = 120;
        return true;
    }

    // Do all the logic.
    if (game_state == STATE_ARE) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
            generate_next_piece();
            game_state = STATE_ACTIVE;
        }
    } else if (game_state == STATE_ACTIVE) {
        // Rotate.
        // Move.
        // Gravity.
        if (frame_ctr % 30 == 0) {
            current_piece.y++;
            if (piece_collides(current_piece) != -1) {
                current_piece.y--;
                current_piece.lock_status = LOCK_FLASH;
                game_state = STATE_LOCKFLASH;
                game_state_ctr = 3;
            }
        }
    } else if (game_state == STATE_LOCKFLASH) {
        game_state_ctr--;
        if (game_state_ctr == 0) {
            game_state = STATE_ARE;
            game_state_ctr = 8;
            write_piece(current_piece);
            current_piece.type = BLOCK_VOID;
        }
    }

    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    seed_rng();
    generate_first_piece();

    // Create the window.
    SDL_SetAppMetadata("Tinytris", "1.0", "org.villadelfia.tinytris");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Tinytris", 12*16*RENDER_SCALE, 26*16*RENDER_SCALE, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS, &window, &renderer);
    SDL_SetRenderScale(renderer, 1.0f * (float)RENDER_SCALE, 1.0f * (float)RENDER_SCALE);

    // Make the window entirely draggable.
    SDL_SetWindowHitTest(window, HitTest, NULL);

    // Load the image for a block.
    blockTexture = IMG_LoadTexture(renderer, "block.bmp");
    SDL_SetTextureScaleMode(blockTexture, SDL_SCALEMODE_NEAREST);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    // Handle 1/frame update.
    const Uint64 now = SDL_GetTicksNS();
    if(now >= last_time + (SDL_NS_PER_SECOND / 60LL) && SDL_GetKeyboardFocus() == window) {
        last_time = now;
        frame_ctr++;
        if (!state_machine_tick()) return SDL_APP_SUCCESS;
    }

    // Render the game.
    render_game();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}