#include "comm_function.hpp"
#include "siphash.hpp"

#include <cstdint>
#include <random>

namespace {
constexpr std::uint64_t SECRET_K0 = 0xBEEFCAFEDEADBABEULL;
constexpr std::uint64_t SECRET_K1 = 0xCAFEBABE12345678ULL;
constexpr std::uint64_t SUICIDE_KEY = 0x0BADF00DBADC0FFEULL;

} // namespace

std::uint8_t authenticate(
    std::uint64_t round,
    std::uint8_t sonar_id,
    std::uint64_t k0,
    std::uint64_t k1)
{
    std::uint64_t input = (round << 8) | sonar_id;
    std::uint64_t hash = siphash24(input, k0, k1);

    return hash >> 58;
}

std::uint64_t enemy_queen_sonar(
    std::uint8_t x,
    std::uint8_t y,
    std::uint8_t propagate,
    std::uint64_t round)
{
    std::uint64_t msg = 0;

    msg |= 0b1111ULL << 60;

    // Queen X: bits 59-54
    msg |= (std::uint64_t)(x & 0x3F) << 54;

    // Queen Y: bits 53-48
    msg |= (std::uint64_t)(y & 0x3F) << 48;

    // Propagate count: bits 47-45 (3 bits), decrements on propagation
    msg |= (std::uint64_t)(propagate & 0x07) << 45;

    // 39 random bits -> bits 44-6
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uint64_t random_bits = gen() & ((1ULL << 39) - 1);
    msg |= random_bits << 6;

    // 6-bit authentication -> bits 5-0
    std::uint8_t auth =
        authenticate(round, 0b1111, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}

std::uint64_t idle_sonar(
    std::uint8_t hx,
    std::uint8_t hy,
    const std::vector<std::vector<bool>>& pearl_grid,
    std::uint64_t round)
{
    std::uint64_t msg = 0;

    // ID = 0010
    msg |= 0b0010ULL << 60;

    // Head position
    msg |= (std::uint64_t)(hx & 0x3F) << 54;
    msg |= (std::uint64_t)(hy & 0x3F) << 48;

    // 6x6 pearl grid = 36 bits
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (pearl_grid[i][j]) {
                int bit = 35 - (i * 6 + j);
                msg |= 1ULL << (bit + 12);
            }
        }
    }

    // 6 random bits -> bits 11-6
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uint64_t random_bits = gen() & 0x3F;
    msg |= random_bits << 6;

    // Authentication -> bits 5-0
    std::uint8_t auth =
        authenticate(round, 0b0010, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}

std::uint64_t portal_sonar(
    std::uint8_t sx,
    std::uint8_t sy,
    std::uint8_t sdir,
    std::uint8_t x1,
    std::uint8_t y1,
    std::uint8_t x2,
    std::uint8_t y2,
    std::uint8_t dir2,
    std::uint8_t x3,
    std::uint8_t y3,
    std::uint8_t dir3,
    std::uint64_t round)
{
    std::uint64_t msg = 0;

    // ID = 0001
    msg |= 0b0001ULL << 60;

    // Start point
    msg |= (std::uint64_t)(sx & 0x3F) << 54;
    msg |= (std::uint64_t)(sy & 0x3F) << 48;
    msg |= (std::uint64_t)(sdir & 0x03) << 46;

    // Critical point 1
    msg |= (std::uint64_t)(x1 & 0x3F) << 40;
    msg |= (std::uint64_t)(y1 & 0x3F) << 34;

    // Critical point 2
    msg |= (std::uint64_t)(x2 & 0x3F) << 28;
    msg |= (std::uint64_t)(y2 & 0x3F) << 22;
    msg |= (std::uint64_t)(dir2 & 0x03) << 20;

    // Critical point 3
    msg |= (std::uint64_t)(x3 & 0x3F) << 14;
    msg |= (std::uint64_t)(y3 & 0x3F) << 8;
    msg |= (std::uint64_t)(dir3 & 0x03) << 6;

    // Authentication
    std::uint8_t auth =
        authenticate(round, 0b0001, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}

std::uint64_t suicide_sonar(std::uint64_t round)
{
    std::uint64_t msg = 0;

    // ID = 0000
    msg |= 0b0000ULL << 60;

    // 54-bit secret
    std::uint64_t secret = SUICIDE_KEY & ((1ULL << 54) - 1);
    msg |= secret << 6;

    // 6-bit authentication
    std::uint8_t auth =
        authenticate(round, 0b0000, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}
std::uint64_t queen_killed_sonar(std::uint64_t round)
{
    std::uint64_t msg = 0;

    // ID = 0011
    msg |= 0b0011ULL << 60;

    // 54 random bits -> bits 59-6
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uint64_t random_bits = gen() & ((1ULL << 54) - 1);
    msg |= random_bits << 6;

    // 6-bit authentication -> bits 5-0
    std::uint8_t auth =
        authenticate(round, 0b0011, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}
std::uint64_t food_signal(
    std::uint8_t x,
    std::uint8_t y,
    std::uint8_t value,
    std::uint64_t round)
{
    std::uint64_t msg = 0;

    // ID = 1001
    msg |= 0b1001ULL << 60;

    // X coordinate: bits 59-54
    msg |= (std::uint64_t)(x & 0x3F) << 54;

    // Y coordinate: bits 53-48
    msg |= (std::uint64_t)(y & 0x3F) << 48;

    // 8-bit value: bits 47-40
    msg |= (std::uint64_t)(value & 0xFF) << 40;

    // 34 random bits -> bits 39-6
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uint64_t random_bits = gen() & ((1ULL << 34) - 1);
    msg |= random_bits << 6;

    // 6-bit authentication -> bits 5-0
    std::uint8_t auth =
        authenticate(round, 0b1001, SECRET_K0, SECRET_K1);
    msg |= auth;

    return msg;
}

std::vector<std::uint64_t> decode_sonar(
    std::uint64_t msg,
    std::uint64_t round)
{
    std::vector<std::uint64_t> decoded;

    // Extract sonar ID
    std::uint8_t sonar_id = (msg >> 60) & 0x0F;

    // Extract received authentication
    std::uint8_t received_auth = msg & 0x3F;

    // Check current round
    std::uint8_t expected_auth =
        authenticate(round, sonar_id, SECRET_K0, SECRET_K1);

    bool valid = (received_auth == expected_auth);

    // Check previous round
    if (!valid && round > 0) {
        expected_auth =
            authenticate(round - 1, sonar_id, SECRET_K0, SECRET_K1);

        valid = (received_auth == expected_auth);
    }

    // Reject if neither round matches
    if (!valid)
        return {};

    decoded.push_back(sonar_id);

    if (sonar_id == 0b1111) {
        // Enemy Queen
        std::uint8_t x = (msg >> 54) & 0x3F;
        std::uint8_t y = (msg >> 48) & 0x3F;

        decoded.push_back(x);
        decoded.push_back(y);
        std::uint8_t propagate = (msg >> 45) & 0x07;
        decoded.push_back(propagate);
    }
    else if (sonar_id == 0b0010) {
        // Idle
        std::uint8_t hx = (msg >> 54) & 0x3F;
        std::uint8_t hy = (msg >> 48) & 0x3F;

        decoded.push_back(hx);
        decoded.push_back(hy);

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) {
                int bit = 35 - (i * 6 + j);
                std::uint8_t pearl =
                    (msg >> (bit + 12)) & 1ULL;

                decoded.push_back(pearl);
            }
        }
    }
    else if (sonar_id == 0b0001) {
        // Portal
        std::uint8_t sx = (msg >> 54) & 0x3F;
        std::uint8_t sy = (msg >> 48) & 0x3F;
        std::uint8_t sdir = (msg >> 46) & 0x03;

        std::uint8_t x1 = (msg >> 40) & 0x3F;
        std::uint8_t y1 = (msg >> 34) & 0x3F;

        std::uint8_t x2 = (msg >> 28) & 0x3F;
        std::uint8_t y2 = (msg >> 22) & 0x3F;
        std::uint8_t dir2 = (msg >> 20) & 0x03;

        std::uint8_t x3 = (msg >> 14) & 0x3F;
        std::uint8_t y3 = (msg >> 8) & 0x3F;
        std::uint8_t dir3 = (msg >> 6) & 0x03;

        decoded.push_back(sx);
        decoded.push_back(sy);
        decoded.push_back(sdir);

        decoded.push_back(x1);
        decoded.push_back(y1);

        decoded.push_back(x2);
        decoded.push_back(y2);
        decoded.push_back(dir2);

        decoded.push_back(x3);
        decoded.push_back(y3);
        decoded.push_back(dir3);
    }
    else if (sonar_id == 0b0000) {
        // Suicide
        std::uint64_t secret =
            (msg >> 6) & ((1ULL << 54) - 1);

        std::uint64_t expected_secret =
            SUICIDE_KEY & ((1ULL << 54) - 1);

        if (secret != expected_secret)
            return {};

        decoded.push_back(secret);
    }
    else if (sonar_id == 0b0011) {
        // Queen killed
        // Remaining 54 bits are random.
        // Only the sonar ID and authentication are meaningful.
    }
    else if (sonar_id == 0b1001) {
        // Food signal
        std::uint8_t x = (msg >> 54) & 0x3F;
        std::uint8_t y = (msg >> 48) & 0x3F;
        std::uint8_t value = (msg >> 40) & 0xFF;

        decoded.push_back(x);
        decoded.push_back(y);
        decoded.push_back(value);
    }
    else {
        return {};
    }

    return decoded;
}