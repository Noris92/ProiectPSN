from flask import Flask, jsonify, request

app = Flask(__name__)

state = {
    "temperature": 25.0,
    "led": False,
    "messages": []
}

@app.route("/")
def index():
    return "Proiect Arduino - Azure placeholder"

@app.route("/api/temperature", methods=["GET", "POST"])
def temperature():
    if request.method == "POST":
        state["temperature"] = request.json.get("value", 0)
        return jsonify(ok=True)
    return jsonify(temperature=state["temperature"])

@app.route("/api/led", methods=["GET", "POST"])
def led():
    if request.method == "POST":
        state["led"] = request.json.get("value", False)
        return jsonify(ok=True)
    return jsonify(led=state["led"])

@app.route("/api/messages", methods=["GET", "POST"])
def messages():
    if request.method == "POST":
        msg = request.json.get("message", "")
        if msg:
            state["messages"] = ([msg] + state["messages"])[:10]
        return jsonify(ok=True)
    return jsonify(messages=state["messages"])

if __name__ == "__main__":
    app.run(debug=True) 
