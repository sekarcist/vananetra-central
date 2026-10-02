import paho.mqtt.client as mqtt
import json, time, os, datetime

POWER_CSV = "power_live_log.csv"
RUMBLES_CSV = "rumbles_live_log.csv"

# Initialize CSV headers if files do not exist
if not os.path.exists(POWER_CSV):
    with open(POWER_CSV, "w", encoding="utf-8") as f:
        f.write("timestamp,uptime_s,solar_w,solar_v,solar_ma,battery_v,battery_soc,load_w,load_ma,total_solar_wh,total_load_wh,wifi_rssi\n")

if not os.path.exists(RUMBLES_CSV):
    with open(RUMBLES_CSV, "w", encoding="utf-8") as f:
        f.write("timestamp,node_id,rms_energy_pa,peak_pa,rumble_flag,sample_idx,pressure_hpa\n")

power_count = 0
rumble_count = 0

def on_connect(client, userdata, flags, rc):
    print("==================================================")
    print("📡 Connected to HiveMQ Broker (broker.hivemq.com)")
    print("📥 Subscribed to topics: vananetra/VN-01/#")
    print(f"💾 Saving Power Data  -> {os.path.abspath(POWER_CSV)}")
    print(f"💾 Saving Rumble Data -> {os.path.abspath(RUMBLES_CSV)}")
    print("Press Ctrl+C at any time to stop logging.")
    print("==================================================\n")
    client.subscribe("vananetra/VN-01/#")

def on_message(client, userdata, msg):
    global power_count, rumble_count
    try:
        now_iso = datetime.datetime.now().isoformat()
        payload = json.loads(msg.payload.decode("utf-8"))

        if "power" in msg.topic:
            power_count += 1
            line = (
                f"{now_iso},"
                f"{payload.get('uptime_s', 0)},"
                f"{payload.get('solar_p', 0.0)},"
                f"{payload.get('solar_v', 0.0)},"
                f"{payload.get('solar_i', 0.0)},"
                f"{payload.get('battery_v', 0.0)},"
                f"{payload.get('battery_soc', 0)},"
                f"{payload.get('load_p', 0.0)},"
                f"{payload.get('load_i', 0.0)},"
                f"{payload.get('total_solar_wh', 0.0)},"
                f"{payload.get('total_load_wh', 0.0)},"
                f"{payload.get('wifi_rssi', 0)}\n"
            )
            with open(POWER_CSV, "a", encoding="utf-8") as f:
                f.write(line)
                f.flush()
            
            print(f"[Power #{power_count}] Bat: {payload.get('battery_v'):.2f}V ({payload.get('battery_soc')}%) | "
                  f"Load: {payload.get('load_p'):.2f}W ({payload.get('load_i'):.1f}mA) | "
                  f"Sol: {payload.get('solar_p'):.2f}W | Tot Load: {payload.get('total_load_wh'):.2f}Wh", flush=True)

        elif "rumbles" in msg.topic:
            rumble_count += 1
            rms = payload.get("rms_energy", 0.0)
            peak = payload.get("peak_pa", 0.0)
            flag = 1 if payload.get("rumble_detected", False) else 0
            node = payload.get("node_id", "VN-01")
            samples = payload.get("samples", [])

            with open(RUMBLES_CSV, "a", encoding="utf-8") as f:
                for idx, s in enumerate(samples):
                    f.write(f"{now_iso},{node},{rms},{peak},{flag},{idx},{s}\n")
                f.flush()

            if flag == 1:
                print(f"🚨 [Rumble #{rumble_count}] ELEPHANT RUMBLE DETECTED! RMS: {rms:.2f} Pa | Peak: {peak:.2f} Pa", flush=True)
            else:
                print(f"[Rumble #{rumble_count}] Infrasound Batch: RMS={rms:.2f} Pa | Peak={peak:.2f} Pa | 64 samples saved", flush=True)

    except Exception as e:
        print("Parse error:", e)

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect("broker.hivemq.com", 1883, 60)
    
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print(f"\nStopped logging. Total Power rows: {power_count}, Total Rumble batches: {rumble_count}")

if __name__ == "__main__":
    main()
