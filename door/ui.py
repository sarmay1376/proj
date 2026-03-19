import eventlet
eventlet.monkey_patch()

import serial
import serial.tools.list_ports
import time
from flask import Flask, render_template
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# --- SERIAL CONFIGURATION ---
BAUD_RATE = 115200
import threading
serial_lock = threading.Lock()

def get_arduino_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "Arduino" in port.description or "ACM" in port.device or "USB" in port.device:
            return port.device
    return None

serial_port = None

def serial_background_thread():
    global serial_port
    while True:
        port_name = get_arduino_port()
        if not port_name:
            print("[!] Arduino not found. Retrying in 2s...")
            eventlet.sleep(2)
            continue
            
        try:
            print(f"[*] Connecting to {port_name}...")
            serial_port = serial.Serial(port_name, BAUD_RATE, timeout=0.1)
            print(f"[+] Connected to Smart Door on {port_name}")
            
            while True:
                if serial_port.in_waiting > 0:
                    try:
                        line = serial_port.readline().decode('utf-8', errors='ignore').strip()
                        if not line: continue
                        
                        print(f"DEBUG: {line}")
                        
                        if ":" in line:
                            key, val = line.split(":", 1)
                            socketio.emit('telemetry', {'key': key, 'val': val})
                        else:
                            socketio.emit('telemetry', {'key': 'RAW', 'val': line})
                            
                    except Exception as e:
                        print(f"[-] Data Error: {e}")
                        break
                eventlet.sleep(0.01)
        except Exception as e:
            print(f"[-] Connection Error: {e}")
            eventlet.sleep(2)

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('web_unlock')
def handle_web_unlock():
    global serial_port
    if serial_port and serial_port.is_open:
        with serial_lock:
            print("[*] Web Unlock Triggered")
            serial_port.write(b"UNLOCK_CMD\n")
            serial_port.flush()

@socketio.on('web_lock')
def handle_web_lock():
    global serial_port
    if serial_port and serial_port.is_open:
        with serial_lock:
            print("[*] Web Lock Triggered")
            serial_port.write(b"LOCK_CMD\n")
            serial_port.flush()

if __name__ == '__main__':
    eventlet.spawn(serial_background_thread)
    print("Dashboard listening on http://localhost:5000")
    socketio.run(app, host='0.0.0.0', port=5000)
