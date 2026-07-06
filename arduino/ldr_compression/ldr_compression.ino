#include <SD.h>
#include <SPI.h>
#include "RiceCodec.h"

#define LDR_PIN A0
#define SD_CS_PIN 10
#define LED_PIN 13
#define SAMPLE_INTERVAL 100 // 10 Hz
#define REGIME_THRESHOLD 500 // Dark/Bright threshold
#define MAGIC_HEADER "LDRS"
#define VERSION 0x01

struct RegimeRun {
    uint8_t regime; // 0=LOW, 1=HIGH
    uint16_t base;
    uint16_t count;
    uint8_t optimalK;
};

uint16_t ldrBuffer[256];
int16_t deltaBuffer[255];
uint8_t compressedBuffer[512];
RegimeRun runs[32];
uint8_t runCount = 0;
uint16_t sampleCount = 0;
uint32_t batchCount = 0;
File dataFile;

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LDR_PIN, INPUT);

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD init failed!");
        while(1);
    }
    if (!SD.exists("/compressed")) SD.mkdir("/compressed");

    Serial.println("LDR Monitor with Regime Compression Ready");
}

void segmentRegimes() {
    uint8_t currentRegime = (ldrBuffer[0] < REGIME_THRESHOLD) ? 0 : 1;
    uint16_t runStart = 0;
    runCount = 0;

    for (uint16_t i = 1; i <= 256; i++) { 
        uint8_t regime = (i < 256) ? ((ldrBuffer[i] < REGIME_THRESHOLD) ? 0 : 1) : !currentRegime;
        if (regime != currentRegime || i == 256) {
            runs[runCount].regime = currentRegime;
            runs[runCount].base = ldrBuffer[runStart];
            runs[runCount].count = i - runStart;
            runCount++;
            if (i < 256) {
                currentRegime = regime;
                runStart = i;
            }
        }
    }
}

void writeLDRToSD(uint16_t totalBits) {
    char filename[32];
    sprintf(filename, "/compressed/ldr_%04d.bin", batchCount++);

    dataFile = SD.open(filename, FILE_WRITE);
    if (!dataFile) return;

    dataFile.write((uint8_t*)MAGIC_HEADER, 4);
    dataFile.write(VERSION);
    dataFile.write(runCount);

    for (uint8_t r = 0; r < runCount; r++) {
        dataFile.write(runs[r].regime);
        dataFile.write((runs[r].base >> 8) & 0xFF);
        dataFile.write(runs[r].base & 0xFF);
        dataFile.write((runs[r].count >> 8) & 0xFF);
        dataFile.write(runs[r].count & 0xFF);
        dataFile.write(runs[r].optimalK);
    }

    dataFile.write(compressedBuffer, (totalBits + 7) / 8);
    dataFile.close();

    Serial.print("Saved: "); Serial.println(filename);
}

void processLDRBatch() {
    Serial.println("\n========== LDR COMPRESSION ==========");

    segmentRegimes();

    uint16_t totalBits = 0;
    
    // Clear compressedBuffer
    for (int i = 0; i < 512; i++) compressedBuffer[i] = 0;

    for (uint8_t r = 0; r < runCount; r++) {
        uint16_t startIdx = 0;
        for (uint8_t prev = 0; prev < r; prev++) {
            startIdx += runs[prev].count;
        }

        uint16_t runDeltas[256];
        int16_t rawDeltas[256];
        
        for (uint16_t i = 0; i < runs[r].count - 1; i++) {
            rawDeltas[i] = (int16_t)ldrBuffer[startIdx + i + 1] - (int16_t)ldrBuffer[startIdx + i];
        }
        
        zigzagEncode(rawDeltas, runs[r].count - 1, runDeltas);

        runs[r].optimalK = findOptimalRiceK(runDeltas, runs[r].count - 1);

        uint16_t bits = 0;
        // Since riceEncode replaces all content in output buffer, we need a local buffer
        uint8_t localBuffer[512] = {0};
        riceEncode(runDeltas, runs[r].count - 1, runs[r].optimalK, localBuffer, &bits);
        
        // Append localBuffer to compressedBuffer
        for (uint16_t i = 0; i < bits; i++) {
            uint8_t byteIdx = i >> 3;
            uint8_t bitIdx = 7 - (i & 7);
            uint8_t bitVal = (localBuffer[byteIdx] >> bitIdx) & 1;
            if (bitVal) {
                setBit(compressedBuffer, totalBits + i);
            }
        }
        
        totalBits += bits;
    }

    writeLDRToSD(totalBits);

    Serial.print("Runs: "); Serial.println(runCount);
    Serial.print("Total compressed bits: "); Serial.println(totalBits);
    Serial.print("Compression ratio: ~");
    Serial.print((float)(256 * 2) / ((totalBits + 7)/8.0 + 6 + runCount * 6), 1);
    Serial.println("x");
}

void loop() {
    if (sampleCount < 256) {
        ldrBuffer[sampleCount] = analogRead(LDR_PIN);
        Serial.print("LDR: "); Serial.println(ldrBuffer[sampleCount]);
        sampleCount++;
        delay(SAMPLE_INTERVAL);
    } else if (sampleCount == 256) {
        processLDRBatch();
        sampleCount = 257; // Stop collecting after one batch
    }
}
