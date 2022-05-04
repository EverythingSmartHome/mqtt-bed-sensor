# mqtt-bed-sensor
MQTT Bed Sensor/Scales for bed occupancy (although entirely adaptable for anything), for integrating with Home Assistant, OpenHAB, Domoticz and anything else supporting MQTT.

Has remote tare function which you can issue over MQTT if your sensor suffers from drift which many load cells seem to. This saves having to re-start the device everytime you want to tare. You can also remotely adjust the calibration factor and reboot the device via MQTT. The firmware offers a web interface for configuration as well.

Full guide available [here](https://everythingsmarthome.co.uk/howto/building-a-bed-occupancy-sensor-for-home-assistant/)

## Requirements
### Hardware
- [HX711 Amplifier](https://amzn.to/2POYBzH)
- [Load Cells](https://amzn.to/2PQunMC)
- ESP8266 based board with 5v output (Wemos D1 Mini etc)

### Libraries
- [HX711](https://github.com/bogde/HX711) library - available through Arduino IDE library manager
- [PubSubClient](https://github.com/knolleary/pubsubclient) library - available through Arduino IDE library manager
- [ESP boards](https://github.com/esp8266/Arduino)
- [ESP8266 WiFi](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi) library - available through Arduino IDE library manager
- [LittleFS](https://github.com/esp8266/Arduino/tree/master/libraries/LittleFS) library - available through Arduino IDE library manager. Make sure to select the ESP8266 library, not the ESP32.
- [Vector](https://github.com/janelia-arduino/Vector) library - available through Arduino IDE library manager
- [WiFiWebServer](https://github.com/khoih-prog/WiFiWebServer) library - available through Arduino IDE library manager
- [ArduinoJSON](https://arduinojson.org) library - available through Arduino IDE library manager

### Extensions
- [ESP8266 LittleFS Data Upload](https://randomnerdtutorials.com/install-esp8266-nodemcu-littlefs-arduino/#installing) - Follow the installation instructions on this page, then restart your Arduino IDE.

## Use case
This was built specifically with bed occupancy in mind so that automations can be triggered when in bed such as switching lights off, locking doors, setting alarms etc etc using Home Assistant. I found that load cells seem to "drift" a little over time, with this in mind a remote tare function was built in so that I can issue a tare command over MQTT which will reset it to zero without having to physically reset the device everytime.

## Installation
1. Load sketch to the device
2. Load the required files to the device by clicking the "ESP8266 LittleFS Data Upload" option in the "Tools" menu
3. Without a valid WiFi configuration, the device will go into access point mode automatically. Connect to WiFi "MQTT_Scale" with password "scalemqtt"
4. In a web browser, go to the address http://192.168.4.1
5. Fill in the WiFi information and MQTT server connection information, enter the base MQTT topic to use for the device, enter a device ID (will be used as device_id in Home Assistant) and a name (will appear in Home Assistant), then save.
6. The device will reconnect to the WiFi. If it can't connect, it will return to access point mode. Repeat steps 3-5 to fix WiFi configuration.
7. Once connected to the WiFi, if the device can connect to the MQTT server, it will automatically publish auto-discovery topics, which will be picked up by Home Assistant. Otherwise, find the device's IP address in your router and connect to that IP address in your web browser to adjust the MQTT configurations.
8. In the Home Assistant configuration, under "Devices & Services", find your MQTT integration, and click "devices". You should see a new device with the name you entered in the device configuration page.
9. Clicking on the device will show all sensors and MQTT configuration options.


## Calibration
1. Ensure no additional weight is on the scale.
2. In Home Assistant, go to the device page
3. Press the "Tare scale" button. After a few seconds, the "Weight" value should go to 0kg
4. Place a known weight on scales.
5. Adjust the "Calibration factor" field until the  "Weight" value displays the weight of the object placed on the scale.
6. Remove the weight. The weight shown should return back to zero.



## Need help?
Join the community on discord:
* [Discord](https://discord.gg/Bgfvy2f)

## Support
If you would like to support this project and many more like it, please consider supporting me so I can keep delivering more projects just like this one:

* [Patreon](https://www.patreon.com/everythingsmarthome)
* [Buy Me A Coffee!](https://www.buymeacoffee.com/EverySmartHome)

## Credits
The original idea for this has been adapted from Zack over at [Self Hosted Home](https://selfhostedhome.com/diy-bed-presence-detection-home-assistant/)'s original build, so big thanks to him!
