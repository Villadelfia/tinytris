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
    BLOCK_T,
    BLOCK_BONE,
    BLOCK_HARD,
    ITEM_HARD,
    ITEM_SHOTGUN,
    ITEM_LASER,
    ITEM_NEGATE,
    ITEM_DEL_UPPER,
    ITEM_DEL_LOWER,
    ITEM_DEL_EVEN,
    ITEM_PUSH_LEFT,
    ITEM_PUSH_RIGHT,
    ITEM_PUSH_DOWN,
    ITEM_180,
    ITEM_BIG_BLOCK,
    ITEM_ANTIGRAVITY
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
    block_type_t subtype;
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
    int32_t torikan;
    int32_t freeze;

    // Reserved for later expansion.
    int32_t unused_0;
    int32_t unused_1;
    int32_t unused_2;
    int32_t unused_3;
    int32_t unused_4;
    int32_t unused_5;
    int32_t unused_6;
    int32_t unused_7;

    // Effect bitmask.
    int32_t effect;

    // Only used for roll.
    int32_t duration;
} timing_t;

typedef struct {
    char* name;
    int32_t mask;
} effect_def_t;

#define TORIKAN_SCOPE_MASK           (0x00000001)
#define TORIKAN_EFFECT_MASK          (0x00000002)
#define TGM2_PLUS_SEQUENCE_MASK      (0x00000004)
#define TI_GARBAGE_QUOTA             (0x00000008)
#define RESET_VISIBILITY_MASK        (0x00000010)
#define RESET_VISIBILITY_TIMER_MASK  (0x00000020)
#define INVISIBILITY_HINT_ONCE_MASK  (0x00000040)
#define INVISIBILITY_HINT_FLASH_MASK (0x00000080)
#define FROZEN_RESET_MASK            (0x00000100)
#define CLEAR_FIELD_MASK             (0x00000200)
#define BONES_MASK                   (0x00000400)
#define BIG_MODE_MASK                (0x00000800)
#define BIG_PIECE_MASK               (0x00001000)
#define BIG_MODE_HALF_PIECE_MASK     (0x00002000)
#define SELECTIVE_GRAVITY_MASK       (0x00004000)
#define FIELD_SHOTGUN_MASK           (0x00008000)
#define FIELD_LASER_MASK             (0x00010000)
#define FIELD_NEGATE_MASK            (0x00020000)
#define FIELD_PUSH_LEFT_MASK         (0x00040000)
#define FIELD_PUSH_RIGHT_MASK        (0x00080000)
#define FIELD_PUSH_DOWN_MASK         (0x00100000)
#define FIELD_DEL_UPPER_MASK         (0x00200000)
#define FIELD_DEL_LOWER_MASK         (0x00400000)
#define FIELD_DEL_EVEN_MASK          (0x00800000)
#define FIELD_180_MASK               (0x01000000)

#define ITEM_MODE_MASK               (0x40000000)
#define RANDOM_ITEM_MASK             (0x80000000)