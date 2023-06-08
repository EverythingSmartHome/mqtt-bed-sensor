# mqtt-bed-sensor
MQTT Bed Sensor/Scales for bed occupancy (although entirely adaptable for anything), for integrating with Home Assistant, OpenHAB, Domoticz and anything else supporting MQTT.

Has remote tare function which you can issue over MQTT if your sensor suffers from drift which many load cells seem to. This saves having to re-start the device everytime you want to tare.

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

## Use case
This was built specifically with bed occupancy in mind so that automations can be triggered when in bed such as switching lights off, locking doors, setting alarms etc etc using Home Assistant. I found that load cells seem to "drift" a little over time, with this in mind a remote tare function was built in so that I can issue a tare command over MQTT which will reset it to zero without having to physically reset the device everytime.

## Calibration
1. Load calibration sketch and ensure no additional weight is on the bed/scale
2. Using serial monitor, observe starting measurements
3. Place known weight on bed/scale e.g 5kg and observe new measurements printed on monitor
4. Adjust calibration factor variable and repeat steps till values are correct. Take note of calibration factor

## Bed sensor
1. Load sketch, ensure no additional weight is on the scales
2. Fill in all variables in config.h file
3. Observe output on serial monitor, place known weight on scales and ensure read-out is correct.
4. Once happy, unplug and plug into main power supply and enjoy!

## Using ESPHome insted of MQTT
<details>
1. Load an ESPHome sketch onto the ESP and add the following code. Modify the GPIO pins if necessary.

```yaml
globals:
  - id: constant_weight
    type: float

sensor:
  - platform: hx711
    name: Bed
    dout_pin: GPIO2
    clk_pin: GPIO3
    update_interval: 2s
    unit_of_measurement: kg
    accuracy_decimals: 2
    icon: mdi:weight
```

2. Take note of the values you receive when there is no weight on the sensor.

3. Place a known weight on the bed and note the value.

4. Add the following code to the ESP sketch, replacing the values under `calibrate_linear` with the ones you noted earlier.

```yaml
    filters:
      - calibrate_linear:
          - 1967236 -> 0
          - 3454600 -> 70.5
      - max:
          window_size: 2
          send_every: 1
      - lambda: 'return x >= 10.0 ? x : 0;'
      - delta: 2
```

5. If the high values in your history bother you, you can remove the sensor from the `vices & Services`ection. After that, navigate to the `Statistics` in the `Developer Tools` and click on `Fix issue` for your sensor. Finally, restart Home Assistant, and the ESP sensor should reappear.

Please note that this code is not perfect, but I like the idea of having all my ESP devices in ESPHome, so I created it.

</details>


## Need help?
Join the community on discord:
* [Discord](https://discord.gg/Bgfvy2f)

## Support
If you would like to support this project and many more like it, please consider supporting me so I can keep delivering more projects just like this one:

* [Patreon](https://www.patreon.com/everythingsmarthome)
* [Buy Me A Coffee!](https://www.buymeacoffee.com/EverySmartHome)

## Credits
The original idea for this has been adapted from Zack over at [Self Hosted Home](https://selfhostedhome.com/diy-bed-presence-detection-home-assistant/)'s original build, so big thanks to him!
