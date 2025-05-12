#pragma once

#include <stdint.h>
#include <time.h>

static uint64_t splitmix_state;
static uint64_t xoroshiro_state[4];
static uint64_t splitmix_next() {
    splitmix_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = splitmix_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t rotl(const uint64_t x, const int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t xoroshiro_next() {
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

static void seed_rng() {
    splitmix_state = time(NULL);
    xoroshiro_state[0] = splitmix_next();
    xoroshiro_state[1] = splitmix_next();
    xoroshiro_state[2] = splitmix_next();
    xoroshiro_state[3] = splitmix_next();
}