/****************************************************************************************************************************
  json_configurator.h
  This class manages read/writes to a JSON configuration file on an ESP8266 device using LittleFS

  Features:
    - Tracking of changed fields
    - Automatic assignment of default values
    - Callback when fields change
 ***************************************************************************************************************************************/

#ifndef JSON_CONFIGURATOR_H
#define JSON_CONFIGURATOR_H
#include <vector>
#include <ArduinoJson.h>
#include <LittleFS.h>

#ifndef JSON_DOC_SIZE
#define JSON_DOC_SIZE  1024   // Maximum size of JSON configuration in bytes
#endif

#ifndef DEFAULTS_FILE
#define DEFAULTS_FILE "/defaults.json"
#endif

class JSONConfigurator {
  public:
  
  JSONConfigurator(char* p_filename); // Main class constructor. Takes the configuration file name as an argument. File name must include full path and leading / (ie /config.json)
  char* get_item(char* p_key); // Reads a configuration item and returns the value as char*
  void set_item(String p_key, String p_value); // Sets the value of a configuration item
  bool changed(String p_key); // Check if configuration item p_key has been modified since last save
  bool config_change(); // Returns true if any configuration element has changed since last save
  std::vector<String> changed_items(); // Returns list of changed attributes
  void save_configuration(); // Saves the configuration
  void onConfigChange(void (*callback)()); // Set callback function to notify of configuration changes

  private:
  fs::FS lfs = LittleFS; // For future compatibility with ESP32

  StaticJsonDocument<JSON_DOC_SIZE> configuration; // Main configuration
  StaticJsonDocument<JSON_DOC_SIZE> original_configuration; // Copy of original configuration for comparison
  StaticJsonDocument<JSON_DOC_SIZE> defaults; // Default values to initialize configuration
  
  char* filename; // Name of configuration file
  const char* defaults_file = DEFAULTS_FILE; // Name of defaults file

  bool defaults_available;
  std::vector<String> _changed_items;

  void (* config_change_callback)();

  bool load_json(char* p_filename, JsonDocument& p_dest);
  void apply_defaults();
  void print_config(JsonDocument& p_doc);
  void load_configuration();
  void clear_changes();
  void flag_changes();
};

// Main class constructor
JSONConfigurator::JSONConfigurator(char* p_filename) {
  config_change_callback = NULL;

  filename = strdup(p_filename);
  defaults_available = load_json(const_cast<char*>(defaults_file), defaults);
  load_configuration();
}

// Sets the callback function to notify of configuration changes
void JSONConfigurator::onConfigChange(void (*callback)()) {
  config_change_callback = callback;
}

// Reads item p_key from the configuration and return its value
char* JSONConfigurator::get_item(char* p_key) {
  if (configuration.containsKey(p_key))
  {
    return const_cast<char*>(configuration[p_key].as<const char*>());
  }
  else
  {
    return "";
  }
}

// Set item p_key in the configuration to value p_value
void JSONConfigurator::set_item(String p_key, String p_value) {
  configuration[p_key] = p_value;
}

// Checks if configuration item p_key has changed  since last save
bool JSONConfigurator::changed(String p_key) {
  if (!configuration.containsKey(p_key))
    return false;
    
  if ((configuration.containsKey(p_key)) && (!original_configuration.containsKey(p_key)))
    return true;

  return (configuration[p_key].as<String>() != original_configuration[p_key].as<String>());
}

// Checks if any item of configuration has changed since last save
bool JSONConfigurator::config_change() {
  return (configuration != original_configuration);
}

// Returns the list of changed attributes
std::vector<String> JSONConfigurator::changed_items() {
  return _changed_items;
}

// Saves configuration file
void JSONConfigurator::save_configuration() {
  Serial.println("Saving JSON configuration");
  bool trigger_callback = false;

  File file = lfs.open(filename, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }

  serializeJson(configuration, file);  
  
  file.close();

  // If there were any changes and a callback function exists, trigger the callback function at the end of this function
  if ((config_change()) && (config_change_callback != NULL))
    trigger_callback = true;

  flag_changes(); // Identify changed fields
  original_configuration = configuration; // Reset changes

  if (trigger_callback)
    config_change_callback(); // Call the callback function to notify of configuration changes

}

// Load JSON file p_filename into JSON document p_dest
bool JSONConfigurator::load_json(char* p_filename, JsonDocument& p_dest) {
  Serial.print("Loading ");
  Serial.println(p_filename);

  File file = lfs.open(p_filename, "r");
  if (!file) {
    Serial.print("Failed to open file ");
    Serial.print(p_filename);
    Serial.println(" for reading");
    return "";
  }

  DeserializationError error = deserializeJson(p_dest, file);
  file.close();
  
  if (error) 
    Serial.println("No data found");
  else  
    Serial.println("JSON loaded");

  return (!error);
}  

// Apply values from defaults file to configuration file if these attributes don't exist
void JSONConfigurator::apply_defaults() {
  JsonObject defaults_obj = defaults.as<JsonObject>();

  for (JsonPair param : defaults_obj) {
    if (!configuration.containsKey(param.key().c_str()))
      configuration[param.key().c_str()] = param.value().as<char*>();
  }
}

// Print the configuration file to the serial monitor
void JSONConfigurator::print_config(JsonDocument& p_doc) {
  JsonObject json_obj = p_doc.as<JsonObject>();

  for (JsonPair param : json_obj) {
    Serial.print(param.key().c_str());
    Serial.print(": ");
    Serial.println(param.value().as<char*>());
  }    
}

// Load main configuration file
void JSONConfigurator::load_configuration() {
  Serial.println("Loading configuration");
  load_json(filename, configuration);  

  original_configuration = configuration;

  if (defaults_available)
    apply_defaults();

  if (original_configuration != configuration)
    save_configuration();
    
  print_config(configuration);    
}

// Identify changed fields after last update
void JSONConfigurator::flag_changes() {
  JsonObject json_obj = configuration.as<JsonObject>();
  _changed_items.clear();
  
  for (JsonPair param : json_obj) {
    if (strcmp(param.value().as<char*>(), original_configuration[param.key().c_str()].as<char*>()) != 0) {
      _changed_items.push_back(String(param.key().c_str()));        
    }
  }    
}
#endif
