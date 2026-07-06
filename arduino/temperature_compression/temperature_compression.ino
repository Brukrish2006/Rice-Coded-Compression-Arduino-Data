#include <SD.h>
#include <SPI.h>
#include "RiceCodec.h"

// ============== CONFIGURATION ==============
#define SENSOR_PIN A0
#define SD_CS_PIN 10
#define LED_PIN 13
#define SAMPLE_INTERVAL 1000 // Sampling interval in ms (1 Hz)
#define BATCH_SIZE 60        // Samples per compression batch
#define MAX_SAMPLES 500      // Maximum samples to collect

#define MAGIC_HEADER "THMP"
#define VERSION 0x01

// ============== GLOBAL VARIABLES ==============
uint16_t adcBuffer[BATCH_SIZE];       // Raw ADC readings
int16_t deltaBuffer[BATCH_SIZE - 1];  // Delta-encoded values
uint8_t compressedBuffer[512];        // Compressed output buffer
uint16_t sampleCount = 0;
uint32_t batchCount = 0;
File dataFile;

// ============== CALIBRATION ==============
const float roomTemperatureC = 33.0;
const float baselineVoltage_mV = 655.0;
const float tempCoefficient = 2.0; // ~2mV/degC for silicon diode

// ============== SETUP ==============
void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    pinMode(LED_PIN, OUTPUT);
    pinMode(SENSOR_PIN, INPUT);

    Serial.println("Initializing Temperature Monitor with Compression...");

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("ERROR: SD card initialization failed!");
        while (1);
    }
    Serial.println("SD card initialized.");

    if (!SD.exists("/compressed")) {
        SD.mkdir("/compressed");
    }

    Serial.println("System Ready. Collecting data...");
}

// ============== SENSOR READING ==============
float readTemperature() {
    int rawValue = analogRead(SENSOR_PIN);
    float currentVoltage_mV = rawValue * (5000.0 / 1023.0);
    float voltageDifference = baselineVoltage_mV - currentVoltage_mV;
    return roomTemperatureC + (voltageDifference / tempCoefficient);
}

// ============== SD CARD STORAGE ==============
void writeCompressedToSD(uint8_t *data, uint16_t bits, uint8_t k, uint16_t base) {
    char filename[32];
    sprintf(filename, "/compressed/temp_%04d.bin", batchCount);

    dataFile = SD.open(filename, FILE_WRITE);
    if (!dataFile) {
        Serial.println("ERROR: Failed to open file!");
        return;
    }

    // Header: [Magic(4B)][Version(1B)][Count(2B)][Base(2B)][k(1B)]
    dataFile.write((uint8_t*)MAGIC_HEADER, 4);
    dataFile.write(VERSION);

    uint16_t count = min(BATCH_SIZE, sampleCount - (batchCount * BATCH_SIZE));
    dataFile.write((count >> 8) & 0xFF);
    dataFile.write(count & 0xFF);
    dataFile.write((base >> 8) & 0xFF);
    dataFile.write(base & 0xFF);
    dataFile.write(k);

    uint16_t bytes = (bits + 7) >> 3;
    dataFile.write(data, bytes);
    dataFile.close();

    Serial.print("Saved to: "); Serial.println(filename);
}

// ============== COMPRESSION PIPELINE ==============
void processAndStoreBatch() {
    uint16_t currentBatchSize = min(BATCH_SIZE, sampleCount - (batchCount * BATCH_SIZE));
    if (currentBatchSize < 2) return;

    Serial.println("\n========== COMPRESSING BATCH ==========");
    Serial.print("Batch "); Serial.println(batchCount);

    deltaEncode(adcBuffer, deltaBuffer, currentBatchSize);

    uint16_t zigzagBuffer[BATCH_SIZE];
    zigzagEncode(deltaBuffer, currentBatchSize - 1, zigzagBuffer);

    uint8_t optimalK = findOptimalRiceK(zigzagBuffer, currentBatchSize - 1);
    Serial.print("Optimal k*: "); Serial.println(optimalK);

    uint16_t totalBits = 0;
    riceEncode(zigzagBuffer, currentBatchSize - 1, optimalK, compressedBuffer, &totalBits);

    Serial.print("Compressed to "); Serial.print(totalBits);
    Serial.print(" bits ("); Serial.print((totalBits + 7) / 8);
    Serial.println(" bytes)");

    writeCompressedToSD(compressedBuffer, totalBits, optimalK, adcBuffer[0]);

    uint16_t originalBytes = currentBatchSize * 2;
    uint16_t compressedBytes = (totalBits + 7) / 8 + 9;
    float ratio = (float)originalBytes / compressedBytes;

    Serial.print("Original: "); Serial.print(originalBytes);
    Serial.print(" bytes | Compressed: "); Serial.print(compressedBytes);
    Serial.print(" bytes | Ratio: "); Serial.print(ratio, 2);
    Serial.println("x\n");

    batchCount++;
}

// ============== MAIN LOOP ==============
void loop() {
    if (sampleCount < MAX_SAMPLES) {
        uint16_t batchIndex = sampleCount % BATCH_SIZE;
        adcBuffer[batchIndex] = analogRead(SENSOR_PIN);

        float temp = readTemperature();
        Serial.print("Sample "); Serial.print(sampleCount);
        Serial.print(" | ADC: "); Serial.print(adcBuffer[batchIndex]);
        Serial.print(" | Temp: "); Serial.print(temp, 2);
        Serial.println(" C");

        sampleCount++;

        if (sampleCount % BATCH_SIZE == 0 || sampleCount >= MAX_SAMPLES) {
            processAndStoreBatch();
        }
        delay(SAMPLE_INTERVAL);
    } else {
        Serial.println("Data collection complete.");
        delay(10000);
    }
}
