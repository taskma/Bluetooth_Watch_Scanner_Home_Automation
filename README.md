# ESP32 Bluetooth Watch Scanner → MQTT (Presence / Proximity)

Scans BLE beacons (iBeacon-like), reads RSSI, and publishes `{MAC, RSSI}` to MQTT.  
Useful for simple presence/proximity automation in home setups.

## Hardware
- ESP32
- BLE beacon source (watch / tag)
- Wi-Fi + MQTT broker

## Configure
Edit `config.h`:
- Wi-Fi SSID / password
- MQTT host / port
- Publish topic

## MQTT payload
Example:
- topic: `home/presence/ble`
- payload: `{"mac":"AA:BB:CC:DD:EE:FF","rssi":-62}`

## Safety & privacy
Do not track people without consent.

---

## Bluetooth Watch Scanner with ESP32 for Home Automation (Beacon Mode)
- Many Bluetooth watches can broadcast beacon signals (iBeacon-like).
- This project captures those signals, reads RSSI (received signal strength),
  and publishes the watch MAC address + RSSI to MQTT.
- You can consume these MQTT messages to estimate proximity (distance approximation)
  and trigger automations (e.g., turn lights on/off when someone enters/leaves a room).


![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/esp32.jpeg)
![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/beacon.jpg)
![alt text](https://github.com/taskma/Bluetooth_Watch_Scanner_Home_Automation/blob/master/lamp.jpeg)
