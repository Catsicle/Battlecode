#pragma once
#include <cstdint>

#define ROTL(x,b) ((x << b) | (x >> (64 - b)))

inline uint64_t siphash24(uint64_t m, uint64_t k0, uint64_t k1)
{
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;

    auto SIPROUND = [&]() {
        v0 += v1;
        v1 = ROTL(v1,13);
        v1 ^= v0;
        v0 = ROTL(v0,32);

        v2 += v3;
        v3 = ROTL(v3,16);
        v3 ^= v2;

        v0 += v3;
        v3 = ROTL(v3,21);
        v3 ^= v0;

        v2 += v1;
        v1 = ROTL(v1,17);
        v1 ^= v2;
        v2 = ROTL(v2,32);
    };

    // One 8-byte message block
    v3 ^= m;
    SIPROUND();
    SIPROUND();
    v0 ^= m;

    // Final block: message length = 8 bytes
    uint64_t b = 8ULL << 56;

    v3 ^= b;
    SIPROUND();
    SIPROUND();
    v0 ^= b;

    // Finalization
    v2 ^= 0xff;
    SIPROUND();
    SIPROUND();
    SIPROUND();
    SIPROUND();

    return v0 ^ v1 ^ v2 ^ v3;
}