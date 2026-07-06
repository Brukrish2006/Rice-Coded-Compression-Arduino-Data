#!/usr/bin/env python3
"""
================================================================================
SENSOR DATA DECOMPRESSION BACKEND
================================================================================
Lossless decompression for Arduino-compressed temperature and LDR sensor data.
"""

import struct
import os
import sys
from typing import List, Tuple, Optional
from dataclasses import dataclass
from pathlib import Path

ESCAPE_THRESHOLD = 127

@dataclass
class CompressedHeader:
    """Structure for compressed file header"""
    magic: str
    version: int
    count: int
    base_value: int
    rice_k: int

class RiceDecoder:
    """Rice (Golomb) decoder for non-negative integers"""

    def __init__(self, k: int):
        self.k = k
        self.M = 1 << k

    def decode_value(self, bit_stream: str, start_idx: int) -> Tuple[int, int]:
        """
        Decode a single Rice-coded value from bit stream.
        Returns: (decoded_value, next_bit_index)
        """
        q = 0
        idx = start_idx
        
        esc_q = ESCAPE_THRESHOLD >> self.k
        
        while idx < len(bit_stream) and bit_stream[idx] == '1':
            q += 1
            idx += 1
            if q == esc_q:
                break

        if idx >= len(bit_stream) or bit_stream[idx] != '0':
            raise ValueError("Invalid Rice code: missing stop bit")
        idx += 1

        if q == esc_q:
            # Escape path: read 16 bits
            if idx + 16 > len(bit_stream):
                raise ValueError("Incomplete bit stream for escape value")
            value = int(bit_stream[idx:idx + 16], 2)
            idx += 16
            return value, idx

        if idx + self.k > len(bit_stream):
            raise ValueError("Incomplete bit stream for remainder")

        r = 0
        if self.k > 0:
            r = int(bit_stream[idx:idx + self.k], 2)
        idx += self.k

        value = q * self.M + r
        return value, idx

def zigzag_decode(u: int) -> int:
    """
    Inverse zigzag mapping: unsigned -> signed
    """
    if u % 2 == 0:
        return u // 2
    else:
        return -(u + 1) // 2

def bytes_to_bitstream(data: bytes) -> str:
    """Convert byte array to bit string"""
    return ''.join(format(b, '08b') for b in data)

def decompress_temperature(filename: str) -> List[int]:
    """
    Decompress temperature data from binary file.
    """
    with open(filename, 'rb') as f:
        data = f.read()

    magic = data[0:4].decode('ascii')
    if magic != "THMP":
        raise ValueError(f"Invalid magic: {magic}")

    version = data[4]
    count = struct.unpack('>H', data[5:7])[0]
    base_value = struct.unpack('>H', data[7:9])[0]
    rice_k = data[9]

    print(f"Header: magic={magic}, version={version}, count={count}")
    print(f"Base={base_value}, k*={rice_k}")

    payload = data[10:]
    bit_stream = bytes_to_bitstream(payload)

    decoder = RiceDecoder(rice_k)
    deltas = []
    idx = 0

    for _ in range(count - 1):
        unsigned_delta, idx = decoder.decode_value(bit_stream, idx)
        signed_delta = zigzag_decode(unsigned_delta)
        deltas.append(signed_delta)

    reconstructed = [base_value]
    current = base_value
    for d in deltas:
        current += d
        reconstructed.append(current)

    print(f"Decompressed {len(reconstructed)} values")
    return reconstructed

def decompress_ldr(filename: str) -> List[int]:
    """
    Decompress LDR data with regime-based structure.
    """
    with open(filename, 'rb') as f:
        data = f.read()

    magic = data[0:4].decode('ascii')
    if magic != "LDRS":
        raise ValueError(f"Invalid magic: {magic}")

    version = data[4]
    run_count = data[5]

    print(f"Header: magic={magic}, version={version}, runs={run_count}")

    idx = 6
    runs = []
    for _ in range(run_count):
        regime = data[idx]
        base = struct.unpack('>H', data[idx+1:idx+3])[0]
        count = struct.unpack('>H', data[idx+3:idx+5])[0]
        k = data[idx+5]
        runs.append((regime, base, count, k))
        idx += 6

    payload = data[idx:]
    bit_stream = bytes_to_bitstream(payload)

    reconstructed = []
    bit_idx = 0

    for regime, base, count, k in runs:
        run_values = [base]
        current = base

        decoder = RiceDecoder(k)

        for _ in range(count - 1):
            unsigned_delta, bit_idx = decoder.decode_value(bit_stream, bit_idx)
            signed_delta = zigzag_decode(unsigned_delta)
            current += signed_delta
            run_values.append(current)

        reconstructed.extend(run_values)

    print(f"Decompressed {len(reconstructed)} values across {run_count} runs")
    return reconstructed

def adc_to_temperature(adc_value: int) -> float:
    """Convert ADC reading back to temperature"""
    voltage_mV = adc_value * (5000.0 / 1023.0)
    baselineVoltage_mV = 655.0
    roomTemperatureC = 33.0
    tempCoefficient = 2.0

    voltage_diff = baselineVoltage_mV - voltage_mV
    return roomTemperatureC + (voltage_diff / tempCoefficient)

def batch_process_directory(input_dir: str, output_dir: str):
    """Process all compressed files in a directory"""
    os.makedirs(output_dir, exist_ok=True)

    for filename in os.listdir(input_dir):
        if not filename.endswith('.bin'):
            continue

        filepath = os.path.join(input_dir, filename)

        try:
            if filename.startswith('temp_'):
                values = decompress_temperature(filepath)
                output_file = os.path.join(
                    output_dir, filename.replace('.bin', '.csv'))
                with open(output_file, 'w') as f:
                    f.write("sample,adc_value,temperature_C\n")
                    for i, adc in enumerate(values):
                        temp = adc_to_temperature(adc)
                        f.write(f"{i},{adc},{temp:.2f}\n")
                print(f"Saved: {output_file}\n")

            elif filename.startswith('ldr_'):
                values = decompress_ldr(filepath)
                output_file = os.path.join(
                    output_dir, filename.replace('.bin', '.csv'))
                with open(output_file, 'w') as f:
                    f.write("sample,ldr_value,regime\n")
                    for i, val in enumerate(values):
                        regime = "DARK" if val > 500 else "BRIGHT"
                        f.write(f"{i},{val},{regime}\n")
                print(f"Saved: {output_file}\n")

        except Exception as e:
            print(f"Error processing {filename}: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1].endswith(".bin"):
            if "temp_" in sys.argv[1]:
                decompress_temperature(sys.argv[1])
            else:
                decompress_ldr(sys.argv[1])
        else:
            batch_process_directory(sys.argv[1], sys.argv[1] + "_csv")
    else:
        print("Usage: python decompress.py <file.bin> OR python decompress.py <directory_with_bins>")
