import pandas as pd
import numpy as np

print("==================================================")
print("       TELEMETRY DATA INSPECTION REPORT           ")
print("==================================================")

# 1. Inspect Pressure Data
try:
    df_p = pd.read_csv("pressure_data.csv")
    print("\n--- 1. PRESSURE DATA ANALYSIS (pressure_data.csv) ---")
    print(f"Total Rows Captured:       {len(df_p)}")
    
    if len(df_p) > 1:
        # Time difference in ms between consecutive samples
        diffs = np.diff(df_p['esp_ms'])
        avg_dt = np.mean(diffs)
        sample_rate = 1000.0 / avg_dt if avg_dt > 0 else 0
        
        print(f"First Sample Time (ms):    {df_p['esp_ms'].iloc[0]}")
        print(f"Last Sample Time (ms):     {df_p['esp_ms'].iloc[-1]}")
        print(f"Total Duration:            {(df_p['esp_ms'].iloc[-1] - df_p['esp_ms'].iloc[0]) / 1000.0:.2f} seconds")
        print(f"Average Sampling Interval: {avg_dt:.2f} ms")
        print(f"Effective Sample Rate:     {sample_rate:.1f} samples/sec (Hz)")
        
        p = df_p['pressure_hPa']
        print(f"Mean Pressure:             {p.mean():.3f} hPa")
        print(f"Min Pressure:              {p.min():.3f} hPa")
        print(f"Max Pressure:              {p.max():.3f} hPa")
        print(f"Peak-to-Peak Noise:        {(p.max() - p.min()):.4f} hPa ({(p.max() - p.min()) * 100.0:.1f} Pa)")
        print(f"Std Deviation (Jitter):    {p.std():.4f} hPa ({p.std() * 100.0:.2f} Pa)")
        
        print("\nFirst 5 Pressure Samples:")
        print(df_p.head())
        print("\nLast 5 Pressure Samples:")
        print(df_p.tail())
except Exception as e:
    print(f"Error inspecting pressure_data.csv: {e}")

# 2. Inspect Power Telemetry Data
try:
    df_b = pd.read_csv("power_telemetry.csv")
    print("\n--- 2. POWER & LOAD DATA ANALYSIS (power_telemetry.csv) ---")
    print(f"Total Rows Captured:       {len(df_b)} (Target: 60)")
    
    print("\nFan Current Draw (Stone-PRO DC6025HSL Brushless Fan):")
    print(f"  Average Current:         {df_b['load_mA'].mean():.2f} mA")
    print(f"  Minimum Current:         {df_b['load_mA'].min():.2f} mA")
    print(f"  Maximum Current:         {df_b['load_mA'].max():.2f} mA")
    
    print("\nBattery / Load Bus Voltage:")
    print(f"  Average Measured Voltage: {df_b['load_v'].mean():.2f} V")
    print(f"  Expected Battery Voltage: 13.90 V (as shown on CM-D20 display)")
    
    print("\nSolar Input (INA260 @ 0x40):")
    print(f"  Average Solar Voltage:   {df_b['solar_v'].mean():.2f} V (Idle/Disconnected)")
    print(f"  Average Solar Current:   {df_b['solar_mA'].mean():.2f} mA")

    print("\nFirst 5 Power Samples:")
    print(df_b.head())
except Exception as e:
    print(f"Error inspecting power_telemetry.csv: {e}")

print("==================================================")
