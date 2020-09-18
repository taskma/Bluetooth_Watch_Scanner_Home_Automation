// Your Wifi SSID.
#define WIFI_SSID "?"

// Wifi password.
#define WIFI_PASSWORD "?"

// MQTT server details.
#define MQTT_ADDRESS "192.168.0.28"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

//HostName
const String hostName = "beacon_scanner1";

//Mqtt reconnect time
byte repeatMqttSec = 60;
byte scanTime = 5; //In seconds

//LED
#define LedPin 2
