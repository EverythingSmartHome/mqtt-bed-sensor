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
  create_topic(base_topic, ATTRIBUTES_SUFFIX, &attributes_topic); // Topic to publish attributes
  create_topic(base_topic, TARE_SUFFIX, &tare_topic); // Topic to subscribe to for taring scale
  create_topic(base_topic, CALIBRATE_SUFFIX, &calibrate_topic); // Topic to publish the scale calibration factor
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
    if ((connectivity_changes) || (device_changes))
      connectMqtt();

    // Update configuration parameters
    if (operation_changes) 
      calibration_factor = atoi(configuration->get_item("calibration_factor"));      
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
    client.setBufferSize(MQTT_BUFFER_SIZE);                        // Set size of the MQTT publish buffer
    client.setCallback(callback);                                  // Set callback address, this is used for remote tare   

    if ((strlen(configuration->get_item("device_id")) > 0) && (strlen(configuration->get_item("mqtt_user")) > 0) && (strlen(availability_topic) > 0)) {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(configuration->get_item("device_id"), configuration->get_item("mqtt_user"), configuration->get_item("mqtt_password"), availability_topic, 2, true, "offline")) {       //Connect to MQTT server and set LWT message
        Serial.println("connected"); 
        
        publish_config(); // Publish Home Assistant auto discovery items

        // Publish status 
        client.publish(availability_topic, "online", true);         // Once connected, publish online to the availability topic
        publish_attributes(-1, -1);

        if (strlen(tare_topic) > 0)
          client.subscribe(tare_topic);       //Subscribe to tare topic for remote tare

        if (strlen(calibrate_topic) > 0)
          client.subscribe(calibrate_topic); //Subscribe to calibrate topic to adjust calibration

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
  String device = "\"dev\": { \"ids\": [ \"";
  device += device_id;
  device += "\" ], \"name\": \"";
  device += device_name;
  device += "\", \"cu\": \"http://";
  device += server->ip();
  device += "\", \"mf\": \"Open source\", \"mdl\": \"Custom\" }";

  // Main state device
  base_config_topic = "homeassistant/binary_sensor/";
  base_config_topic += device_id;
  base_config_topic += "/connectivity/config";
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Status\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_connectivity\", \"stat_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"dev_cla\": \"connectivity\", \"pl_on\": \"online\", \"pl_off\": \"offline\", \"json_attr_t\": \"~/";
  config_message += ATTRIBUTES_SUFFIX;
  config_message += "\" }";  
  
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
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Weight\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_weight\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += ATTRIBUTES_SUFFIX;
  config_message += "\", \"val_tpl\": \"{{ value_json.weight }}\", \"unit_of_measurement\": \"kg\", \"icon\": \"mdi:scale\", \"entity_category\": \"diagnostic\" }";  
  
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
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Raw weight\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_raw_weight\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += ATTRIBUTES_SUFFIX;
  config_message += "\", \"val_tpl\": \"{{ value_json.raw }}\", \"icon\": \"mdi:numeric\", \"entity_category\": \"diagnostic\" }";  
  
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
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Calibration factor\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_calibration_factor\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += ATTRIBUTES_SUFFIX;
  config_message += "\", \"val_tpl\": \"{{ value_json.calibration_factor }}\", \"cmd_t\": \"~/";
  config_message += CALIBRATE_SUFFIX;
  config_message += "\", \"icon\": \"mdi:wrench\", \"entity_category\": \"config\", \"min\": -65535, \"max\": 65535 }";  
  
  result = client.publish(base_config_topic.c_str(), config_message.c_str(), true);         // Once connected, publish online to the availability topic  
  if (!result) {
    Serial.println("Failed to publish auto discovery configuration");
    Serial.print("Base configuration topic used: ");
    Serial.println(base_config_topic);
  }


  // Tare button
  base_config_topic = "homeassistant/button/";
  base_config_topic += device_id;
  base_config_topic += "/tare/config";
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
  config_message += base_topic;
  config_message += "\", \"name\": \"Tare scale\", \"uniq_id\": \"";
  config_message += device_id;
  config_message += "_tare_scale\", \"avty_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"stat_t\": \"~/";
  config_message += AVAILABILITY_SUFFIX;
  config_message += "\", \"cmd_t\": \"~/";
  config_message += TARE_SUFFIX;
  config_message += "\", \"icon\": \"mdi:numeric-0-circle-outline\", \"entity_category\": \"diagnostic\" }";  
  
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
  
  config_message = "{ ";
  config_message += device;
  config_message += ", \"~\": \"";
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

  unsigned long time_passed = millis() - last_read;

  // Scale needs to be available and there needs to be at least 3 seconds that passed before it can be brought back up
  if( (scale_available) && (time_passed >= SCALE_READ_INTERVAL) ) {
    Serial.print("Reading: ");            // Prints weight readings in .2 decimal kg units.
    scale.power_up();

    // Tare scale if tare command was received
    if (do_tare) {
      Serial.println("starting tare...");
      scale.wait_ready();
      scale.set_scale();
      scale.tare();       //Reset scale to zero
      Serial.println("Scale reset to zero");      

      do_tare = false;
    }
    
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
  
    if (reading < 0) 
      reading = 0.00;     //Sets reading to 0 if it is a negative value, sometimes loadcells will drift into slightly negative values


    if (client.connected()) {
      String value_str = String(reading);
      String value_raw_str = String(raw);
      //client.publish(weight_state_topic, (char *)value_str.c_str());               // Publish weight to the STATE topic
      //client.publish(raw_state_topic, (char *)value_raw_str.c_str());       // Publish raw value to the RAW topic
      publish_attributes(reading, raw);
    
      client.loop();          // MQTT task loop
    }

    scale.power_down();    // Puts the scale to sleep mode for 3 seconds. I had issues getting readings if I did not do this
    last_read = millis();
  }
  else {
    // Still execute MQTT loop
    if (client.connected()) {
      if (time_passed >= SCALE_READ_INTERVAL) { // Don't publish attributes any more often than SCALE_READ_INTERVAL to avoid overloading MQTT server
        publish_attributes(-1, -1); // Publish attributes without scale data
        last_read = millis();
      }
      
      client.loop();
    }
  }


}

// Publishes attributes to MQTT server. Set both arguments to negative value if no data avaialble or scale is not available
void publish_attributes(float weight, float raw) {
  String attributes = "{ \"ip\": \"";
  attributes += server->ip();
  attributes += "\", \"calibration_factor\": ";
  attributes += String(calibration_factor);
  attributes += ", \"Source\": \"";
  attributes += GITHUB_SOURCE;
  attributes += "\"";

  if (weight >= 0) {
    attributes += ", \"weight\": ";
    attributes += String(weight);
  }

  if (raw >= 0) {
    attributes += ", \"raw\": ";
    attributes += String(raw);
  }

  attributes += " }";

  client.publish(attributes_topic, (char *)attributes.c_str());
}

// Callback function for all MQTT subscriptions
void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, tare_topic) == 0) { //Tare the scale
    if (scale_available) {
      do_tare = true;
    }
  }
  else if (strcmp(topic, calibrate_topic) == 0) { // Change calibration factor
    if (length > 0) {
      // Convert payload from bytes to char array
      char* c_payload = new char[length+1];
      for (int i = 0 ; i < length ; i++)
        c_payload[i] = (char)payload[i];

      c_payload[length] = '\0';

      // Update configuration. New calibration factor will be published on next iteration of main loop
      configuration->set_item("calibration_factor", String(c_payload));
      configuration->save_configuration(); 
    }
  }
  else if (strcmp(topic, restart_topic) == 0) { // Reboot the device
    Serial.println("Rebooting device...");
    ESP.restart();
  }
}
