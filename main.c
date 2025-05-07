#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_timer.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *blockTexture = NULL;
static Uint64 lastTime = 0;

const float FIELD_X_OFFSET = 16.0f;
const float FIELD_Y_OFFSET = 16.0f * 4.0f;

SDL_HitTestResult HitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    return SDL_HITTEST_DRAGGABLE;
}

typedef enum {BLOCK_VOID, BLOCK_X, BLOCK_I, BLOCK_L, BLOCK_O, BLOCK_Z, BLOCK_T, BLOCK_J, BLOCK_S} block_type_t;
typedef enum {LOCK_LOCKED, LOCK_FLASH, LOCK_UNLOCKED, LOCK_GHOST} lock_status_t;
typedef struct {
    block_type_t type;
    lock_status_t lockStatus;
    float lockParam;
} block_t;

block_t field[10][20] = {0};

void renderBlockImpl(int col, int row, block_type_t block, lock_status_t lockStatus, float lockParam, bool voidToLeft, bool voidToRight, bool voidAbove, bool voidBelow) {
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

void renderBlock(int x, int y) {
    if(x < 0 || x > 9 || y < 0 || y > 19) return;
    bool voidToLeft = x != 0 && field[x-1][y].type == BLOCK_VOID;
    bool voidToRight = x != 9 && field[x+1][y].type == BLOCK_VOID;
    bool voidAbove = y != 0 && field[x][y-1].type == BLOCK_VOID;
    bool voidBelow = y != 19 && field[x][y+1].type == BLOCK_VOID;
    renderBlockImpl(x, y, field[x][y].type, field[x][y].lockStatus, field[x][y].lockParam, voidToLeft, voidToRight, voidAbove, voidBelow);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    // Create the window.
    SDL_SetAppMetadata("Tinytris", "1.0", "org.villadelfia.tinytris");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Tinytris", 12*16, 26*16, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS, &window, &renderer);

    // Make the window entirely draggable.
    SDL_SetWindowHitTest(window, HitTest, NULL);

    // Load the image for a block.
    blockTexture = IMG_LoadTexture(renderer, "block.bmp");
    SDL_Log("%s", SDL_GetError());

    // Test data.
    field[3][15] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[4][15] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};
    field[5][15] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};

    field[1][16] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[2][16] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[3][16] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[4][16] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[5][16] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};

    field[0][17] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[1][17] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[2][17] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};
    field[3][17] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[4][17] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[5][17] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};

    field[0][18] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};
    field[1][18] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};
    field[2][18] = (block_t){.type = BLOCK_L, .lockStatus = LOCK_LOCKED};
    field[3][18] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[4][18] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[6][18] = (block_t){.type = BLOCK_J, .lockStatus = LOCK_LOCKED};
    field[7][18] = (block_t){.type = BLOCK_J, .lockStatus = LOCK_LOCKED};
    field[8][18] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};

    field[0][19] = (block_t){.type = BLOCK_J, .lockStatus = LOCK_LOCKED};
    field[1][19] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[2][19] = (block_t){.type = BLOCK_O, .lockStatus = LOCK_LOCKED};
    field[3][19] = (block_t){.type = BLOCK_J, .lockStatus = LOCK_LOCKED};
    field[4][19] = (block_t){.type = BLOCK_S, .lockStatus = LOCK_LOCKED};
    field[5][19] = (block_t){.type = BLOCK_S, .lockStatus = LOCK_LOCKED};
    field[6][19] = (block_t){.type = BLOCK_Z, .lockStatus = LOCK_LOCKED};
    field[7][19] = (block_t){.type = BLOCK_Z, .lockStatus = LOCK_LOCKED};
    field[8][19] = (block_t){.type = BLOCK_T, .lockStatus = LOCK_LOCKED};

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

        // Do all the logic.
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

    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 20; y++) {
            renderBlock(x, y);
        }
    }

    renderBlockImpl(3, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
    renderBlockImpl(4, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
    renderBlockImpl(5, -3, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);
    renderBlockImpl(4, -2, BLOCK_T, LOCK_UNLOCKED, 1.0f, false, false, false, false);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}