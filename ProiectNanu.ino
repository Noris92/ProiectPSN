#include <DHT.h>
#include <EEPROM.h>

#define DHT_PIN 2
#define DHT_TYPE DHT11

#define LED_GREEN 9
#define LED_YELLOW 10
#define LED_RED 8

#define FLOOD_PIN A0
#define FLOOD_THRESHOLD 300

#define MAX_MESSAGES 10
#define MESSAGE_SIZE 30
#define EEPROM_MESSAGES_START 0

#define EEPROM_FLOOD_COUNT 300
#define EEPROM_FLOOD_STATE 301

DHT dht(DHT_PIN, DHT_TYPE);

int messageIndex = 0;
unsigned long lastReadTime = 0;
const unsigned long readInterval = 2000;

float temperature = 0.0;
float humidity = 0.0;
bool ledAutomaticEnabled = true;
bool floodDetected = false;

void turnOffAllLeds() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
}

String getTemperatureStatus(float temp) {
  if (temp < 30) return "NORMAL";
  else if (temp < 35) return "WARNING";
  else return "ALERT";
}

void updateLedsByTemperature(float temp) {
  if (!ledAutomaticEnabled) {
    turnOffAllLeds();
    Serial.println("LED_STATE:OFF");
    return;
  }
  if (temp < 30) {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
    Serial.println("LED_STATE:GREEN");
  } else if (temp < 35) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);
    Serial.println("LED_STATE:YELLOW");
  } else {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
    Serial.println("LED_STATE:RED");
  }
}

void saveMessageToEEPROM(String message) {
  if (message.length() > MESSAGE_SIZE - 1) {
    message = message.substring(0, MESSAGE_SIZE - 1);
  }
  int address = EEPROM_MESSAGES_START + (messageIndex % MAX_MESSAGES) * MESSAGE_SIZE;
  for (int i = 0; i < MESSAGE_SIZE; i++) {
    EEPROM.write(address + i, i < (int)message.length() ? message[i] : '\0');
  }
  Serial.print("MSG_SAVED:");
  Serial.println(message);
  messageIndex++;
}

String readMessageFromEEPROM(int index) {
  int address = EEPROM_MESSAGES_START + index * MESSAGE_SIZE;
  String result = "";
  for (int i = 0; i < MESSAGE_SIZE; i++) {
    char c = EEPROM.read(address + i);
    if (c == '\0') break;
    result += c;
  }
  return result;
}

void printStoredMessages() {
  Serial.println("MESSAGES_BEGIN");
  for (int i = 0; i < MAX_MESSAGES; i++) {
    String msg = readMessageFromEEPROM(i);
    if (msg.length() > 0) {
      Serial.print("MSG:");
      Serial.print(i);
      Serial.print(":");
      Serial.println(msg);
    }
  }
  Serial.println("MESSAGES_END");
}

void saveFloodEventToEEPROM() {
  byte count = EEPROM.read(EEPROM_FLOOD_COUNT);
  if (count > 250) count = 0;
  count++;
  EEPROM.write(EEPROM_FLOOD_COUNT, count);
  EEPROM.write(EEPROM_FLOOD_STATE, 1);
}

void checkFloodSensor() {
  int sensorValue = analogRead(FLOOD_PIN);
  bool currentFlood = (sensorValue > FLOOD_THRESHOLD) && (sensorValue < 1023);

  if (currentFlood && !floodDetected) {
    floodDetected = true;
    saveFloodEventToEEPROM();
    byte count = EEPROM.read(EEPROM_FLOOD_COUNT);
    Serial.print("FLOOD_DETECTED:");
    Serial.println(count);
  } else if (!currentFlood && floodDetected) {
    floodDetected = false;
    EEPROM.write(EEPROM_FLOOD_STATE, 0);
    Serial.println("FLOOD_CLEAR");
  }
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  turnOffAllLeds();
  EEPROM.write(EEPROM_FLOOD_STATE, 0);
  delay(1000);
  Serial.println("SYSTEM_READY");
  Serial.println("LED_MODE:ON");
}

void loop() {
  if (millis() - lastReadTime >= readInterval) {
    lastReadTime = millis();

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity) || temperature < -10 || temperature > 60) {
      Serial.println("DHT_ERROR");
    } else {
      Serial.print("TEMP:");
      Serial.println(temperature);
      Serial.print("HUM:");
      Serial.println(humidity);
      Serial.print("TEMP_STATUS:");
      Serial.println(getTemperatureStatus(temperature));
      Serial.print("LED_MODE:");
      Serial.println(ledAutomaticEnabled ? "ON" : "OFF");
      updateLedsByTemperature(temperature);
    }

    checkFloodSensor();
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "LED_ON") {
      ledAutomaticEnabled = true;
      Serial.println("LED_MODE:ON");
      if (!isnan(temperature)) updateLedsByTemperature(temperature);
    } else if (command == "LED_OFF") {
      ledAutomaticEnabled = false;
      turnOffAllLeds();
      Serial.println("LED_MODE:OFF");
      Serial.println("LED_STATE:OFF");
    } else if (command.startsWith("M:")) {
      String message = command.substring(2);
      saveMessageToEEPROM(message);
    } else if (command == "GET_MESSAGES") {
      printStoredMessages();
    } else if (command == "GET_FLOOD_COUNT") {
      byte count = EEPROM.read(EEPROM_FLOOD_COUNT);
      Serial.print("FLOOD_COUNT:");
      Serial.println(count);
    } else {
      Serial.print("UNKNOWN_COMMAND:");
      Serial.println(command);
    }
  }
}
