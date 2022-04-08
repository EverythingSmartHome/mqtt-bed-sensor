#ifndef JSON_CONFIGURATOR_H
#define JSON_CONFIGURATOR_H
#include <vector>
#include <ArduinoJson.h>
#include <LittleFS.h>

#ifndef JSON_DOC_SIZE
#define JSON_DOC_SIZE  1024
#endif

class JSONConfigurator {
  public:
  
  JSONConfigurator(char* p_filename);
  char* get_item(char* p_key);
  void set_item(String p_key, String p_value);
  bool changed(String p_key);
  bool config_change();
  std::vector<String> changed_items();
  void save_configuration();
  void onConfigChange(void (*callback)());

  private:
  fs::FS lfs = LittleFS; // For future compatibility with ESP32

  StaticJsonDocument<JSON_DOC_SIZE> configuration;
  StaticJsonDocument<JSON_DOC_SIZE> original_configuration;
  StaticJsonDocument<JSON_DOC_SIZE> defaults;
  
  char* filename;
  const char* defaults_file = "/defaults.json";

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

JSONConfigurator::JSONConfigurator(char* p_filename) {
  config_change_callback = NULL;

  Serial.print("Filename: ");
  Serial.println(p_filename);

  filename = strdup(p_filename);
  Serial.println(filename);
  defaults_available = load_json(const_cast<char*>(defaults_file), defaults);
  load_configuration();
}

void JSONConfigurator::onConfigChange(void (*callback)()) {
  config_change_callback = callback;
}

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

void JSONConfigurator::set_item(String p_key, String p_value) {
  configuration[p_key] = p_value;
}

bool JSONConfigurator::changed(String p_key) {
  if (!configuration.containsKey(p_key))
    return false;
    
  if ((configuration.containsKey(p_key)) && (!original_configuration.containsKey(p_key)))
    return true;

  return (configuration[p_key].as<String>() != original_configuration[p_key].as<String>());
}

bool JSONConfigurator::config_change() {
  return (configuration != original_configuration);
}

std::vector<String> JSONConfigurator::changed_items() {
  return _changed_items;
}

void JSONConfigurator::save_configuration() {
  Serial.println("Saving JSON configuration");
  bool trigger_callback = false;

  #ifdef DEBUG_IOT   
  Serial.println("Configuration:");
  print_config(configuration);
  Serial.println("");
  #endif

  File file = lfs.open(filename, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }

  serializeJson(configuration, file);  
  
  file.close();

  if ((config_change()) && (config_change_callback != NULL))
    trigger_callback = true;

  flag_changes();
  original_configuration = configuration;

  if (trigger_callback)
    config_change_callback();

}

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

void JSONConfigurator::apply_defaults() {
  JsonObject defaults_obj = defaults.as<JsonObject>();

  for (JsonPair param : defaults_obj) {
    if (!configuration.containsKey(param.key().c_str()))
      configuration[param.key().c_str()] = param.value().as<char*>();
  }
}

void JSONConfigurator::print_config(JsonDocument& p_doc) {
  JsonObject json_obj = p_doc.as<JsonObject>();

  for (JsonPair param : json_obj) {
    Serial.print(param.key().c_str());
    Serial.print(": ");
    Serial.println(param.value().as<char*>());
  }    
}

void JSONConfigurator::load_configuration() {
  Serial.println("Loading configuration");
  load_json(filename, configuration);  
  Serial.println("Configuration loaded");

  original_configuration = configuration;

  if (defaults_available)
    apply_defaults();

  if (original_configuration != configuration)
    save_configuration();
    
  print_config(configuration);    
}

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
