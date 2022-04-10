// MQTT Settings
#define AVAILABILITY_SUFFIX           "status"
#define TARE_SUFFIX                   "tare"
#define CALIBRATE_SUFFIX              "calibrate"
#define RESTART_SUFFIX                "restart"
#define ATTRIBUTES_SUFFIX             "info"

#define MQTT_BUFFER_SIZE              1024  // Maximum size of MQTT messages. Increase this value if messages are not going through

// File where all configuration is stored
#define CONFIG_FILE_NAME        "/config.json"

#define GITHUB_SOURCE           "https://github.com/EverythingSmartHome/mqtt-bed-sensor"

// HX711 Pins
const int LOADCELL_DOUT_PIN = 2; // Remember these are ESP GPIO pins, they are not the physical pins on the board.
const int LOADCELL_SCK_PIN = 3;
#define SCALE_READ_INTERVAL            3000 // Time to wait between scale reads in milliseconds
int calibration_factor = 2000; // Defines calibration factor we'll use for calibrating.

// File system library depending on board type
fs::FS lfs = LittleFS; // For future compatibility with ESP32

JSONConfigurator *configuration;      // Configuration file manager
WebConfigurator *server;              // Web server for device configuration

HX711 scale;                          // Initiate HX711 library
bool scale_available = false;
unsigned long last_read = 0;
bool do_tare = false;

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

// Define MQTT topics
char* base_topic;
char* availability_topic;
char* tare_topic;
char* calibrate_topic;
char* restart_topic;
char* attributes_topic;
