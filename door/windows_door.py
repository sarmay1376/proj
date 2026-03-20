import eventlet
eventlet.monkey_patch()

import serial
import serial.tools.list_ports
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# --- SETTINGS ---
BAUD_RATE = 115200
WINDOWS_COM_PORT = "AUTO" 

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

def get_port():
    if WINDOWS_COM_PORT != "AUTO": return WINDOWS_COM_PORT
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "Arduino" in p.description or "USB" in p.description or "CH340" in p.description:
            return p.device
    return ports[0].device if ports else None

serial_port = None

def serial_worker():
    global serial_port
    while True:
        port = get_port()
        if not port:
            print("[!] No Arduino found! Retrying...")
            time.sleep(2)
            continue
            
        try:
            print(f"[*] Windows Sync: {port} @ {BAUD_RATE}")
            serial_port = serial.Serial(port, BAUD_RATE, timeout=0.1)
            print(f"[+] Connected to Smart Door on {port}")
            
            while True:
                if serial_port.in_waiting > 0:
                    try:
                        line = serial_port.readline().decode('utf-8', errors='ignore').strip()
                        if not line: continue
                        print(f"DEBUG: {line}")
                        
                        if ":" in line:
                            key, val = line.split(":", 1)
                            socketio.emit('telemetry', {'key': key, 'val': val}, namespace='/')
                        else:
                            socketio.emit('telemetry', {'key': 'RAW', 'val': line}, namespace='/')
                            
                    except Exception as e:
                        print(f"[-] Data Error: {e}")
                        break
                eventlet.sleep(0.01)
        except Exception as e:
            print(f"[-] Connection Error: {e}")
            time.sleep(2)

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('web_unlock')
def handle_web_unlock():
    global serial_port
    if serial_port and serial_port.is_open:
        print("[*] Web Unlock Triggered")
        serial_port.write(b"UNLOCK_CMD\n")
        serial_port.flush()

@socketio.on('web_lock')
def handle_web_lock():
    global serial_port
    if serial_port and serial_port.is_open:
        print("[*] Web Lock Triggered")
        serial_port.write(b"LOCK_CMD\n")
        serial_port.flush()

if __name__ == '__main__':
    eventlet.spawn(serial_worker)
    print("Windows Door Dashboard Ready: http://localhost:5000")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
