# Remote Sensor Data Monitoring with Lossless Techniques

This repository provides domain-specific lossless compression algorithms for resource-constrained Arduino microcontroller platforms. It features a delta-Rice coding scheme for temperature data and a regime-based adaptive Rice coding scheme for LDR (Light Dependent Resistor) data.

## Features
*   **Lossless Compression**: Exact byte-for-byte reconstruction.
*   **Resource Efficient**: Requires <256 bytes of RAM on an Arduino Uno during encoding.
*   **Domain-Specific**: Exploits structural properties of physical sensor data (temporal coherence, regime-based behavior).
*   **Cross-platform Decompression**: Python backend for restoring and analyzing data.

## Structure
*   `arduino/`: Contains the shared `RiceCodec` C++ files and the sketches for `temperature_compression` and `ldr_compression`.
*   `python/`: Contains the `decompress.py` backend, statistical analysis tools, and unit tests.

## Usage
1.  **Arduino**: Upload the desired sketch to your Arduino with an SD card module connected (CS=10, MOSI=11, MISO=12, SCK=13). The encoded data will be saved as `.bin` files in the `/compressed/` directory on the SD card.
2.  **Python**: Use `python/decompress.py <file.bin>` to convert the binary files back into readable lists or process a whole directory using the script's `batch_process_directory` functionality.

## Tests
Run the unit tests in the `python/` directory using:
```bash
python -m unittest test_decompress.py
```
