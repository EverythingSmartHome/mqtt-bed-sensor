#include <Arduino.h>
#include <HX711.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <vector>
#include <LittleFS.h>

#include "json_configurator.h"
#include "web_configurator.h"

#include "config.h"

// Create a topic "dest" based on the base topic "base" followed by "/" and "suffix"
void create_topic(const char* base, const char* suffix, char** dest) {
  char* out = new char[strlen(base) + strlen(suffix) + 2];
  strcpy(out, base);
  
  if (strlen(out) > 0) {
    if (out[strlen(out)-1] != '/')
      strcat(out, "/");

    strcat(out, suffix);
  }  

  *dest = new char[strlen(out) + 1];
  strcpy(*dest, out);
}

// Set MQTT topics
void set_topics() {
  base_topic = new char[strlen(configuration->get_item("base_topic")) + 1];
  strcpy(base_topic, configuration->get_item("base_topic")); // Define device base topic from configuration
  
  create_topic(base_topic, AVAILABILITY_SUFFIX, &availability_topic); //Set availability topic

  state_topic = new char[strlen(base_topic) + 1];
  strcpy(state_topic, base_topic); // State topic is the same as base topic
  
  create_topic(base_topic, RAW_SUFFIX, &raw_state_topic); // Topic to publish raw scale reading
  create_topic(base_topic, TARE_SUFFIX, &tare_topic); // Topic to subscribe to for taring scale
  create_topic(base_topic, CALIBRATE_SUFFIX, &calibrate_topic); // Topic to publish the scale calibration factor
  calibrate_topic_set = new char[strlen(calibrate_topic) + 5]; // Topic to subscribe to update calibration factor
  create_topic(base_topic, RESTART_SUFFIX, &restart_topic); // Topic to subscribe to reboot the ESP device
}

// Triggered whenever there is a change in the configuration file
void handleConfigurationChange() {
  std::vector<String> items = configuration->changed_items();
  bool connectivity_changes = false;
  bool device_changes = false;
  bool operation_changes = false;

  for (String item : items) {
    // Handle changes to network connection
    if ((item.compareTo("wifi_ssid") == 0) || (item.compareTo("wifi_password") == 0) || (item.compareTo("mqtt_host") == 0) || (item.compareTo("mqtt_port") == 0) || (item.compareTo("mqtt_user") == 0) || (item.compareTo("mqtt_password") == 0))
      connectivity_changes = true;

    // Handle changes to device identification
    if ((item.compareTo("device_id") == 0) || (item.compareTo("base_topic") == 0))
      device_changes = true;

    // Handle changes in device operation parameters
    if (item.compareTo("calibration_factor") == 0) 
      operation_changes = true;

    // Process changes

    //Reset topics
    if (device_changes)
      set_topics();

    //Reconnect to MQTT server. Wifi connection is handled by WebConfigurator
    if (connectivity_changes)
      connectMqtt();

    // Update configuration parameters
    if (operation_changes) {
      calibration_factor = atoi(configuration->get_item("calibration_factor"));

      if (strlen(calibrate_topic) > 0) {
        client.publish(calibrate_topic, configuration->get_item("calibration_factor"));
      }
    }
  }
}

// Handle MQTT server connection
void connectMqtt() {
  if (client.connected())
    client.disconnect();

  if ((strlen(configuration->get_item("mqtt_host")) > 0) && (strlen(configuration->get_item("mqtt_port")) > 0)) {
    Serial.print("Setting up MQTT connection to ");
    Serial.print(configuration->get_item("mqtt_host"));

    int port = atoi(configuration->get_item("mqtt_port"));
    Serial.print(" on port ");
    Serial.println(port);
    
    client.setServer(configuration->get_item("mqtt_host"), port);  // Set MQTT server and port number
    client.setBufferSize(1024);                                    // Set size of the MQTT publish buffer
    client.setCallback(callback);                                  // Set callback address, this is used for remote tare   

    if ((strlen(configuration->get_item("device_id")) > 0) && (strlen(configuration->get_item("mqtt_user")) > 0) && (strlen(availability_topic) >0)) {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(configuration->get_item("device_id"), configuration->get_item("mqtt_user"), configuration->get_item("mqtt_password"), availability_topic, 2, true, "offline")) {       //Connect to MQTT server
        Serial.println("connected"); 
        
        publish_config(); // Publish Home Assistant auto discovery items

        // Publish status 
        client.publish(availability_topic, "online", true);         // Once connected, publish online to the availability topic

        char _calibration[sizeof(int) + 1];
        itoa(calibration_factor, _calibration, 10);
        client.publish(calibrate_topic, _calibration, true);

        if (strlen(tare_topic) > 0)
          client.subscribe(tare_topic);       //Subscribe to tare topic for remote tare

        if (strlen(calibrate_topic_set) > 0)
          client.subscribe(calibrate_topic_set); //Subscribe to calibrate topic to adjust calibration

        if (strlen(restart_topic) >0)
          client.subscribe(restart_topic); // Subscribe to restart topic to reboot device
          
      } else {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
  }
}

// Publish Home Assistant auto-discovery topics
void publish_config() {
  String base_config_topic;
  String config_message;
  bool result;
  String device_id = configuration->get_item("device_id");
  String device_name = (strlen(configuration->get_item("device_name")) > 0) ? configuration->get_item("device_name") : configuration->get_item("device_id");

  // Main state device
  base_config_topic = "homeassistant/binary_sensor/";
  base_config_topic += device_id;
  base_config_topic += "/connectivity/config";
  
  config_message = "{ \"dev\": { \"ids\": [ \"";
  config_message += device_id;
  config_message += "\" ], \"name\": \"";
  config_message += device_name;
  config_message += "\" }, \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Status\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_connectivity\", \"stat_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"dev_cla\": \"connectivity\", \"pl_on\": \"online\", \"pl_off\": \"offline\"  }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }


  // Scale kg sensor
  base_config_topic = "homeassistant/sensor/";
  base_config_topic += device_id;
  base_config_topic += "/weight/config";
  
  config_message = "{ \"dev\": { \"ids\": [ \"";
  config_message += device_id;
  config_message += "\" ], \"name\": \"";
  config_message += device_name;
  config_message += "\" }, \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Weight\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_weight\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/weight\", \"unit_of_measurement\": \"kg\", \"icon\": \"mdi:scale\", \"entity_category\": \"diagnostic\" }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }

  // Scale raw sensor
  base_config_topic = "homeassistant/sensor/";
  base_config_topic += device_id;
  base_config_topic += "/raw_weight/config";
  
  config_message = "{ \"dev\": { \"ids\": [ \"";
  config_message += device_id;
  config_message += "\" ], \"name\": \"";
  config_message += device_name;
  config_message += "\" }, \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Raw weight\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_raw_weight\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/raw_weight\", \"icon\": \"mdi:numeric\", \"entity_category\": \"diagnostic\" }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }


  // Calibration adjustment control
  base_config_topic = "homeassistant/number/";
  base_config_topic += device_id;
  base_config_topic += "/calibration_factor/config";
  
  config_message = "{ \"dev\": { \"ids\": [ \"";
  config_message += device_id;
  config_message += "\" ], \"name\": \"";
  config_message += device_name;
  config_message += "\" }, \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Calibration factor\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_calibration_factor\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += CALIBRATE_SUFFIX;
  config_message += "\", \"cmd_t\": \"~/";
  config_message += CALIBRATE_SUFFIX;
  config_message += "/set\", \"icon\": \"mdi:wrench\", \"entity_category\": \"config\", \"min\": 0, \"max\": 65535 }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }


  // Reset button
  base_config_topic = "homeassistant/button/";
  base_config_topic += device_id;
  base_config_topic += "/restart/config";
  
  config_message = "{ \"dev\": { \"ids\": [ \"";
  config_message += device_id;
  config_message += "\" ], \"name\": \"";
  config_message += device_name;
  config_message += "\" }, \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Restart device\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_calibration_factor\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"cmd_t\": \"~/";
  config_message += RESTART_SUFFIX;
  config_message += "\", \"icon\": \"mdi:gesture-tap-button\", \"entity_category\": \"diagnostic\" }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }

}


// Device initial setup
void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial library to initialize
  Serial.println();

  // Mount file system
  if (!(lfs.begin())) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  configuration = new JSONConfigurator(CONFIG_FILE_NAME); // Load configuration
  configuration->onConfigChange(handleConfigurationChange); // Handle changes in configuration 

  calibration_factor = atoi(configuration->get_item("calibration_factor"));
  
  set_topics(); // Set the MQTT topics

  // Start web server
  Serial.println("Initializing web server");
  server = new WebConfigurator(configuration, "wifi_ssid", "wifi_password", 80, "substitutions.json");
  server->connect_wifi();
  server->begin();

  Serial.print(F("HTTP server started @ "));
  Serial.println(server->ip());

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);   // Start scale on specified pins
  
  if (scale.wait_ready_timeout(1000))   {               //Ensure scale is ready without blocking to ensure web server operation
    scale_available = true;
    scale.set_scale();                                  
    scale.wait_ready();
    scale.tare();                                       // Tare scale on startup
    scale.wait_ready();
    Serial.println("Scale ready");
  }
  else {
    Serial.println("No scale available.");
    scale_available = false;
  }

  connectMqtt();
}


// Main loop
void loop() {
  server->handleClient(); // Manage HTTP connections
  
  float reading = 0; // Float for reading
  float raw = 0; // Float for raw value which can be useful

  // Ensure we are still connected to MQTT Topics
  if (!client.connected()) 
    connectMqtt();

  if (scale_available) {
    Serial.print("Reading: ");            // Prints weight readings in .2 decimal kg units.
    scale.wait_ready(); // Wait till scale is ready, this is blocking if your hardware is not connected properly.
    scale.set_scale(calibration_factor);  // Sets the calibration factor.
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

    if (client.connected()) {
      String value_str = String(reading);
      String value_raw_str = String(raw);
      client.publish(state_topic, (char *)value_str.c_str());               // Publish weight to the STATE topic
      client.publish(raw_state_topic, (char *)value_raw_str.c_str());       // Publish raw value to the RAW topic
    
      client.loop();          // MQTT task loop
    }

    scale.power_down();    // Puts the scale to sleep mode for 3 seconds. I had issues getting readings if I did not do this
    delay(3000);
    scale.power_up();
  }
  else {
    // Still execute MQTT loop
    if (client.connected())
      client.loop();
  }


}

// Callback function for all MQTT subscriptions
void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, tare_topic) == 0) { //Tare the scale
    if (scale_available) {
      Serial.println("starting tare...");
      scale.wait_ready();
      scale.set_scale();
      scale.tare();       //Reset scale to zero
      Serial.println("Scale reset to zero");
    }
  }
  else if (strcmp(topic, calibrate_topic_set) == 0) { // Change calibration factor
    if (length > 0) {
      char* c_payload = new char[length+1];
      for (int i = 0 ; i < length ; i++)
        c_payload[i] = (char)payload[i];

      c_payload[length] = '\0';

      configuration->set_item("calibration_factor", String(c_payload));
      configuration->save_configuration(); // New calibration factor will be published on configuration change
    }
  }
  else if (strcmp(topic, restart_topic) == 0) { // Reboot the device
    Serial.println("Rebooting device...");
    ESP.restart();
  }
}
