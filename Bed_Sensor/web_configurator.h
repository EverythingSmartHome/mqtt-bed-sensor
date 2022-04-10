/****************************************************************************************************************************
  web_configurator.h
  This class manages WiFi connection and runs an HTTP server to manage device configurations

  Features:
    - Index page on file system
    - Substitution of placeholder items with configuration values in HTTP form
    - Automatic switch to standalone Access Point mode if no connection to WiFi is available

  Uses JSON Configurator 

  Required files on the LittleFS filesystem:
    - index.html: The HTML file to be served. All form fields that myst contain a value from the configuration file myst have a placeholder in the place where the configuration value must be inserted
    - Substitutions file: This file contains the key/value pairs of placeholders in the index file and the configuration file field name that must be used to substitute it
 ***************************************************************************************************************************************/

#ifndef WEB_CONFIGURATOR_H
#define WEB_CONFIGURATOR_H

#include "defines.h"
#include "json_configurator.h"
#include <ArduinoJson.h>

#include <LittleFS.h>

#ifndef JSON_DOC_SIZE
#define JSON_DOC_SIZE  1024     // Maximum size of JSON document in bytes
#endif

#define WIFI_MAX_ATTEMPTS       5               // Maximum number of connection attemps to WiFi network before switching to Access Point mode
#define AP_SSID                 "MQTT_Scale"    // SSID of the access point when in Access Point mode
#define AP_PASSWORD             "scalemqtt"     // Password of the access point when in Access Point mode

class WebConfigurator {

  public:

  /*  Main class constructor
   *  
   *  Takes these input parameters:
   *  p_configuration: Pointer to the application configuration file
   *  p_wifi_ssid: SSID of the WiFi network to connect to. Set to "" if none
   *  p_wifi_password: Password of the WiFi network to connect to. Set to "" if none
   *  p_port: Port of the webserver. Defaults to 80
   *  p_substitutions_file: Full path, including leading /, to the substitutions file. This file contains the list of tags in the HTML form and the configuration field name to replace them with to display configuration values on the HTML form
   */
  WebConfigurator(JSONConfigurator *p_configuration, String p_wifi_ssid, String p_wifi_password, int p_port=80, String p_substitutions_file=""); 
  
  void connect_wifi(); // Connects the WiFi and switches to Access Point mode if connection is not successful
  void begin(); // Initiate the web server
  String ip(); // Get IP address of WiFi client

  static void root_callback(); // Used internally to handle HTTP GET /
  static void update_callback(); // Used internally to handle HTTP POST to /update
  static void not_found_callback(); // Used internally to handle any other HTTP request
  
  void handleRoot(); // Internal function that handles the HTTP GET /
  void handleUpdate(); // Internal function that handles the HTTP POST to /update
  void handleNotFound(); // Internal function that handles any other HTTP request

  void handleClient(); // Call this function regularly to allow the web server to handle HTTP requests
  
  static WebConfigurator *server_class; // Internal static link to main instantiated web server class
  bool debug_mode = false; // Set to true to enable debug mode. Outputs tracing information to serial monitor

  
  private:

  fs::FS *lfs; // For future compatibilty with ESP32
  
  int status;     // the Wifi radio's status
  int reqCount;   // number of requests received
  bool ap_mode;   // Indicate if we are in Access Point mode or not

  JSONConfigurator *configuration; // Main configuration file
  char* wifi_ssid;                 // SSID of the WiFi to connect to
  char* wifi_password;             // Password of the WiFi to connect to
  int wifi_port;                   // Port of web server

  String substitutions_file;       // Name of the substitutions file
  StaticJsonDocument<JSON_DOC_SIZE> substitutions; // Content of the substitutions file

  String wifi_ssid_field; // Name of the configuration file field that contains the WiFi SSID
  String wifi_password_field; // Name of the configuration file field that contains the WiFi password
  
  WiFiWebServer *server; // The actual web server

  void set_wifi_info(); // Set all variables to handle WiFi connection
  String getFile(const char *file_name);   // Reads file_name from file system
  String escape_html(const char* source); // Escape HTML characters 

  void load_substitutions(); // Load the substitutions file
  String apply_substitutions(String file, String content); // Apply substitions from substitutions file to HTML file

  const char* index_page = "/index.html"; // Path to index page file
};

// Main class constructor. See definition in class definition
WebConfigurator::WebConfigurator(JSONConfigurator *p_configuration, String p_wifi_ssid, String p_wifi_password, int p_port, String p_substitutions_file) {
  Serial.println("Initializing web configurator");

  lfs = &LittleFS;

  status = WL_IDLE_STATUS;
  reqCount = 0;                // number of requests received
  ap_mode = false;

  configuration = p_configuration;
  wifi_ssid_field = p_wifi_ssid;
  wifi_password_field = p_wifi_password;
  set_wifi_info();
  
  wifi_port = p_port;

  Serial.println("Initializing web server");
  server = new WiFiWebServer(p_port);

  substitutions_file = p_substitutions_file;
  if (substitutions_file.length() > 0)
    load_substitutions();
}

// Connect to WiFi and switch to Access Point mode if unable to connect
void WebConfigurator::connect_wifi() {
  Serial.println("Initializing wifi connection");

  if (status == WL_CONNECTED)
  {
    if (ap_mode)
      WiFi.softAPdisconnect(true);
    else
      WiFi.disconnect(true);

    status = WL_IDLE_STATUS;
  }
  
  if (strlen(wifi_ssid) > 0)
  {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA);
    status = WiFi.begin(wifi_ssid, wifi_password);

    delay(1000);
    int attempts = 0;

    // attempt to connect to WiFi network
    while ( (status != WL_CONNECTED) && (attempts < WIFI_MAX_ATTEMPTS))
    {
      delay(500);

      // Connect to WPA/WPA2 network
      status = WiFi.status();
      attempts++;
    }

    ap_mode = (status != WL_CONNECTED);

    if (!ap_mode)
    {
      Serial.print("Connected to ");
      Serial.print(wifi_ssid);
      Serial.print(" with IP address ");
      Serial.println(WiFi.localIP());
    }
  }
  else
    ap_mode = true;

  if (ap_mode) // Setup access point
  {
    Serial.println("Unable to connect to WiFi. Enabling local access point.");

    WiFi.mode(WIFI_AP);
    IPAddress local_IP(192,168,4,1);
    IPAddress gateway(0,0,0,0);
    IPAddress subnet(255,255,255,0);

    Serial.print("Setting soft-AP configuration ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  
    Serial.print("Setting soft-AP ... ");
    Serial.println(WiFi.softAP(AP_SSID, AP_PASSWORD) ? "Ready" : "Failed!");
  
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());
  }
}  

WebConfigurator *WebConfigurator::server_class = NULL;

// Start web server
void WebConfigurator::begin() {
  server_class = this;
    server->on(F("/"), WebConfigurator::root_callback);
    server->on(F("/update/"), update_callback);
    server->onNotFound(not_found_callback);
  
  
    server->begin();
  
}

// Return IP address of web server
String WebConfigurator::ip() {
  return (ap_mode) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

void WebConfigurator::root_callback() {
  server_class->handleRoot();
}

// Serve index page with substitution values replaced from substititions file
void WebConfigurator::handleRoot()
{
  String page = apply_substitutions(index_page, getFile(index_page));

  server->send(200, F("text/html"), page);
}

void WebConfigurator::update_callback() {
  server_class->handleUpdate();
}

// Process update and save to configuration file
void WebConfigurator::handleUpdate()
{
  if (server->method() != HTTP_POST)
  {
    server->send(405, F("text/plain"), F("Method Not Allowed"));
  }
  else
  {
    for (uint8_t i = 0; i < server->args() - 1; i++)
    {
      #ifdef DEBUG_IOT      
      Serial.print(server->argName(i));
      Serial.println(server->arg(i));
      #endif

      configuration->set_item(server->argName(i), server->arg(i));
    }

    if ((configuration->changed("wifi_ssid")) || (configuration->changed("wifi_password")))
    {
      #ifdef DEBUG_IOT      
      Serial.println("Wifi changed");
      #endif

      set_wifi_info();
      connect_wifi();      
    }

    configuration->save_configuration();

    handleRoot();
  }
}

void WebConfigurator::not_found_callback() {
  server_class->handleUpdate();
}

// Handle invalid requests
void WebConfigurator::handleNotFound()
{
  String message = F("File Not Found\n\n");

  message += F("URI: ");
  message += server->uri();
  message += F("\nMethod: ");
  message += (server->method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server->args();
  message += F("\n");

  for (uint8_t i = 0; i < server->args(); i++)
  {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }

  server->send(404, F("text/plain"), message);
}

// Handle incoming requests. This must be called regularly to ensure HTTP requests are processed in a timely manner
void WebConfigurator::handleClient() {
  server->handleClient();
}

// Read a file from the file system
String WebConfigurator::getFile(const char *file_name) {
  File file = lfs->open(file_name, "r");
  if (!file) {
    Serial.print("Failed to open file ");
    Serial.print(file_name);
    Serial.println("for reading");
    return "";
  }

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();

  #ifdef DEBUG_IOT      
  Serial.println("File Content:");
  Serial.println(content);
  #endif

  return content;
}

// Escape HTML characters and return modified content
String WebConfigurator::escape_html(const char* source) {
  String str = String(source);
  str.replace("\"", "&quot;");
  return str;
}  

// Load substitutions file
void WebConfigurator::load_substitutions() {
  Serial.print("Loading ");
  Serial.println(substitutions_file);

  File file = lfs->open(substitutions_file, "r");
  if (!file) {
    Serial.print("Failed to open file ");
    Serial.print(substitutions_file);
    Serial.println(" for reading");
    return;
  }

  DeserializationError error = deserializeJson(substitutions, file);
  file.close();
  
  if (error) 
    Serial.println("No data found");
  else  
    Serial.println("JSON loaded");
}  

// Apply substitutions for file to content and return new content
String WebConfigurator::apply_substitutions(String file, String content)
{
  String out = content;
  
  if (substitutions.containsKey(file))
  {
    JsonObject subs_items = substitutions[file].as<JsonObject>();

    for (JsonPair subs : subs_items) {
      char * value = new char[subs.value().as<String>().length() + 1];
      strcpy(value, subs.value().as<String>().c_str());

      out.replace(subs.key().c_str(), escape_html(configuration->get_item(value)));
    }
  }

  return out;
}

// Set WiFi variables
void WebConfigurator::set_wifi_info() {
  Serial.println("Getting wifi configuration");
  char* v_ssid = new char[wifi_ssid_field .length() + 1];
  char* v_pwd = new char[wifi_password_field .length() + 1];

  strcpy(v_ssid, wifi_ssid_field .c_str());
  strcpy(v_pwd, wifi_password_field .c_str());

  wifi_ssid = new char[strlen(configuration->get_item(v_ssid))+1];
  strcpy(wifi_ssid, configuration->get_item(v_ssid));

  wifi_password = new char[strlen(configuration->get_item(v_pwd))+1];
  strcpy(wifi_password, configuration->get_item(v_pwd));  
}

#endif
