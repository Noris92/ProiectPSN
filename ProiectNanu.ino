#include <DHT.h>
#include <EEPROM.h>

#define DHT_PIN 2
#define DHT_TYPE DHT11

#define LED_GREEN 8
#define LED_YELLOW 9
#define LED_RED 10

#define MAX_MESSAGES 10
#define MESSAGE_SIZE 30
#define EEPROM_MESSAGES_START 0

DHT dht(DHT_PIN, DHT_TYPE);

int messageIndex = 0;

unsigned long lastReadTime = 0;
const unsigned long readInterval = 2000;

float temperature = 0.0;
float humidity = 0.0;

bool ledAutomaticEnabled = true;

void turnOffAllLeds() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
}

String getTemperatureStatus(float temp) {
  if (temp < 30) {
    return "NORMAL";
  } 
  else if (temp >= 30 && temp < 35) {
    return "WARNING";
  } 
  else {
    return "ALERT";
  }
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
  } 
  else if (temp >= 30 && temp < 35) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);
    Serial.println("LED_STATE:YELLOW");
  } 
  else {
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
    if (i < message.length()) {
      EEPROM.write(address + i, message[i]);
    } else {
      EEPROM.write(address + i, '\0');
    }
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

    if (c == '\0') {
      break;
    }

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

void setup() {
  Serial.begin(9600);

  dht.begin();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  turnOffAllLeds();

  delay(1000);

  Serial.println("SYSTEM_READY");
  Serial.println("LED_MODE:ON");
}

void loop() {
  if (millis() - lastReadTime >= readInterval) {
    lastReadTime = millis();

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("DHT_ERROR");
    } 
    else {
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
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "LED_ON") {
      ledAutomaticEnabled = true;

      Serial.println("LED_MODE:ON");

      if (!isnan(temperature)) {
        updateLedsByTemperature(temperature);
      }
    }

    else if (command == "LED_OFF") {
      ledAutomaticEnabled = false;

      turnOffAllLeds();

      Serial.println("LED_MODE:OFF");
      Serial.println("LED_STATE:OFF");
    }

    else if (command.startsWith("M:")) {
      String message = command.substring(2);
      saveMessageToEEPROM(message);
    }

    else if (command == "GET_MESSAGES") {
      printStoredMessages();
    }

    else {
      Serial.print("UNKNOWN_COMMAND:");
      Serial.println(command);
    }
  }
}