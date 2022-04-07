#include <Arduino.h>
#include <HX711.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#if (ESP32)
#include <LITTLEFS.h>
#else
#include <LittleFS.h>
#endif

#include "json_configurator.h"
#include "web_configurator.h"

#include "config.h"

// File system library depending on board type
#if (ESP32)
fs::LITTLEFSFS lfs = LITTLEFS;
#else
fs::FS lfs = LittleFS;
#endif

JSONConfigurator *configuration;      // Configuration file manager
WebConfigurator *server;              // Web server for device configuration

HX711 scale;                          // Initiate HX711 library
WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

char* base_topic;
char* availability_topic;
char* state_topic;
char* raw_state_topic;
char* tare_topic;

char* create_topic(const char* base, const char* suffix) {
  char* out;
  strcpy(out, base);
  if (strlen(out) > 0) {
    if (out[strlen(out)-1] != '/')
      strcat(out, "/");

    strcat(out, suffix);
  }  

  return out;
}

void set_topics() {
  strcpy(base_topic, configuration->get_item("base_topic"));
  strcpy(availability_topic, create_topic(base_topic, AVAILABILITY_SUFFIX));
  strcpy(state_topic, base_topic);
  strcpy(raw_state_topic, create_topic(base_topic, RAW_SUFFIX));
  strcpy(tare_topic, create_topic(base_topic, TARE_SUFFIX));
}

void handleConfigurationChange() {
  Serial.println("Configuration has changed");
}

void connectMqtt() {
  if (client.connected())
    client.disconnect();

  if ((strlen(configuration->get_item("mqtt_host")) > 0) && (strlen(configuration->get_item("mqtt_port")) > 0)) {
    client.setServer(configuration->get_item("mqtt_host"), atoi(configuration->get_item("mqtt_port")));                // Set MQTT server and port number
    client.setCallback(callback);                                                                                     // Set callback address, this is used for remote tare   

    if ((strlen(configuration->get_item("device_id")) > 0) && (strlen(configuration->get_item("mqtt_username")) > 0) && (strlen(availability_topic) >0)) {
      while (!client.connected()) {       // Loop until connected to MQTT server
        Serial.print("Attempting MQTT connection...");
        if (client.connect(configuration->get_item("device_id"), configuration->get_item("mqtt_username"), configuration->get_item("mqtt_password"), availability_topic, 2, true, "offline")) {       //Connect to MQTT server
          Serial.println("connected"); 
          client.publish(availability_topic, "online", true);         // Once connected, publish online to the availability topic

          if (strlen(tare_topic) > 0)
            client.subscribe(tare_topic);       //Subscribe to tare topic for remote tare
        } else {
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);  // Will attempt connection again in 5 seconds
        }
      }  
    }
  }
}

void setup() {
  Serial.begin(74880);
  while (!Serial); // Wait for Serial library to initialize
  Serial.println();

  // Mount file system
  if (!(lfs.begin())) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  configuration = new JSONConfigurator(CONFIG_FILE_NAME); // Load configuration
  configuration->onConfigChange(handleConfigurationChange); // Handle changes in configuration 
  
  set_topics(); // Set the MQTT topics

  // Start web server
  server = new WebConfigurator(configuration, "wifi_ssid", "wifi_password", 80, "substitutions.json");
  server->connect_wifi();
  server->begin();

  Serial.print(F("HTTP server started @ "));
  Serial.println(server->ip());

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);   // Start scale on specified pins
  scale.wait_ready();                                 //Ensure scale is ready, this is a blocking function
  scale.set_scale();                                  
  Serial.println("Scale Set");
  scale.wait_ready();
  scale.tare();                                       // Tare scale on startup
  scale.wait_ready();
  Serial.println("Scale Zeroed");

  connectMqtt();
}

void loop() {
  server->handleClient(); // Manage HTTP connections
  
  float reading; // Float for reading
  float raw; // Float for raw value which can be useful
  scale.wait_ready(); // Wait till scale is ready, this is blocking if your hardware is not connected properly.
  scale.set_scale(calibration_factor);  // Sets the calibration factor.

  // Ensure we are still connected to MQTT Topics
  if (!client.connected()) 
    connectMqtt();

  
  Serial.print("Reading: ");            // Prints weight readings in .2 decimal kg units.
  scale.wait_ready();
  reading = scale.get_units(10);        //Read scale in g/Kg
  raw = scale.read_average(5);          //Read raw value from scale too
  Serial.print(reading, 2);
  Serial.println(" kg");
  Serial.print("Raw: ");
  Serial.println(raw);
  Serial.print("Calibration factor: "); // Prints calibration factor.
  Serial.println(calibration_factor);

  if (reading < 0) {
    reading = 0.00;     //Sets reading to 0 if it is a negative value, sometimes loadcells will drift into slightly negative values
  }

  String value_str = String(reading);
  String value_raw_str = String(raw);
  client.publish(state_topic, (char *)value_str.c_str());               // Publish weight to the STATE topic
  client.publish(raw_state_topic, (char *)value_raw_str.c_str());       // Publish raw value to the RAW topic

  client.loop();          // MQTT task loop
  scale.power_down();    // Puts the scale to sleep mode for 3 seconds. I had issues getting readings if I did not do this
  delay(3000);
  scale.power_up();
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, tare_topic) == 0) {
    Serial.println("starting tare...");
    scale.wait_ready();
    scale.set_scale();
    scale.tare();       //Reset scale to zero
    Serial.println("Scale reset to zero");
  }
}
