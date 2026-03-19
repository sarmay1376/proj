import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import sys
import glob

# --- CONFIG ---
BAUD_RATE = 9600
MAX_DISTANCE = 150 # cm
SWEEP_RES = 1      # 1 degree resolution
FADE_SPEED = 0.08  # Higher = faster fade (cleant noise)
REVERSE_DIRECTION = False 

def get_port():
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    if ports: return ports[0]
    return '/dev/ttyACM0'

# --- SERIAL CONNECTION ---
try:
    port = get_port()
    ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
    print(f"[*] Connected to Radar on {port}")
except Exception as e:
    print(f"[!] Error: {e}")
    sys.exit()

# --- UI SETUP ---
plt.style.use('dark_background')
fig = plt.figure(figsize=(10, 10), facecolor='#000000')
ax = fig.add_subplot(111, polar=True)
ax.set_facecolor('#000000')

# Setup polar axis
ax.set_theta_zero_location('N')
ax.set_theta_direction(-1) # Clockwise
ax.set_xlim(-np.pi/2, np.pi/2) # 180 degrees
ax.set_ylim(0, MAX_DISTANCE)

# Custom grid
ax.grid(False)
angles = np.linspace(-np.pi/2, np.pi/2, 180)
for r in [50, 100, 150]:
    ax.plot(angles, [r]*len(angles), color='#00ff44', alpha=0.2, linewidth=0.5)
    ax.text(np.pi/2 + 0.1, r, f"{r}cm", color='#00ff44', alpha=0.4, fontsize=8)

ax.set_xticks(np.deg2rad(np.arange(-90, 91, 30)))
ax.set_xticklabels(['90°L', '60°L', '30°L', 'MID', '30°R', '60°R', '90°R'], color='#00ff44', weight='bold')

# --- DATA LAYERS ---
angles_deg = np.arange(0, 181, SWEEP_RES)
data_dist = np.full(len(angles_deg), MAX_DISTANCE, dtype=float)
intensity = np.zeros(len(angles_deg))

# 1. Sweep Beam
beam_line, = ax.plot([0, 0], [0, MAX_DISTANCE], color='#00ff44', linewidth=2, zorder=10)

# 2. Solid Wedges (The "Full" look)
# Using bar plot for filled obstacles
bars = ax.bar(np.deg2rad(angles_deg - 90), data_dist, width=np.deg2rad(SWEEP_RES*2), bottom=0, color='red', alpha=0.0, zorder=5)

# Status Text
status_text = fig.text(0.5, 0.08, "RADAR SCANNING", color='#00ff44', fontsize=20, ha='center', weight='bold', family='monospace')
info_line = fig.text(0.5, 0.04, "Waiting for data...", color='#00d2ff', fontsize=12, ha='center', family='monospace')

def update(frame):
    global intensity, data_dist
    
    # Process ALL available serial data
    while ser.in_waiting > 0:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if ',' in line:
                raw_angle, dist = map(float, line.split(','))
                
                # Apply Direction Mapping
                angle_deg = (180 - raw_angle) if REVERSE_DIRECTION else raw_angle
                angle_rad = np.deg2rad(angle_deg - 90)
                
                # Update Sweep Beam
                beam_line.set_data([angle_rad, angle_rad], [0, MAX_DISTANCE])
                
                # Update state array
                idx = int(angle_deg)
                if 0 <= idx < len(data_dist):
                    if 1 < dist < MAX_DISTANCE:
                        data_dist[idx] = dist
                        intensity[idx] = 1.0 # High intensity
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

    # Update Bars with Fading
    for i, bar in enumerate(bars):
        intensity[i] = max(0, intensity[i] - FADE_SPEED)
        if intensity[i] > 0:
            bar.set_height(data_dist[i])
            bar.set_alpha(intensity[i])
            # Color intensity: bright red for new, dark red for old
            bar.set_color((1.0, 0, 0)) 
        else:
            bar.set_height(0)
            bar.set_alpha(0)

    return beam_line, bars, status_text, info_line

ani = animation.FuncAnimation(fig, update, interval=30, blit=False)
plt.title("PRO-SCAN RADAR v3.0", color='#00ff44', pad=30, fontsize=28, weight='bold', family='monospace')
plt.show()

ser.close()
