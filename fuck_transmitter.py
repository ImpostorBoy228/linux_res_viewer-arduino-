import time
import json
import locale
import psutil
import subprocess
import serial
import os
from collections import OrderedDict

SERIAL_PORT = "/dev/ttyACM0"
BAUD = 9600
SEND_INTERVAL = 2.0
POST_WRITE_DELAY = 0.3

try:
    locale.setlocale(locale.LC_NUMERIC, "C")
except Exception:
    pass

def _run_cmd(cmd):
    try:
        out = subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL)
        return out.strip()
    except Exception:
        return None

def get_cpu_gpu_temps(prev_cpu=None, prev_gpu=None):
    cpu_t, gpu_t = None, None
    try:
        temps = psutil.sensors_temperatures()
        if "coretemp" in temps:
            vals = [e.current for e in temps["coretemp"] if e.current]
            if vals: cpu_t = max(vals)
        if "amdgpu" in temps:
            vals = [e.current for e in temps["amdgpu"] if e.current]
            if vals: gpu_t = max(vals)
    except Exception:
        pass
    if cpu_t is None: cpu_t = prev_cpu
    if gpu_t is None: gpu_t = prev_gpu
    return cpu_t, gpu_t

def get_metrics(state):
    cpu = psutil.cpu_percent()
    ram = psutil.virtual_memory().percent
    f = psutil.cpu_freq()
    cpu_speed = (f.current / 1000.0) if f and f.current else 0.0
    load_avg_5min = os.getloadavg()[1] if hasattr(os, "getloadavg") else 0.0

    pernic = psutil.net_io_counters(pernic=True)
    iface = state.get("wifi_iface")
    if iface is None or iface not in pernic:
        iface = next((n for n in pernic.keys() if n.startswith("wl")), None)
        state["wifi_iface"] = iface
    if iface and iface in pernic:
        curr_bytes = pernic[iface].bytes_recv
    else:
        curr_bytes = psutil.net_io_counters().bytes_recv

    prev_bytes = state.get("prev_bytes")
    wl_recv = (curr_bytes - prev_bytes) / 1024.0 / SEND_INTERVAL if prev_bytes else 0.0
    state["prev_bytes"] = curr_bytes

    cpu_temp, gpu_temp = get_cpu_gpu_temps(state.get("prev_cpu_temp"), state.get("prev_gpu_temp"))
    state["prev_cpu_temp"], state["prev_gpu_temp"] = cpu_temp, gpu_temp

    metrics = OrderedDict([
        ("cpu", round(cpu, 1)),
        ("ram", round(ram, 1)),
        ("cpu_speed", round(cpu_speed, 2)),
        ("wl_recv", round(wl_recv, 2)),
        ("load_avg_5min", round(load_avg_5min, 2)),
        ("cpu_temp", round(cpu_temp if cpu_temp else 0.0, 1)),
        ("gpu_temp", round(gpu_temp if gpu_temp else 0.0, 1)),
    ])
    return metrics

def main():
    state = {}
    ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
    time.sleep(2)
    while True:
        m = get_metrics(state)
        packet = f"<JSON>{json.dumps(m,separators=(',',':'))}</JSON>\n"
        print("[PY] METRICS:", m)          # ЛОГ 1
        print("[PY] PACKET :", packet)     # ЛОГ 2
        ser.write(packet.encode("utf-8"))
        ser.flush()
        time.sleep(POST_WRITE_DELAY)
        time.sleep(SEND_INTERVAL - POST_WRITE_DELAY)

if __name__ == "__main__":
    main()

