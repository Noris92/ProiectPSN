from flask import Flask, render_template, request, jsonify
import threading
import time
import random
from datetime import datetime

try:
    import serial
except ImportError:
    serial = None


app = Flask(__name__)

SERIAL_PORT = "COM6"
BAUD_RATE = 9600

FORCE_SIMULATION = False

ser = None
simulation_mode = True
serial_lock = threading.Lock()

state = {
    "temperature": 0.0,
    "humidity": 0.0,
    "temperature_status": "UNKNOWN",
    "led_mode": "ON",
    "led_state": "OFF",
    "messages": [],
    "system_status": "Aplicația pornește...",
    "simulation": True,
    "last_update": "Niciodată"
}


def connect_serial():
    global ser, simulation_mode

    if FORCE_SIMULATION:
        simulation_mode = True
        state["simulation"] = True
        state["system_status"] = "Mod simulare activat manual."
        return

    if serial is None:
        simulation_mode = True
        state["simulation"] = True
        state["system_status"] = "PySerial nu este instalat. Rulez în simulare."
        return

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)

        simulation_mode = False
        state["simulation"] = False
        state["system_status"] = f"Arduino conectat pe {SERIAL_PORT}."

        print(f"Arduino conectat pe {SERIAL_PORT}")

    except Exception as e:
        ser = None
        simulation_mode = True
        state["simulation"] = True
        state["system_status"] = f"Arduino indisponibil. Mod simulare. Eroare: {e}"

        print(f"Arduino indisponibil. Mod simulare. Eroare: {e}")


def send_command(command):
    if simulation_mode or ser is None:
        print(f"[SIMULARE] Comandă: {command}")
        return False

    try:
        with serial_lock:
            ser.write((command + "\n").encode("utf-8"))
        return True

    except Exception as e:
        state["system_status"] = f"Eroare la trimitere comandă: {e}"
        print(f"Eroare serial: {e}")
        return False


def process_serial_line(line):
    line = line.strip()

    if not line:
        return

    print(f"Arduino -> {line}")

    if line == "SYSTEM_READY":
        state["system_status"] = "Arduino pornit și pregătit."

    elif line == "DHT_ERROR":
        state["system_status"] = "Eroare la citirea senzorului DHT11."

    elif line.startswith("TEMP:"):
        try:
            state["temperature"] = float(line.split(":", 1)[1])
            state["last_update"] = datetime.now().strftime("%H:%M:%S")
        except ValueError:
            pass

    elif line.startswith("HUM:"):
        try:
            state["humidity"] = float(line.split(":", 1)[1])
        except ValueError:
            pass

    elif line.startswith("TEMP_STATUS:"):
        state["temperature_status"] = line.split(":", 1)[1]

    elif line.startswith("LED_MODE:"):
        state["led_mode"] = line.split(":", 1)[1]

    elif line.startswith("LED_STATE:"):
        state["led_state"] = line.split(":", 1)[1]

    elif line.startswith("MSG_SAVED:"):
        message = line.split(":", 1)[1]

        if message:
            state["messages"].insert(0, message)
            state["messages"] = state["messages"][:10]

    elif line.startswith("MSG:"):
        parts = line.split(":", 2)

        if len(parts) == 3:
            message = parts[2]

            if message and message not in state["messages"]:
                state["messages"].append(message)
                state["messages"] = state["messages"][:10]


def read_serial_loop():
    while True:
        if simulation_mode:
            state["temperature"] = round(24 + random.uniform(-3, 8), 1)
            state["humidity"] = round(45 + random.uniform(-10, 15), 1)
            state["last_update"] = datetime.now().strftime("%H:%M:%S")

            if state["temperature"] < 30:
                state["temperature_status"] = "NORMAL"
                simulated_led = "GREEN"
            elif state["temperature"] < 35:
                state["temperature_status"] = "WARNING"
                simulated_led = "YELLOW"
            else:
                state["temperature_status"] = "ALERT"
                simulated_led = "RED"

            if state["led_mode"] == "ON":
                state["led_state"] = simulated_led
            else:
                state["led_state"] = "OFF"

            time.sleep(2)
            continue

        try:
            if ser and ser.is_open and ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore")
                process_serial_line(line)

        except Exception as e:
            state["system_status"] = f"Eroare citire serială: {e}"
            print(f"Eroare citire serială: {e}")

        time.sleep(0.1)


@app.route("/")
def index():
    return render_template("index.html", state=state)


@app.route("/status")
def status():
    return jsonify(state)


@app.route("/led-on", methods=["POST"])
def led_on():
    state["led_mode"] = "ON"
    send_command("LED_ON")

    return jsonify({
        "ok": True,
        "state": state
    })


@app.route("/led-off", methods=["POST"])
def led_off():
    state["led_mode"] = "OFF"
    state["led_state"] = "OFF"
    send_command("LED_OFF")

    return jsonify({
        "ok": True,
        "state": state
    })


@app.route("/message", methods=["POST"])
def send_message():
    data = request.get_json()
    message = data.get("message", "").strip()

    if not message:
        return jsonify({
            "ok": False,
            "error": "Mesajul este gol."
        }), 400

    message = message[:29]

    send_command(f"M:{message}")

    state["messages"].insert(0, message)
    state["messages"] = state["messages"][:10]

    return jsonify({
        "ok": True,
        "messages": state["messages"]
    })


@app.route("/refresh-messages", methods=["POST"])
def refresh_messages():
    state["messages"] = []
    send_command("GET_MESSAGES")

    return jsonify({
        "ok": True
    })


if __name__ == "__main__":
    connect_serial()

    thread = threading.Thread(target=read_serial_loop, daemon=True)
    thread.start()

    app.run(debug=False)