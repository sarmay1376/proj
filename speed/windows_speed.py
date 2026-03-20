import eventlet
eventlet.monkey_patch()

import serial
import serial.tools.list_ports
import sys
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# SETTINGS
BAUD_RATE = 9600
WINDOWS_COM_PORT = "AUTO"  # Set to "COM3", "COM4", etc. to override auto-discovery

def get_port():
    """Automatic COM Port detection for Windows Users"""
    # 1. MANUAL OVERRIDE
    if WINDOWS_COM_PORT != "AUTO":
        return WINDOWS_COM_PORT

    # 2. AUTO-SCAN
    ports = list(serial.tools.list_ports.comports())
    if not ports: return None
    
    # Priority for typical Arduino clones
    for p in ports:
        if "USB" in p.description or "CH340" in p.description or "Arduino" in p.description:
            return p.device
            
    return ports[0].device

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

@app.route('/')
def index():
    return render_template('index.html')

def serial_worker():
    ser = None
    while True:
        port = get_port()
        if not port:
            print("[!] No COM Port found! Is the Nano plugged in?")
            time.sleep(2)
            continue

        print(f"[*] Windows Syncing: {port} @ {BAUD_RATE}")
        try:
            if ser is not None:
                try: ser.close()
                except: pass

            ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            ser.flush()
            print(f"[+] SYNCED on {port}")
            
            while True:
                try:
                    if ser.in_waiting > 0:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if not line: continue
                        print(f"[{port}] {line}")
                        
                        if line.startswith("SPEED:"):
                            parts = line.split(":")
                            if len(parts) >= 5:
                                kmh = parts[2]
                                ms = parts[4]
                                socketio.emit('speed_update', {'speed': ms, 'unit': 'm/s'}, namespace='/')
                                socketio.emit('speed_update', {'speed': kmh, 'unit': 'km/h'}, namespace='/')
                        elif line.startswith("STATE:"):
                            state = line.split(":")[1]
                            socketio.emit('state_update', {'state': state}, namespace='/')
                        elif "DEBUG_DIST" in line:
                            socketio.emit('debug_data', {'info': line}, namespace='/')
                    
                    eventlet.sleep(0.01)
                    
                except Exception as inner_e:
                    print(f"[*] Port disconnected: {inner_e}")
                    break
            
            ser.close()

        except Exception as e:
            print(f"[!] PORT BUSY on {port}: {e}")
            print("[?] Close the Arduino IDE Serial Monitor!")
            time.sleep(2)

if __name__ == '__main__':
    eventlet.spawn(serial_worker)
    print("Windows Dashboard Bridge Ready: http://localhost:5000")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
