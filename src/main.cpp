#include <WiFiManager.h>      // define before <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <secrets.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_log.h>
#include <camera_utils.h>

const char LITTLE_FS_TAG[] = "LITTLE_FS";
const char WIFI_TAG[] = "WIFI";
const char WEBSOCKET_TAG[] = "WEBSOCKET";

// To keep track of number of connected clients
uint8_t numberConnectedClients = 0;

WiFiManager wm;

/* Function prototypes */
void initWebSocket();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {

  Serial.begin(115200);


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

  initWebSocket();

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

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    ESP_LOGI(WEBSOCKET_TAG, "WebSocket client #%u connected from %s",
             client->id(), client->remoteIP().toString().c_str());
    numberConnectedClients++;
    // Only init the camera module when the 1st client is connected,
    // subsequent clients do not require re-initialization
    if (numberConnectedClients == 1) {
      cameraInit();
    }

    break;
  case WS_EVT_DISCONNECT:
    ESP_LOGI(WEBSOCKET_TAG, "WebSocket client #%u disconnected",
             client->id());
    numberConnectedClients--;

    // When no more clients are connected, it's better to deinitialize the camera object and free the resources
    if (numberConnectedClients == 0) {
      cameraStop();
    }
    break;
  case WS_EVT_DATA:
    // handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}