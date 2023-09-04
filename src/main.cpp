#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <secrets.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <camera_utils.h>

const char LITTLE_FS_TAG[] = "LITTLE_FS";
const char WIFI_TAG[] = "WIFI";

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {

  Serial.begin(115200);

  cameraInit();

  // WiFi initialization
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    ESP_LOGI(WIFI_TAG, "Connecting to: %s", SSID);
  }
  ESP_LOGI(WIFI_TAG, "IP Address: %s", WiFi.localIP().toString());

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

  AsyncElegantOTA.begin(&server); // enable OTA update
  server.begin();
}

void loop() {}