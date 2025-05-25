#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BLOCK_VOID,
    BLOCK_X,
    BLOCK_I,
    BLOCK_Z,
    BLOCK_S,
    BLOCK_J,
    BLOCK_L,
    BLOCK_O,
    BLOCK_T
} block_type_t;

typedef enum {
    LOCK_LOCKED,
    LOCK_FLASH,
    LOCK_UNLOCKED,
    LOCK_GHOST,
    LOCK_NEXT,
    LOCK_HOLD
} lock_status_t;

typedef struct {
    block_type_t type;
    lock_status_t lock_status;
    float lock_param;
    int locked_at;
    bool fading;
    float fade_state;
} block_t;

typedef struct {
    block_type_t type;
    int rotation_state;
    int x;
    int y;
    lock_status_t lock_status;
    float lock_param;
    int lock_delay;
    bool floor_kicked;
} live_block_t;

typedef struct {
    int32_t split_frames;
    int32_t lap_frames;
} split_t;

typedef struct {
    int32_t total_frames;
    int32_t current_section;
    int32_t highest_regular_section;
    split_t splits[4096];
} game_details_t;

typedef char* rotation_state_t;

typedef rotation_state_t piece_def_t[4];

typedef piece_def_t piece_defs_t[9];

typedef enum {
    STATE_ARE,
    STATE_ACTIVE,
    STATE_LOCKFLASH,
    STATE_CLEAR,
    STATE_GAMEOVER,
    STATE_PAUSED,
    STATE_BEGIN,
    STATE_WAIT,
    STATE_FADEOUT
} game_state_t;

typedef struct {
    int32_t level;
    int32_t g;
    int32_t are;
    int32_t line_are;
    int32_t das;
    int32_t lock;
    int32_t clear;
    int32_t fade;
    int32_t garbage;
    int32_t effect;

    // Reserved for later expansion.
    int32_t unused_0;
    int32_t unused_1;
    int32_t unused_2;
    int32_t unused_3;
    int32_t unused_4;
    int32_t unused_5;
    int32_t unused_6;
    int32_t unused_7;
    int32_t unused_8;
    int32_t unused_9;

    // Only used for roll.
    int32_t duration;
} timing_t;

#define TORIKAN_VALUE_MASK           (0x0000FFFF)
#define TORIKAN_SCOPE_MASK           (0x00010000)
#define TORIKAN_EFFECT_MASK          (0x00020000)
#define CLEAR_FIELD_MASK             (0x80000000)
#define RESET_VISIBILITY_MASK        (0x40000000)
#define RESET_VISIBILITY_TIMER_MASK  (0x20000000)
#define INVISIBILITY_HINT_ONCE_MASK  (0x10000000)
#define INVISIBILITY_HINT_FLASH_MASK (0x08000000)
#define TGM2_PLUS_SEQUENCE_MASK      (0x04000000)