# ESP32 Bluetooth Watch Scanner → MQTT (Presence / Proximity)

Scans BLE beacons (iBeacon-like), reads RSSI, and publishes {MAC, RSSI} to MQTT.

## Hardware
- ESP32
- BLE beacon source (watch / tag)
- Wi-Fi + MQTT broker

## Configure
Edit `config.h`:
- Wi-Fi SSID/password
- MQTT host/port
- publish topic

## MQTT Payload
Example:
topic: home/presence/ble
payload: {"mac":"AA:BB:CC:DD:EE:FF","rssi":-62}

## Safety / Privacy
Do not track people without consent.


# Bluetooth Watch Scanner with ESP32 for Home Automation (Beacon Mode)

* You can use all of the bluetooth watches
* All of the bluetooth watches are sending beacon signals like ibeacon
* This code captures this signals and detect RSSI (The received signal strength)
* Sends Bluetooth Watch MAC adress and RSSI to MQTT 
* You can read this MQ mesages and detect distance between watch and ESP32 receiver
* You can decide to anyone is in the room or no and you can turn on or turn off the lights in the room


![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/esp32.jpeg)
![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/beacon.jpg)
![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/lamp.jpeg)
