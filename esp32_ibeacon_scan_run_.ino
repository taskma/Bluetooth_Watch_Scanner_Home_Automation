#include "config.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <sstream>


BLEScan *pBLEScan;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqtt_server = MQTT_ADDRESS;

const String mqttThisRequest = hostName +  "/request";
const String mqttThisResponse = hostName +  "/response";
const String mqttAllRequest = hostName +  "/getip";
const String mqttAllResponse = hostName +  "/ip";

byte wifiCounter = 0;
byte counter = 0;
bool isFirstTime = true;
//

//


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        
    }
};

void setup()
{
    Serial.begin(115200);
    delay(10);
    pinMode(LedPin, OUTPUT);
    digitalWrite(LedPin, LOW);
    Serial.println();

    Serial.print("Connecting to network: ");
    Serial.println(ssid);
    WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
    WiFi.mode(WIFI_STA);    //init wifi mode
    WiFi.begin(ssid, password); //connect to wifi
    wifiProccess();
    Serial.println("");
    Serial.println("WiFi connected...");
    Serial.println("IP address set: ");
    Serial.println(WiFi.localIP()); //print LAN IP
    digitalWrite(LedPin, HIGH);
    delay(500);
    digitalWrite(LedPin, LOW);

    delay(100);
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(callbackMQTT);
    otaProcess();

    Serial.println("Bluetooth Scanning...");
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); //create new scan
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99); // less or equal setInterval value
}

void loop()
{
    wifiProccess();
    mqttProccess();
    ArduinoOTA.handle();
    // put your main code here, to run repeatedly:
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.print("...................Devices found: ");
    Serial.println(foundDevices.getCount());

    sendDevicesJson(foundDevices);
    printDevices(foundDevices);

    Serial.println("..................Scan done!");
    pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
    delay(2000);
}

void sendDevicesJson(BLEScanResults foundDevices)
{
    std::stringstream ss;
    boolean hasDevice = false;

    int count = foundDevices.getCount();
    ss << "[";
    for (int i = 0; i < count; i++)
    {
        if (hasDevice)
        {
            ss << ",";
        }
        BLEAdvertisedDevice d = foundDevices.getDevice(i);
        ss << "{\"Address\":\"" << d.getAddress().toString() << "\",\"Rssi\":" << d.getRSSI();
        ss << "}";
        hasDevice = true;
        if ((i + 1) % 2 == 0)
        {
            ss << "]";
            sendThisMqResponse(ss.str().c_str());
            delay(50);
            ss.str(std::string());
            ss << "[";
            printy("clear sonrasi", ss.str().c_str());
            hasDevice = false;
        }
    }
    if (hasDevice)
    {
        ss << "]";
        sendThisMqResponse(ss.str().c_str());
    }
}

void sendThisMqResponse(String msg)
{
    printy("mqtt", msg);
    mqttClient.publish(string2char(mqttThisResponse), string2char(msg));
}



void printDevices(BLEScanResults foundDevices)
{
    int count = foundDevices.getCount();
    for (int i = 0; i < count; i++)
    {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        printDevice(device);
    }
}

void printDevice(BLEAdvertisedDevice device)
{
    std::stringstream ss;
    ss << "[";
    ss << "{\"Address\":\"" << device.getAddress().toString() << "\",\"Rssi\":" << device.getRSSI();
    std::stringstream sAdress;
    sAdress << device.getAddress().toString();
    String adress = sAdress.str().c_str();

    if (device.haveName())
    {
        ss << ",\"Name\":\"" << device.getName() << "\"";
    }

    if (device.haveTXPower())
    {
        ss << ",\"TxPower\":" << (int)device.getTXPower();
    }
    if (adress.indexOf("e6:7f:58:d7:eb:09") >= 0)
    {
        ss << ", ** Yigit mi Band 4 **";
    }

    if (adress.indexOf("f8:9b:67:e2:9a:2e") >= 0)
    {
        ss << ", ** Pu mi Band 4 **";
    }

    ss << "}";
    ss << "]";
    Serial.println(ss.str().c_str());
}

void mqttProccess()
{
    if (!mqttClient.connected())
    {
        if (repeatMqttSec >= 60)
        {
            reconnectMQTT();
        }
    }
    else
    {
        mqttClient.loop();
        if (isFirstTime)
        {
            isFirstTime = false;
        }
    }
}

void wifiProccess()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        wifiCounter = 0; //reset counter
        Serial.println("Wifi is still connected with IP: ");
        Serial.println(WiFi.localIP()); //inform user about his IP address
    }
    else if (WiFi.status() != WL_CONNECTED)
    { //if we lost connection, retry
        WiFi.begin(ssid, password);
    }
    while (WiFi.status() != WL_CONNECTED)
    { //during lost connection, print dots
        delay(500);
        Serial.print(".");
        wifiCounter++;
        if (wifiCounter >= 60)
        { //30 seconds timeout - reset board
            Serial.println("Esp Restarting ...");
            delay(1000);
            ESP.restart();
        }
    }
}

void reconnectMQTT()
{
    Serial.println("reconnectMQTT girdi...");
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        // if (mqttClient.connect("BlinkMQTTBridge", mqtt_username, mqtt_password)) {
        if (mqttClient.connect(string2char(hostName)))
        {
            Serial.println("connected");
            repeatMqttSec = 60;
            counter = 0;
            // ... and resubscribe
            digitalWrite(LedPin, HIGH);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 0.3 second");
            ++counter;
            if (counter > 20)
            {
                Serial.println(" reseting..........");
                delay(2000);
                ESP.restart();
            }
            // Wait .3 seconds before retrying
            delay(300);
        }
    }
}

void callbackMQTT(char *topic, byte *payload, unsigned int length)
{
    Serial.println("Message arrived [");
    //Serial.print(topic);
    String topicStr = String(topic);
    String value = "";
    for (int i = 0; i < length; i++)
    {
        char receivedChar = (char)payload[i];
        value += String(receivedChar);
    }
    printy(topicStr, value);
    if (topicStr == mqttAllRequest)
    {
        if (value == "showip")
        {
            Serial.println("showip");
            String messagehostName = hostName + " ==>" + WiFi.localIP().toString();
            mqttClient.publish(string2char(mqttAllResponse), string2char(messagehostName));
        }
    }
}

char *string2char(String command)
{
    if (command.length() != 0)
    {
        char *p = const_cast<char *>(command.c_str());
        //Serial.print("*p ==>");
        //Serial.print(p);
        //Serial.println("<==");
        return p;
    }
}

char *string2char(double val)
{
    //Serial.print("string2char ==> ");
    Serial.println(val);
    char buffer[15];
    dtostrf(val, 5, 2, buffer);
    //Serial.print("buffer ==> ");
    //Serial.println(buffer);
    String temper = String(buffer);
    //Serial.print("temper ==>");
    //Serial.print(temper);
    //Serial.println("<==");
    return buffer;
}

void printy(String p1, String p2)
{
    Serial.print(p1);
    Serial.print(" ==> ");
    Serial.println(p2);
}

void printy(String p1, int p2)
{
    printy(p1, String(p2));
}

void printy(String p1, double p2)
{
    printy(p1, String(p2));
}

void printy(String p1, float p2)
{
    printy(p1, String(p2));
}

void otaProcess()
{
  //OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(string2char(hostName));
  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"xxx");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}
