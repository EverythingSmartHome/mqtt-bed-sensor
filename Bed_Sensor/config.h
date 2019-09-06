
// Wifi Settings
#define SSID                          "Your Wifi SSID Here"
#define PASSWORD                      "Your Wifi Password Here"

// MQTT Settings
#define HOSTNAME                      "bed-sensor"
#define MQTT_SERVER                   "your_mqtt_hostname"
#define STATE_TOPIC                   "home/bedroom/bed"
#define STATE_RAW_TOPIC               "home/bedroom/bed/raw"
#define AVAILABILITY_TOPIC            "home/bedroom/bed/available"
#define TARE_TOPIC                    "home/bedroom/bed/tare"
#define mqtt_username                 "mqtt_username"
#define mqtt_password                 "mqtt_password"

// HX711 Pins
const int LOADCELL_DOUT_PIN = 2; // Remember these are ESP GPIO pins, they are not the physical pins on the board.
const int LOADCELL_SCK_PIN = 3;
int calibration_factor = 2000; // Defines calibration factor we'll use for calibrating.
