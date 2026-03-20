import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import sys
import time

# --- CONFIG ---
BAUD_RATE = 9600
MAX_DISTANCE = 150 
SWEEP_RES = 1      
FADE_SPEED = 0.08  
REVERSE_DIRECTION = False 
WINDOWS_COM_PORT = "AUTO"

def get_port():
    if WINDOWS_COM_PORT != "AUTO": return WINDOWS_COM_PORT
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "Arduino" in p.description or "USB" in p.description or "CH340" in p.description:
            return p.device
    return ports[0].device if ports else None

# --- SERIAL CONNECTION ---
port = get_port()
if not port:
    print("[!] Error: No Arduino found! Is it plugged in?")
    sys.exit()

try:
    ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
    print(f"[*] Connected to Radar on Windows: {port}")
except Exception as e:
    print(f"[!] Error: {e}")
    sys.exit()

# --- UI SETUP ---
plt.style.use('dark_background')
fig = plt.figure(figsize=(10, 10), facecolor='#000000')
ax = fig.add_subplot(111, polar=True)
ax.set_facecolor('#000000')

ax.set_theta_zero_location('N')
ax.set_theta_direction(-1) 
ax.set_xlim(-np.pi/2, np.pi/2) 
ax.set_ylim(0, MAX_DISTANCE)

ax.grid(False)
angles = np.linspace(-np.pi/2, np.pi/2, 180)
for r in [50, 100, 150]:
    ax.plot(angles, [r]*len(angles), color='#00ff44', alpha=0.2, linewidth=0.5)
    ax.text(np.pi/2 + 0.1, r, f"{r}cm", color='#00ff44', alpha=0.4, fontsize=8)

ax.set_xticks(np.deg2rad(np.arange(-90, 91, 30)))
ax.set_xticklabels(['90°L', '60°L', '30°L', 'MID', '30°R', '60°R', '90°R'], color='#00ff44', weight='bold')

angles_deg = np.arange(0, 181, SWEEP_RES)
data_dist = np.full(len(angles_deg), MAX_DISTANCE, dtype=float)
intensity = np.zeros(len(angles_deg))

beam_line, = ax.plot([0, 0], [0, MAX_DISTANCE], color='#00ff44', linewidth=2, zorder=10)
bars = ax.bar(np.deg2rad(angles_deg - 90), data_dist, width=np.deg2rad(SWEEP_RES*2), bottom=0, zorder=5)

status_text = fig.text(0.5, 0.08, "RADAR SCANNING", color='#00ff44', fontsize=20, ha='center', weight='bold', family='monospace')
info_line = fig.text(0.5, 0.04, "Waiting for data...", color='#00d2ff', fontsize=12, ha='center', family='monospace')

def update(frame):
    global intensity, data_dist
    while ser.in_waiting > 0:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if ',' in line:
                raw_angle, dist = map(float, line.split(','))
                angle_deg = (180 - raw_angle) if REVERSE_DIRECTION else raw_angle
                angle_rad = np.deg2rad(angle_deg - 90)
                beam_line.set_data([angle_rad, angle_rad], [0, MAX_DISTANCE])
                idx = int(angle_deg)
                if 0 <= idx < len(data_dist):
                    if 1 < dist < MAX_DISTANCE:
                        data_dist[idx] = dist
                        intensity[idx] = 1.0 
                        status_text.set_text("OBJECT DETECTED")
                        status_text.set_color('red')
                        info_line.set_text(f"DISTANCE: {int(dist)} cm | ANGLE: {int(angle_deg)}°")
                    else:
                        data_dist[idx] = MAX_DISTANCE
                        intensity[idx] = 0.0
                        status_text.set_text("SCANNING CLEAR")
                        status_text.set_color('#00ff44')
        except Exception:
            continue

    for i, bar in enumerate(bars):
        intensity[i] = max(0, intensity[i] - FADE_SPEED)
        if intensity[i] > 0:
            bar.set_height(data_dist[i])
            bar.set_alpha(intensity[i])
            bar.set_color((1.0, 0, 0)) 
        else:
            bar.set_height(0)
            bar.set_alpha(0)
    return beam_line, bars, status_text, info_line

ani = animation.FuncAnimation(fig, update, interval=30, blit=False)
plt.title("PRO-SCAN RADAR (WINDOWS)", color='#00ff44', pad=30, fontsize=28, weight='bold', family='monospace')
plt.show()
ser.close()
