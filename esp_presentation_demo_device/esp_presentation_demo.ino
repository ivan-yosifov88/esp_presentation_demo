#include <ESP8266WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Home network
const char* ssid = "";
const char* password = "";

const char* mqtt_server = "";
const char* mqtt_username = "esp8266";
const char* mqtt_password = "mqtt_very666Secret_p@ss"; 

const char* mqtt_topic_led_control = "/led/control";
const char* mqtt_topik_led_state = "/led/state";
const char* mqtt_topic_dht_sensor = "/dht_sensor";
const char* mqtt_topic_potentiometer = "/potentiometer";

const char* device_name = "ESP8266Client";


DHT dht(4, DHT22); // D4 pin and the sensor is DHT22

// There is an issue with the naming of the digital inputs so i get them from rsp chip
// and so the led lights wil be like D5, D6, D7 but theirs values will be like {14, 12, 13}

struct ColorPin {
  String color;
  int pin;
};

// Define the color to pin mappings
ColorPin colorPins[] = {
  {"green", 14},
  {"yellow", 12},
  {"red", 13}
};

const int colorPinCount = sizeof(colorPins) / sizeof(colorPins[0]);
int getPinForColor(String color) {
  for (int i = 0; i < colorPinCount; i++) {
    if (colorPins[i].color == color) {
      return colorPins[i].pin;
    }
  }
  return -1; // Return an invalid pin number if color is not found
}


int lastPotValue = -1; // Store the last sent potentiometer value
const int potentiometerPin = A0;



WiFiClient espClient;
PubSubClient client(espClient);



void setup() {
  Serial.begin(115200);
  dht.begin();
  initializeLeds();

  connectToWiFi();
  connectToMQTTBroker();
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    connectToMQTTBroker();
  }
  
  client.loop();
  publishDHTSensorData();
  publishPotentiometerValue();
}



void connectToWiFi() {
  WiFi.hostname(device_name);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


// Function to connect to MQTT broker 
void connectToMQTTBroker() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(device_name, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_led_control);
       for (int i = 0; i < colorPinCount; i++) {
        bool currentState = digitalRead(colorPins[i].pin);
        publishLEDState(colorPins[i].color.c_str(), currentState);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
    // Convert the incoming byte array to a string
    payload[length] = '\0'; // Null-terminate the array to create a valid string
    String message = String((char*)payload);

    // Example to check the topic and take action
    if (String(topic) == mqtt_topic_led_control) {
        StaticJsonDocument<200> doc; // Create a JSON document for sending state information
        String state;
        if (message == "animate") {
            animateLeds();
        } else {
            int ledPin = getPinForColor(message);
            if (ledPin != -1) {
              bool currentState = digitalRead(ledPin);
              
              digitalWrite(ledPin, !currentState);
              publishLEDState(message.c_str(), !currentState);
            } else {
                Serial.println("Color not found!");
            }
        }
    }
}

void publishLEDState(const char* color, bool state) {
  StaticJsonDocument<200> doc;
  doc["color"] = color;
  doc["state"] = state ? "ON" : "OFF";
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_topik_led_state, jsonBuffer);
}

void publishDHTSensorData() {
    if (client.connected()) {
        StaticJsonDocument<200> doc; 
        // char msg[50];
        float humidity = dht.readHumidity();
        float temperature = dht.readTemperature();
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        char jsonBuffer[512];
        serializeJson(doc, jsonBuffer);
        // snprintf(msg, 50, "Humidity: %.2f%% Temp: %.2fC", humidity, temperature);
        client.publish(mqtt_topic_dht_sensor, jsonBuffer);
    }
}

void publishPotentiometerValue() {
    // char msg[50];
    StaticJsonDocument<200> doc; 
    int potentiometerRawValue = analogRead(potentiometerPin);
    int scaledPotentiometerValue = map(potentiometerRawValue, 0, 1023, 0, 100);
    // Check if the current value is different from the last sent value
    if (scaledPotentiometerValue != lastPotValue) {
      if (client.connected()) {
        // The value has changed, so publish the new value
        // snprintf(msg, 50, "Potentiometer value: %d", scaledPotentiometerValue);
        doc["potentiometer"] = scaledPotentiometerValue;

        char jsonBuffer[512];
        serializeJson(doc, jsonBuffer);
        client.publish(mqtt_topic_potentiometer, jsonBuffer);
      }
        // Update the last sent value to the current value  
        lastPotValue = scaledPotentiometerValue;
    }
}

void initializeLeds() {
  for (int i = 0; i < colorPinCount; i++) {
    pinMode(colorPins[i].pin, OUTPUT); // Use the pin field of each ColorPin
    Serial.print(colorPins[i].color + " LED pin initialized: ");
    Serial.println(colorPins[i].pin);
  }
}

void animateLeds() {
  // Turn all LEDs on, then off, with a delay
  for (int i = 0; i < colorPinCount; i++) {
    digitalWrite(colorPins[i].pin, HIGH);
    delay(500);
  }
  delay(500);
  for (int i = colorPinCount - 1; i >= 0; i--) {
    digitalWrite(colorPins[i].pin, LOW);
    delay(500);
  }
}