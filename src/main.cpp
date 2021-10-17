#include <Arduino.h>
#include <ArduinoJson.h>
#include <MqttClient.h>
#include <FotaClient.h>
#include <ESPWifiClient.h>
#include <RemotePrint.h>
#include <OneButton.h>
#include "settings.h"

MqttClient *mqttClient    = NULL;
FotaClient *fotaClient    = new FotaClient(DEVICE_NAME);
ESPWifiClient *wifiClient = new ESPWifiClient(WIFI_SSID, WIFI_PASS);

OneButton doorSwitch(PIN_DOOR_CONTACT_SENSOR, true);
unsigned long lampLastStatusMsgSentAt = 0;
bool isLampPoweredOn                  = false;
bool isDoorStateOpened                = false;

unsigned long publishStatus(const char   *topic,
                            bool          isPoweredOn,
                            unsigned long lastStatusMsgSentAt,
                            bool          forcePublish = false,
                            const char   *messageId    = NULL) {
  unsigned long now = millis();

  if ((!forcePublish) and (now - lastStatusMsgSentAt < MQTT_PUBLISH_STATUS_INTERVAL)) {
    return lastStatusMsgSentAt;
  }
  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2);
  DynamicJsonDocument root(capacity);
  JsonObject status = root.createNestedObject("status");

  if (messageId != NULL) {
    root["messageId"] = messageId;
  }

  status["powerOn"] = isPoweredOn;

  // convert to String
  String outString;
  serializeJson(root, outString);

  // publish the message
  mqttClient->publish(topic, outString, true);
  return now;
}

void lampPublishStatus(bool        forcePublish = false,
                       const char *messageId    = NULL) {
  lampLastStatusMsgSentAt = publishStatus(MQTT_TOPIC_LAMP_GET,
                                          isLampPoweredOn,
                                          lampLastStatusMsgSentAt,
                                          forcePublish,
                                          messageId);
}

void switchPublishStatus() {
  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2);
  DynamicJsonDocument root(capacity);
  JsonObject status = root.createNestedObject("status");

  if (isDoorStateOpened) {
    status["door"] = "opened";
  } else {
    status["door"] = "closed";
  }

  // convert to String
  String outString;
  serializeJson(root, outString);

  // publish the message
  mqttClient->publish(MQTT_TOPIC_DOOR_GET, outString, true);
}

void setOnboardLed(bool on) {
  if (on) {
    digitalWrite(PIN_ONBOARD_LED, LOW);
  } else {
    digitalWrite(PIN_ONBOARD_LED, HIGH);
  }
}

void setLampPowerStatus(bool isOn) {
  if (isOn) {
    digitalWrite(PIN_LAMP_RELAY, HIGH);
  } else {
    digitalWrite(PIN_LAMP_RELAY, LOW);
  }
  isLampPoweredOn = isOn;
  setOnboardLed(isLampPoweredOn);
}

void lampHandleRequest(String payload) {
  // deserialize the payload to JSON
  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 70;
  DynamicJsonDocument  jsonDoc(capacity);
  DeserializationError error = deserializeJson(jsonDoc, payload);

  if (error) {
    PRINT_E("Failed to deserialize the received payload. Error: ");
    PRINTLN_E(error.c_str());
    PRINTLN_E("The payload is: ");
    PRINTLN_E(payload)
    return;
  }
  JsonObject root   = jsonDoc.as<JsonObject>();
  JsonObject status = root["status"];

  if (status.isNull()) {
    PRINTLN_E(
      "LAMP: The received payload is valid JSON, but \"status\" key is not found.");
    PRINTLN_E("The payload is: ");
    PRINTLN_E(payload)
    return;
  }

  JsonVariant powerOnJV = status["powerOn"];

  if (!powerOnJV.isNull()) {
    bool isLampPoweredOnNew = powerOnJV.as<bool>();

    if (isLampPoweredOnNew == isLampPoweredOn) {
      PRINTLN("LAMP: No need to set the state, as it is already set.");
      return;
    }
    setLampPowerStatus(isLampPoweredOnNew);
  }
  const char *messageId = root["messageId"];
  lampPublishStatus(true, messageId);
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  PRINT("MQTT Message arrived [");
  PRINT(topic);
  PRINTLN("] ");

  // Convert the payload to string
  char spayload[length + 1];
  memcpy(spayload, payload, length);
  spayload[length] = '\0';
  String payloadString = String(spayload);

  // Do something according the topic
  if (strcmp(topic, MQTT_TOPIC_LAMP_SET) == 0) {
    lampHandleRequest(payloadString);
  } else {
    PRINT("MQTT: Warning: Unknown topic: ");
    PRINTLN(topic);
  }
}

void doorSwitchClosed() {
  PRINTLN("GARAGE: Door closed.");
  isDoorStateOpened = false;
  setLampPowerStatus(false);
  switchPublishStatus();
}

void doorSwitchOpened() {
  PRINTLN("GARAGE: Door opened.");
  isDoorStateOpened = true;
  setLampPowerStatus(true);
  switchPublishStatus();
}

void setup() {
  // Lamp relay
  pinMode(PIN_LAMP_RELAY, OUTPUT);
  digitalWrite(PIN_LAMP_RELAY, LOW);

  // Onboard LED
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  digitalWrite(PIN_ONBOARD_LED, HIGH);

  // Door contact sensor
  pinMode(PIN_DOOR_CONTACT_SENSOR, INPUT_PULLUP);

  doorSwitch.attachClick(doorSwitchClosed);
  doorSwitch.attachLongPressStart(doorSwitchClosed);
  doorSwitch.attachLongPressStop(doorSwitchOpened);

  doorSwitch.tick();

  wifiClient->init();
  fotaClient->init();
  mqttClient = new MqttClient(MQTT_SERVER,
                              MQTT_SERVER_PORT,
                              DEVICE_NAME,
                              MQTT_USERNAME,
                              MQTT_PASS,
                              MQTT_TOPIC_LAMP_SET,
                              MQTT_SERVER_FINGERPRINT,
                              mqttCallback);
}

void loop() {
  doorSwitch.tick();
  wifiClient->reconnectIfNeeded();
  RemotePrint::instance()->handle();
  fotaClient->loop();
  mqttClient->loop();

  lampPublishStatus();
}
