import json

from flask import Flask, request, jsonify, render_template
import paho.mqtt.client as mqtt

app = Flask(__name__)

MQTT_BROKER = 'localhost'
MQTT_TOPIC_LED_CONTROL = "/led/control"
MQTT_TOPIC_LED_STATE = "/led/state"
MQTT_TOPIC_POTENTIOMETER_VALUE = "/potentiometer"
MQTT_TOPIC_DHT_SENSOR = "/dht_sensor"
MQTT_USERNAME = "esp8266"
MQTT_PASSWORD = "mqtt_very666Secret_p@ss"

# Store the potentiometer value
potentiometer = "N/A"
temperature = "N/A"
humidity = "N/A"
led_states = {
    "green": False,
    "yellow": False,
    "red": False,
}


def on_connect(client, userdata, flags, rc):
    client.subscribe(MQTT_TOPIC_LED_STATE)
    client.subscribe(MQTT_TOPIC_POTENTIOMETER_VALUE)
    client.subscribe(MQTT_TOPIC_DHT_SENSOR)


def on_message(client, userdata, msg):
    global potentiometer, temperature, humidity, led_states
    payload_str = msg.payload.decode()
    try:
        data = json.loads(payload_str)
        if 'temperature' in data:
            temperature = f"{data['temperature']:.1f}"
        if 'humidity' in data:
            humidity = f"{data['humidity']:.1f}"
        if 'potentiometer' in data:
            potentiometer = data['potentiometer']

        if 'color' in data and 'state' in data:
            print(data)
            color = data['color']
            state = data['state']
            led_states[color] = True if state == 'ON' else False
    except json.JSONDecodeError:
        # If JSON decoding fails, process the payload as plain text
        print(f"Received plain text message: {payload_str}")


mqtt_cli = mqtt.Client()
mqtt_cli.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
mqtt_cli.connect(host=MQTT_BROKER)
mqtt_cli.on_connect = on_connect
mqtt_cli.on_message = on_message
mqtt_cli.loop_start()


@app.route('/')
def hello_world():
    return render_template('index.html')


def mqtt_cli_publish_message(topic, message):
    try:
        mqtt_cli.publish(topic, message)
        return jsonify({"success": True}), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500


# Route to toggle LEDs
@app.route('/control/led', methods=['POST'])
def control_led():
    color = request.json.get('color')
    return mqtt_cli_publish_message(MQTT_TOPIC_LED_CONTROL, color)


# Route to trigger animation
@app.route('/control/animate', methods=['POST'])
def animate():
    return mqtt_cli_publish_message(MQTT_TOPIC_LED_CONTROL, 'animate')


@app.route('/get-state')
def get_sensor_data():
    sensors_data = {
        "potentiometer": potentiometer,
        "temperature": temperature,
        "humidity": humidity
    }
    sensors_data.update(led_states)
    return jsonify(sensors_data)


@app.route('/get-led-state')
def get_led_state():
    return jsonify(led_states)


if __name__ == '__main__':
    # sudo apt-get update
    # sudo apt-get install mosquitto mosquitto-clients
    # mosquitto      // this command will run the broker
    # /etc/mosquitto/mosquitto.conf
    # edit file and add line    listener 1883
    # add my user and pass in mosquitto sudo mosquitto_passwd -b /etc/mosquitto/passwd <username> <password>
    app.run()
