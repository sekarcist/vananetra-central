import serial
import time
import csv

PORT = "COM13"
BAUD = 115200
DURATION = 30 # 30 seconds high-speed recording

pressure_file = "pressure_data.csv"
power_file = "power_telemetry.csv"

print(f"Connecting to {PORT} @ {BAUD} baud...")
s = serial.Serial(PORT, BAUD, timeout=1)

# Reset ESP32 cleanly
s.dtr = False
s.rts = True
time.sleep(0.1)
s.rts = False
time.sleep(0.5)

print(f"Recording high-speed telemetry for {DURATION} seconds...")

p_records = []
b_records = []

start_time = time.time()
last_report = 0

with open(pressure_file, mode='w', newline='') as f_p, open(power_file, mode='w', newline='') as f_b:
    writer_p = csv.writer(f_p)
    writer_b = csv.writer(f_b)

    writer_p.writerow(["pc_time", "esp_ms", "pressure_hPa"])
    writer_b.writerow(["pc_time", "esp_ms", "solar_v", "solar_mA", "solar_mW", "load_v", "load_mA", "load_mW"])

    while time.time() - start_time < DURATION:
        elapsed = time.time() - start_time
        if int(elapsed) > last_report and int(elapsed) % 5 == 0:
            last_report = int(elapsed)
            print(f"  ... {int(elapsed)}s / {DURATION}s (Pressure Samples: {len(p_records)}, Power Samples: {len(b_records)})")

        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            continue

        ts_now = time.strftime("%Y-%m-%d %H:%M:%S")

        if line.startswith("P,"):
            parts = line.split(",")
            if len(parts) == 3:
                try:
                    esp_ms = int(parts[1])
                    p_val = float(parts[2])
                    writer_p.writerow([ts_now, esp_ms, p_val])
                    p_records.append((esp_ms, p_val))
                except ValueError:
                    pass

        elif line.startswith("B,"):
            parts = line.split(",")
            if len(parts) == 8:
                try:
                    esp_ms = int(parts[1])
                    s_v = float(parts[2])
                    s_i = float(parts[3])
                    s_p = float(parts[4])
                    l_v = float(parts[5])
                    l_i = float(parts[6])
                    l_p = float(parts[7])
                    writer_b.writerow([ts_now, esp_ms, s_v, s_i, s_p, l_v, l_i, l_p])
                    b_records.append((esp_ms, s_v, s_i, s_p, l_v, l_i, l_p))
                    print(f"  [Load Telemetry] Bat/Load: {l_v:.2f}V | Fan Current: {l_i:.1f}mA | Fan Power: {l_p:.2f}W | Solar: {s_v:.2f}V")
                except ValueError:
                    pass
        elif line.startswith("#"):
            print(f"  [ESP32 System Status]: {line}")

s.close()
print("\nRecording Complete!")
print(f"Saved {len(p_records)} pressure data rows to '{pressure_file}'")
print(f"Saved {len(b_records)} power telemetry rows to '{power_file}'")
