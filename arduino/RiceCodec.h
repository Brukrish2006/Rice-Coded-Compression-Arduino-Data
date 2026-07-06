#ifndef RICE_CODEC_H
#define RICE_CODEC_H

#include <Arduino.h>

#define RICE_K_MIN 0
#define RICE_K_MAX 7
#define ESCAPE_THRESHOLD 127

// Shared utility functions
void setBit(uint8_t *buffer, uint16_t pos);
void clearBit(uint8_t *buffer, uint16_t pos);

// Compression pipeline functions
void deltaEncode(uint16_t *adcBuffer, int16_t *deltaBuffer, uint16_t currentBatchSize);
void zigzagEncode(int16_t *deltas, uint16_t count, uint16_t *encoded);
uint8_t findOptimalRiceK(uint16_t *values, uint16_t count);
void riceEncode(uint16_t *values, uint16_t count, uint8_t k, uint8_t *output, uint16_t *outBits);

#endif
