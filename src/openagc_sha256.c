/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc_sha256.h"

#include <string.h>

static const uint32_t openagc_sha256_rounds[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t openagc_sha256_rotate(uint32_t value, unsigned int bits)
{
    return (value >> bits) | (value << (32u - bits));
}

static void openagc_sha256_block(uint32_t state[8], const uint8_t bytes[64])
{
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t first;
    uint32_t second;
    unsigned int i;

    for (i = 0u; i < 16u; ++i) {
        words[i] = ((uint32_t)bytes[4u * i] << 24) |
                   ((uint32_t)bytes[4u * i + 1u] << 16) |
                   ((uint32_t)bytes[4u * i + 2u] << 8) |
                   (uint32_t)bytes[4u * i + 3u];
    }
    for (i = 16u; i < 64u; ++i) {
        uint32_t small0 = openagc_sha256_rotate(words[i - 15u], 7u) ^
                          openagc_sha256_rotate(words[i - 15u], 18u) ^
                          (words[i - 15u] >> 3);
        uint32_t small1 = openagc_sha256_rotate(words[i - 2u], 17u) ^
                          openagc_sha256_rotate(words[i - 2u], 19u) ^
                          (words[i - 2u] >> 10);
        words[i] = words[i - 16u] + small0 + words[i - 7u] + small1;
    }
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];
    for (i = 0u; i < 64u; ++i) {
        first = h +
                (openagc_sha256_rotate(e, 6u) ^
                 openagc_sha256_rotate(e, 11u) ^
                 openagc_sha256_rotate(e, 25u)) +
                ((e & f) ^ (~e & g)) + openagc_sha256_rounds[i] + words[i];
        second = (openagc_sha256_rotate(a, 2u) ^
                  openagc_sha256_rotate(a, 13u) ^
                  openagc_sha256_rotate(a, 22u)) +
                 ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void openagc_sha256(const uint8_t *bytes, size_t length, uint8_t digest[32])
{
    uint32_t state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    uint8_t tail[128] = { 0 };
    uint64_t bit_length = (uint64_t)length * 8u;
    size_t block_count = length / 64u;
    size_t remaining = length % 64u;
    size_t tail_length = remaining < 56u ? 64u : 128u;
    size_t i;

    for (i = 0u; i < block_count; ++i) {
        openagc_sha256_block(state, bytes + 64u * i);
    }
    if (remaining != 0u) {
        memcpy(tail, bytes + 64u * block_count, remaining);
    }
    tail[remaining] = 0x80u;
    for (i = 0u; i < 8u; ++i) {
        tail[tail_length - 1u - i] = (uint8_t)(bit_length >> (8u * i));
    }
    openagc_sha256_block(state, tail);
    if (tail_length == 128u) {
        openagc_sha256_block(state, tail + 64u);
    }
    for (i = 0u; i < 8u; ++i) {
        digest[4u * i] = (uint8_t)(state[i] >> 24);
        digest[4u * i + 1u] = (uint8_t)(state[i] >> 16);
        digest[4u * i + 2u] = (uint8_t)(state[i] >> 8);
        digest[4u * i + 3u] = (uint8_t)state[i];
    }
}
