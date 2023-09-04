#include <WiFiManager.h>      // define before <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <secrets.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_log.h>
#include <camera_utils.h>

const char LITTLE_FS_TAG[] = "LITTLE_FS";
const char WIFI_TAG[] = "WIFI";

WiFiManager wm;

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {

  Serial.begin(115200);

  cameraInit();

  // WiFi initialization
  WiFi.mode(WIFI_STA);  // wifi station mode
  bool res;
  res = wm.autoConnect("AutoConnectAP","password");
  if(!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } 
  else {
      Serial.println("connected...yeey :)");
  }

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    ESP_LOGE(LITTLE_FS_TAG, "An Error has occurred while mounting LittleFS");
    return;
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false);
  });

  server.serveStatic("/", LittleFS, "/");

  // server.on("/status", HTTP_GET, getCameraStatus);
  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    streamJpg(request);
  });
  server.onNotFound(notFound);

  server.begin();
}

void loop() {}