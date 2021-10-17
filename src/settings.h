#ifndef SETTING_H
#define SETTING_H

#define DEVICE_NAME "GarageLamp"

#define MQTT_TOPIC_LAMP_GET "get/garage/space/lamp"
#define MQTT_TOPIC_LAMP_SET "set/garage/space/lamp"
#define MQTT_TOPIC_DOOR_GET "get/garage/space/door/internal"


#define SWITH_CHECK_INTERVAL 1000

#define PIN_ONBOARD_LED 2          // GPI02
#define PIN_LAMP_RELAY 4           // GPIO4
#define PIN_DOOR_CONTACT_SENSOR 14 // GPI14

#endif // ifndef SETTING_H
