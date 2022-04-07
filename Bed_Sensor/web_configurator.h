#ifndef WEB_CONFIGURATOR_H
#define WEB_CONFIGURATOR_H

#include "defines.h"
#include "json_configurator.h"
#include <ArduinoJson.h>

#if (ESP32)
#include <LITTLEFS.h>
#else
#include <LittleFS.h>
#endif

#ifndef JSON_DOC_SIZE
#define JSON_DOC_SIZE  1024
#endif

#define WIFI_MAX_ATTEMPTS       5
#define AP_SSID                 "MQTT_Scale"
#define AP_PASSWORD             "scalemqtt"

class WebConfigurator {

  public:

  WebConfigurator(JSONConfigurator *p_configuration, String p_wifi_ssid, String p_wifi_password, int p_port=80, String p_substitutions_file="");
  
  void connect_wifi();
  void begin();
  String ip();

  static void root_callback();
  static void update_callback();
  static void not_found_callback();
  
  void handleRoot();
  void handleUpdate();
  void handleNotFound();

  void handleClient();
  
  static WebConfigurator *server_class;
  bool debug_mode = false;

  
  private:

  #if (ESP32)
  fs::LITTLEFSFS *lfs;
  #else
  fs::FS *lfs;
  #endif  
  
  int status;     // the Wifi radio's status
  int reqCount;                // number of requests received
  bool ap_mode;

  JSONConfigurator *configuration;
  char* wifi_ssid;
  char* wifi_password;
  int wifi_port;

  String substitutions_file;
  StaticJsonDocument<JSON_DOC_SIZE> substitutions;

  String wifi_ssid_field;
  String wifi_password_field;
  
  WiFiWebServer *server;

  void set_wifi_info();
  String getFile(const char *file_name);  
  String escape_html(const char* source);

  void load_substitutions();
  String apply_substitutions(String file, String content);

  const char* index_page = "/index.html";
};

WebConfigurator::WebConfigurator(JSONConfigurator *p_configuration, String p_wifi_ssid, String p_wifi_password, int p_port, String p_substitutions_file) {
  Serial.println("Initializing web configurator");

  #if (ESP32)
  lfs = &LITTLEFS;
  #else
  lfs = &LittleFS;
  #endif  

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

  config_change_callback = NULL;

  substitutions_file = p_substitutions_file;
  if (substitutions_file.length() > 0)
    load_substitutions();
}

void WebConfigurator::connect_wifi() {
  Serial.println("Initializing wifi connection");
#if !(ESP32 || ESP8266)  
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println(F("WiFi shield not present"));
    // don't continue
    while (true);
  }
#endif

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

  if (ap_mode)
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

void WebConfigurator::begin() {
  server_class = this;
    server->on(F("/"), WebConfigurator::root_callback);
    server->on(F("/update/"), update_callback);
    server->onNotFound(not_found_callback);
  
  
    server->begin();
  
}

String WebConfigurator::ip() {
  return (ap_mode) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

void WebConfigurator::root_callback() {
  server_class->handleRoot();
}

void WebConfigurator::handleRoot()
{
  String page = apply_substitutions(index_page, getFile(index_page));

  server->send(200, F("text/html"), page);
}

void WebConfigurator::update_callback() {
  server_class->handleUpdate();
}

void WebConfigurator::handleUpdate()
{
  bool reconnectWifi = false;
  
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

      reconnectWifi = true;
    }

    configuration->save_configuration();

    if (reconnectWifi)
    {
      set_wifi_info();
      connect_wifi();      
    }
      
    handleRoot();
  }
}

void WebConfigurator::not_found_callback() {
  server_class->handleUpdate();
}

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

void WebConfigurator::handleClient() {
  server->handleClient();
}

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

String WebConfigurator::escape_html(const char* source) {
  String str = String(source);
  str.replace("\"", "&quot;");
  return str;
}  

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
