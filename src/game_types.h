#pragma once

#include <stdbool.h>

typedef enum {
    BLOCK_VOID,
    BLOCK_X,
    BLOCK_I,
    BLOCK_L,
    BLOCK_O,
    BLOCK_Z,
    BLOCK_T,
    BLOCK_J,
    BLOCK_S
} block_type_t;

typedef enum {
    LOCK_LOCKED,
    LOCK_FLASH,
    LOCK_UNLOCKED,
    LOCK_GHOST,
    LOCK_NEXT
} lock_status_t;

typedef struct {
    block_type_t type;
    lock_status_t lock_status;
    float lock_param;
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
    STATE_WAIT
} game_state_t;

typedef struct {
    int level;
    int g;
    int are;
    int line_are;
    int das;
    int lock;
    int clear;
} timing_t;