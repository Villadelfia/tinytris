#pragma once

#define IS_HELD(input_data) (input_data > 0)
#define IS_RELEASED(input_data) (input_data <= 0)
#define FRAMES_HELD(input_data) (input_data - 1)
#define FRAMES_RELEASED(input_data) (-(input_data + 1))
#define IS_HELD_FOR_EXACTLY(input_data, frames) (IS_HELD(input_data) && FRAMES_HELD(input_data) == frames)
#define IS_RELEASED_FOR_EXACTLY(input_data, frames) (IS_RELEASED(input_data) && FRAMES_RELEASED(input_data) == frames)
#define IS_JUST_HELD(input_data) (IS_HELD_FOR_EXACTLY(input_data, 0))
#define IS_JUST_RELEASED(input_data) (IS_RELEASED_FOR_EXACTLY(input_data, 0))
#define IS_HELD_FOR_AT_LEAST(input_data, frames) (IS_HELD(input_data) && FRAMES_HELD(input_data) >= frames)
#define IS_RELEASED_FOR_AT_LEAST(input_data, frames) (IS_RELEASED(input_data) && FRAMES_RELEASED(input_data) >= frames)
#define IS_HELD_FOR_MORE_THAN(input_data, frames) (IS_HELD(input_data) && FRAMES_HELD(input_data) > frames)
#define IS_RELEASED_FOR_MORE_THAN(input_data, frames) (IS_RELEASED(input_data) && FRAMES_RELEASED(input_data) > frames)
#define IS_HELD_FOR_AT_MOST(input_data, frames) (IS_HELD(input_data) && FRAMES_HELD(input_data) <= frames)
#define IS_RELEASED_FOR_AT_MOST(input_data, frames) (IS_RELEASED(input_data) && FRAMES_RELEASED(input_data) <= frames)
#define IS_HELD_FOR_LESS_THAN(input_data, frames) (IS_HELD(input_data) && FRAMES_HELD(input_data) < frames)
#define IS_RELEASED_FOR_LESS_THAN(input_data, frames) (IS_RELEASED(input_data) && FRAMES_RELEASED(input_data) < frames)