import eventlet
eventlet.monkey_patch()

import serial
import sys
import glob
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# EMERGENCY STABILITY: 9600 BAUD
BAUD_RATE = 9600

def get_port():
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    if ports:
        return ports[0]
    return 'COM3' if sys.platform.startswith('win') else '/dev/ttyACM0'

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

def serial_worker():
    while True:
        port = get_port()
        print(f"[*] Connecting: {port} @ {BAUD_RATE}")
        try:
            # Use slightly longer timeout for terminal stability
            ser = serial.Serial(port, BAUD_RATE, timeout=0.5)
            ser.flush()
            print(f"[+] Synced with {port}")
            
            while True:
                try:
                    if ser.in_waiting > 0:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if not line: continue
                        
                        # Print EVERYTHING for the presentation terminal
                        print(f"[HW] {line}")
                        
                        if line.startswith("STATE:"):
                            socketio.emit('state_update', {'state': line.split(":")[1]}, namespace='/')
                        elif line.startswith("SPEED_MS:"):
                            socketio.emit('speed_update', {'speed': line.split(":")[1], 'unit': 'm/s'}, namespace='/')
                        elif line.startswith("SPEED_KMH:"):
                            socketio.emit('speed_update', {'speed': line.split(":")[1], 'unit': 'km/h'}, namespace='/')
                        elif line.startswith("DEBUG_DIST"):
                            # Send debug distances to dash for live troubleshooting
                            socketio.emit('debug_data', {'info': line}, namespace='/')
                        elif line.startswith("ALERT:"):
                            socketio.emit('speed_alert', {'type': 'OVERSPEED'}, namespace='/')
                    
                    eventlet.sleep(0.01)
                    
                except (OSError, serial.SerialException):
                    break # Port vanished
            ser.close()
        except:
            eventlet.sleep(2) # Retry

@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    eventlet.spawn(serial_worker)
    print("Dashboard Ready: http://localhost:5000")
    socketio.run(app, host='0.0.0.0', port=5000, log_output=False)
