// MQTT Settings
#define AVAILABILITY_SUFFIX           "status"
#define RAW_SUFFIX                    "raw"
#define WEIGHT_SUFFIX                 "weight"
#define TARE_SUFFIX                   "tare"
#define CALIBRATE_SUFFIX              "calibrate"
#define RESTART_SUFFIX                "restart"

// File where all configuration is stored
#define CONFIG_FILE_NAME        "/config.json"

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
char* weight_state_topic;
char* raw_state_topic;
char* tare_topic;
char* calibrate_topic;
char* calibrate_topic_set;
char* restart_topic;
