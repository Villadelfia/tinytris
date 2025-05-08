#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>
#include <stdint.h>
#include <time.h>

// Offset of the field from the top left of the window.
const float FIELD_X_OFFSET = 16.0f;
const float FIELD_Y_OFFSET = 16.0f * 4.0f;

// Render scale.
const int RENDER_SCALE = 2;

// Renderer and target.
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *blockTexture = NULL;
static Uint64 lastTime = 0;
static Uint64 frameCtr = 0;

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
    uint8_t y;
    lock_status_t lock_status;
    float lock_param;
} live_block_t;

// The current field of locked pieces.
block_t field[10][21] = {0};
block_type_t history[4] = {0};
block_type_t next_piece;
live_block_t current_piece;

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
    current_piece.y = 0;
    current_piece.lock_param = 1.0f;
    current_piece.lock_status = LOCK_UNLOCKED;
    current_piece.rotation_state = 0;

    // TODO: Apply 20G if needed.
    // TODO: Apply IRS.
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
            mod = 0.8f;
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
            render_raw_block(3, -3, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(6, -3, BLOCK_I, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_L:
            render_raw_block(3, -3, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(3, -2, BLOCK_L, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_O:
            render_raw_block(4, -3, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -2, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -2, BLOCK_O, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_Z:
            render_raw_block(3, -3, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -2, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -2, BLOCK_Z, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_T:
            render_raw_block(3, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -2, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_J:
            render_raw_block(3, -3, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -3, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -2, BLOCK_J, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        case BLOCK_S:
            render_raw_block(4, -3, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(5, -3, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(3, -2, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            render_raw_block(4, -2, BLOCK_S, LOCK_UNLOCKED, 1.0f, false, false, false, false);
            break;
        default:
            return;
    }
}

void render_current_block() {
    if(current_piece.type == BLOCK_VOID) return;
    // TODO.
}

void render_field() {
    for (int x = 0; x < 10; x++) {
        for (int y = 1; y < 21; y++) {
            render_field_block(x, y);
        }
    }
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

    // Test data.
    field[3][16] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[4][16] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};
    field[5][16] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};

    field[1][17] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[2][17] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[3][17] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[4][17] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[5][17] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};

    field[0][18] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[1][18] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[2][18] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};
    field[3][18] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[4][18] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[5][18] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};

    field[0][19] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};
    field[1][19] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};
    field[2][19] = (block_t){.type = BLOCK_L, .lock_status = LOCK_LOCKED};
    field[3][19] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[4][19] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[6][19] = (block_t){.type = BLOCK_J, .lock_status = LOCK_LOCKED};
    field[7][19] = (block_t){.type = BLOCK_J, .lock_status = LOCK_LOCKED};
    field[8][19] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};

    field[0][20] = (block_t){.type = BLOCK_J, .lock_status = LOCK_LOCKED};
    field[1][20] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[2][20] = (block_t){.type = BLOCK_O, .lock_status = LOCK_LOCKED};
    field[3][20] = (block_t){.type = BLOCK_J, .lock_status = LOCK_LOCKED};
    field[4][20] = (block_t){.type = BLOCK_S, .lock_status = LOCK_LOCKED};
    field[5][20] = (block_t){.type = BLOCK_S, .lock_status = LOCK_LOCKED};
    field[6][20] = (block_t){.type = BLOCK_Z, .lock_status = LOCK_LOCKED};
    field[7][20] = (block_t){.type = BLOCK_Z, .lock_status = LOCK_LOCKED};
    field[8][20] = (block_t){.type = BLOCK_T, .lock_status = LOCK_LOCKED};

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch(event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    Uint64 now = SDL_GetTicksNS();
    if(now >= lastTime + (SDL_NS_PER_SECOND / 60LL) && SDL_GetKeyboardFocus() == window) {
        lastTime = now;
        frameCtr++;

        // Do all the logic.
        if (frameCtr % 30 == 0) generate_next_piece();
    }

    // Render the game.
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
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 8, .y = FIELD_Y_OFFSET - (3.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 16, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.2f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 6, .y = FIELD_Y_OFFSET - (3.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 12, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.3f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 4, .y = FIELD_Y_OFFSET - (3.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 8, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.4f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f) - 2, .y = FIELD_Y_OFFSET - (3.0f * 16.0f) - 4, .w = 6.0f * 16.0f + 4, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0.5f);
    dst = (SDL_FRect){.x = FIELD_X_OFFSET + (2.0f * 16.0f), .y = FIELD_Y_OFFSET - (3.0f * 16.0f) - 4, .w = 6.0f * 16.0f, .h = 2.0f * 16.0f + 8};
    SDL_RenderFillRect(renderer, &dst);

    // Render the game.
    render_field();
    render_next_block();
    render_current_block();

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}