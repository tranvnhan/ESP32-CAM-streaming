#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <secrets.h>

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART =
    "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";
static const char *JPG_CONTENT_TYPE = "image/jpeg";

// This project was tested with the AI Thinker Model
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

typedef struct {
  camera_fb_t *fb;
  size_t index;
} camera_frame_t;

static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

class AsyncJpegStreamResponse : public AsyncAbstractResponse {
private:
  camera_frame_t _frame;
  size_t _index;
  size_t _jpg_buf_len;
  uint8_t *_jpg_buf;
  uint64_t lastAsyncRequest;

public:
  AsyncJpegStreamResponse() {
    _callback = nullptr;
    _code = 200;
    _contentLength = 0;
    _contentType = STREAM_CONTENT_TYPE;
    _sendContentLength = false;
    _chunked = true;
    _index = 0;
    _jpg_buf_len = 0;
    _jpg_buf = NULL;
    lastAsyncRequest = 0;
    memset(&_frame, 0, sizeof(camera_frame_t));
  }
  ~AsyncJpegStreamResponse() {
    if (_frame.fb) {
      if (_frame.fb->format != PIXFORMAT_JPEG) {
        free(_jpg_buf);
      }
      esp_camera_fb_return(_frame.fb);
    }
  }
  bool _sourceValid() const { return true; }
  virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
    size_t ret = _content(buf, maxLen, _index);
    if (ret != RESPONSE_TRY_AGAIN) {
      _index += ret;
    }
    return ret;
  }
  size_t _content(uint8_t *buffer, size_t maxLen, size_t index) {
    if (!_frame.fb || _frame.index == _jpg_buf_len) {
      if (index && _frame.fb) {
        uint64_t end = (uint64_t)micros();
        int fp = (end - lastAsyncRequest) / 1000;
        log_printf("Size: %uKB, Time: %ums (%.1ffps)\n", _jpg_buf_len / 1024,
                   fp);
        lastAsyncRequest = end;
        if (_frame.fb->format != PIXFORMAT_JPEG) {
          free(_jpg_buf);
        }
        esp_camera_fb_return(_frame.fb);
        _frame.fb = NULL;
        _jpg_buf_len = 0;
        _jpg_buf = NULL;
      }
      if (maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) +
                    strlen(JPG_CONTENT_TYPE) + 8)) {
        // log_w("Not enough space for headers");
        return RESPONSE_TRY_AGAIN;
      }
      // get frame
      _frame.index = 0;

      _frame.fb = esp_camera_fb_get();
      if (_frame.fb == NULL) {
        log_e("Camera frame failed");
        return 0;
      }

      if (_frame.fb->format != PIXFORMAT_JPEG) {
        unsigned long st = millis();
        bool jpeg_converted =
            frame2jpg(_frame.fb, 80, &_jpg_buf, &_jpg_buf_len);
        if (!jpeg_converted) {
          log_e("JPEG compression failed");
          esp_camera_fb_return(_frame.fb);
          _frame.fb = NULL;
          _jpg_buf_len = 0;
          _jpg_buf = NULL;
          return 0;
        }
        log_i("JPEG: %lums, %uB", millis() - st, _jpg_buf_len);
      } else {
        _jpg_buf_len = _frame.fb->len;
        _jpg_buf = _frame.fb->buf;
      }

      // send boundary
      size_t blen = 0;
      if (index) {
        blen = strlen(STREAM_BOUNDARY);
        memcpy(buffer, STREAM_BOUNDARY, blen);
        buffer += blen;
      }
      // send header
      size_t hlen =
          sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, _jpg_buf_len);
      buffer += hlen;
      // send frame
      hlen = maxLen - hlen - blen;
      if (hlen > _jpg_buf_len) {
        maxLen -= hlen - _jpg_buf_len;
        hlen = _jpg_buf_len;
      }
      memcpy(buffer, _jpg_buf, hlen);
      _frame.index += hlen;
      return maxLen;
    }

    size_t available = _jpg_buf_len - _frame.index;
    if (maxLen > available) {
      maxLen = available;
    }
    memcpy(buffer, _jpg_buf + _frame.index, maxLen);
    _frame.index += maxLen;

    return maxLen;
  }
};

void getCameraStatus(AsyncWebServerRequest *request) {
  static char json_response[1024];

  sensor_t *s = esp_camera_sensor_get();
  if (s == NULL) {
    request->send(501);
    return;
  }
  char *p = json_response;
  *p++ = '{';

  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  p += sprintf(p, "\"quality\":%u,", s->status.quality);
  p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
  p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
  p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
  p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
  p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
  p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
  p += sprintf(p, "\"awb\":%u,", s->status.awb);
  p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
  p += sprintf(p, "\"aec\":%u,", s->status.aec);
  p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
  p += sprintf(p, "\"denoise\":%u,", s->status.denoise);
  p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
  p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
  p += sprintf(p, "\"agc\":%u,", s->status.agc);
  p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
  p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
  p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
  p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
  p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
  p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
  p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
  p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
  p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
  p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
  *p++ = '}';
  *p++ = 0;

  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json_response);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

void streamJpg(AsyncWebServerRequest *request) {
  AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
  if (!response) {
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

void setup() {

  Serial.begin(115200);

  // Config camera pins
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Frame quality settings
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera init success");

  // WiFi initialization
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("Connecting to ");
    Serial.println(SSID);
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Server config
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello, world");
  });

  server.on("/status", HTTP_GET, getCameraStatus);
  server.on("/stream", HTTP_GET, streamJpg);
  server.onNotFound(notFound);

  server.begin();
}

void loop() {}