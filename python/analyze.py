import pandas as pd
import sys

def analyze_csv(filepath):
    df = pd.read_csv(filepath)
    print(f"Analysis for {filepath}")
    print(df.describe())
    
    if "adc_value" in df.columns:
        print(f"\nMax diff between adjacent readings: {df['adc_value'].diff().abs().max()}")
    elif "ldr_value" in df.columns:
        print(f"\nMax diff between adjacent readings: {df['ldr_value'].diff().abs().max()}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        analyze_csv(sys.argv[1])
    else:
        print("Usage: python analyze.py <file.csv>")
