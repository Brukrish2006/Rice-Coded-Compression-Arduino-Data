#include "RiceCodec.h"

void setBit(uint8_t *buffer, uint16_t pos) {
    buffer[pos >> 3] |= (1 << (7 - (pos & 7)));
}

void clearBit(uint8_t *buffer, uint16_t pos) {
    buffer[pos >> 3] &= ~(1 << (7 - (pos & 7)));
}

void deltaEncode(uint16_t *adcBuffer, int16_t *deltaBuffer, uint16_t currentBatchSize) {
    for (uint16_t i = 1; i < currentBatchSize; i++) {
        deltaBuffer[i - 1] = (int16_t)adcBuffer[i] - (int16_t)adcBuffer[i - 1];
    }
}

void zigzagEncode(int16_t *deltas, uint16_t count, uint16_t *encoded) {
    for (uint16_t i = 0; i < count; i++) {
        int16_t v = deltas[i];
        encoded[i] = (v >= 0) ? (uint16_t)(2 * v) : (uint16_t)(-2 * v - 1);
    }
}

uint8_t findOptimalRiceK(uint16_t *values, uint16_t count) {
    uint32_t minTotalBits = 0xFFFFFFFF;
    uint8_t bestK = 0;

    for (uint8_t k = RICE_K_MIN; k <= RICE_K_MAX; k++) {
        uint32_t totalBits = 0;
        for (uint16_t i = 0; i < count; i++) {
            uint16_t v = values[i];
            if (v > ESCAPE_THRESHOLD) {
                uint16_t esc_q = ESCAPE_THRESHOLD >> k;
                totalBits += esc_q + 1 + 16;
            } else {
                uint16_t q = v >> k;
                totalBits += q + 1 + k;
            }
        }
        if (totalBits < minTotalBits) {
            minTotalBits = totalBits;
            bestK = k;
        }
    }
    return bestK;
}

void riceEncode(uint16_t *values, uint16_t count, uint8_t k, uint8_t *output, uint16_t *outBits) {
    uint16_t M = 1 << k;
    uint16_t bitPos = 0;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t v = values[i];

        if (v > ESCAPE_THRESHOLD) {
            uint16_t esc_q = ESCAPE_THRESHOLD >> k;
            for (uint16_t j = 0; j < esc_q; j++) setBit(output, bitPos++);
            clearBit(output, bitPos++);
            for (int8_t b = 0; b < 16; b++) {
                if (v & (1 << (15 - b))) setBit(output, bitPos++);
                else clearBit(output, bitPos++);
            }
            continue;
        }

        uint16_t q = v >> k;
        uint16_t r = v & (M - 1);

        for (uint16_t j = 0; j < q; j++) setBit(output, bitPos++);
        clearBit(output, bitPos++);

        for (int8_t b = k - 1; b >= 0; b--) {
            if (r & (1 << b)) setBit(output, bitPos++);
            else clearBit(output, bitPos++);
        }
    }
    *outBits = bitPos;
}
