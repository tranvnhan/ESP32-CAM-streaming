#include <Arduino.h>
#include <AsyncElegantOTA.h>;
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
    // log_printf("New client\n");
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
    // log_printf("Client exit\n");
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
        // log_printf("Size: %uKB, Time: %ums (%.1ffps)\n", _jpg_buf_len / 1024,
        //            fp);
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

// Website
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title></title>
  <style>
body {
    font-family: Arial, Helvetica, sans-serif;
    background: #181818;
    color: #EFEFEF;
    font-size: 16px
}

h2 {
    font-size: 18px
}

section.main {
    display: flex
}

#menu,
section.main {
    flex-direction: column
}

#menu {
    display: none;
    flex-wrap: nowrap;
    min-width: 340px;
    background: #363636;
    padding: 8px;
    border-radius: 4px;
    margin-top: -10px;
    margin-right: 10px;
}

#content {
    display: flex;
    flex-wrap: wrap;
    align-items: stretch
}

figure {
    padding: 0px;
    margin: 0;
    -webkit-margin-before: 0;
    margin-block-start: 0;
    -webkit-margin-after: 0;
    margin-block-end: 0;
    -webkit-margin-start: 0;
    margin-inline-start: 0;
    -webkit-margin-end: 0;
    margin-inline-end: 0
}

figure img {
    display: block;
    width: 100%;
    height: auto;
    border-radius: 4px;
    margin-top: 8px;
}

@media (min-width: 800px) and (orientation:landscape) {
    #content {
        display: flex;
        flex-wrap: nowrap;
        align-items: stretch
    }

    figure img {
        display: block;
        max-width: 100%;
        max-height: calc(100vh - 40px);
        width: auto;
        height: auto
    }

    figure {
        padding: 0 0 0 0px;
        margin: 0;
        -webkit-margin-before: 0;
        margin-block-start: 0;
        -webkit-margin-after: 0;
        margin-block-end: 0;
        -webkit-margin-start: 0;
        margin-inline-start: 0;
        -webkit-margin-end: 0;
        margin-inline-end: 0
    }
}

section#buttons {
    display: flex;
    flex-wrap: nowrap;
    /* justify-content: space-between */
    justify-content: center
}

#nav-toggle {
    cursor: pointer;
    display: block
}

#nav-toggle-cb {
    outline: 0;
    opacity: 0;
    width: 0;
    height: 0
}

#nav-toggle-cb:checked+#menu {
    display: flex
}

.input-group {
    display: flex;
    flex-wrap: nowrap;
    line-height: 22px;
    margin: 5px 0
}

.input-group>label {
    display: inline-block;
    padding-right: 10px;
    min-width: 47%
}

.input-group input,
.input-group select {
    flex-grow: 1
}

.range-max,
.range-min {
    display: inline-block;
    padding: 0 5px
}

button,
.button {
    display: block;
    margin: 5px;
    padding: 0 12px;
    border: 0;
    line-height: 28px;
    cursor: pointer;
    color: #fff;
    background: #ff3034;
    border-radius: 5px;
    font-size: 16px;
    outline: 0
}

.save {
    position: absolute;
    right: 25px;
    top: 0px;
    height: 16px;
    line-height: 16px;
    padding: 0 4px;
    text-decoration: none;
    cursor: pointer
}

button:hover {
    background: #ff494d
}

button:active {
    background: #f21c21
}

button.disabled {
    cursor: default;
    background: #a0a0a0
}

input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 22px;
    background: #363636;
    cursor: pointer;
    margin: 0
}

input[type=range]:focus {
    outline: 0
}

input[type=range]::-webkit-slider-runnable-track {
    width: 100%;
    height: 2px;
    cursor: pointer;
    background: #EFEFEF;
    border-radius: 0;
    border: 0 solid #EFEFEF
}

input[type=range]::-webkit-slider-thumb {
    border: 1px solid rgba(0, 0, 30, 0);
    height: 22px;
    width: 22px;
    border-radius: 50px;
    background: #ff3034;
    cursor: pointer;
    -webkit-appearance: none;
    margin-top: -11.5px
}

input[type=range]:focus::-webkit-slider-runnable-track {
    background: #EFEFEF
}

input[type=range]::-moz-range-track {
    width: 100%;
    height: 2px;
    cursor: pointer;
    background: #EFEFEF;
    border-radius: 0;
    border: 0 solid #EFEFEF
}

input[type=range]::-moz-range-thumb {
    border: 1px solid rgba(0, 0, 30, 0);
    height: 22px;
    width: 22px;
    border-radius: 50px;
    background: #ff3034;
    cursor: pointer
}

input[type=range]::-ms-track {
    width: 100%;
    height: 2px;
    cursor: pointer;
    background: 0 0;
    border-color: transparent;
    color: transparent
}

input[type=range]::-ms-fill-lower {
    background: #EFEFEF;
    border: 0 solid #EFEFEF;
    border-radius: 0
}

input[type=range]::-ms-fill-upper {
    background: #EFEFEF;
    border: 0 solid #EFEFEF;
    border-radius: 0
}

input[type=range]::-ms-thumb {
    border: 1px solid rgba(0, 0, 30, 0);
    height: 22px;
    width: 22px;
    border-radius: 50px;
    background: #ff3034;
    cursor: pointer;
    height: 2px
}

input[type=range]:focus::-ms-fill-lower {
    background: #EFEFEF
}

input[type=range]:focus::-ms-fill-upper {
    background: #363636
}

.switch {
    display: block;
    position: relative;
    line-height: 22px;
    font-size: 16px;
    height: 22px
}

.switch input {
    outline: 0;
    opacity: 0;
    width: 0;
    height: 0
}

.slider {
    width: 50px;
    height: 22px;
    border-radius: 22px;
    cursor: pointer;
    background-color: grey
}

.slider,
.slider:before {
    display: inline-block;
    transition: .4s
}

.slider:before {
    position: relative;
    content: "";
    border-radius: 50%;
    height: 16px;
    width: 16px;
    left: 4px;
    top: 3px;
    background-color: #fff
}

input:checked+.slider {
    background-color: #ff3034
}

input:checked+.slider:before {
    -webkit-transform: translateX(26px);
    transform: translateX(26px)
}

select {
    border: 1px solid #363636;
    font-size: 14px;
    height: 22px;
    outline: 0;
    border-radius: 5px
}

.image-container {
    position: relative;
    min-width: 160px
}

.close {
    position: absolute;
    right: 5px;
    top: 5px;
    background: #ff3034;
    width: 16px;
    height: 16px;
    border-radius: 100px;
    color: #fff;
    text-align: center;
    line-height: 18px;
    cursor: pointer
}

.hidden {
    display: none
}

input[type=text] {
    border: 1px solid #363636;
    font-size: 14px;
    height: 20px;
    margin: 1px;
    outline: 0;
    border-radius: 5px
}

.inline-button {
    line-height: 20px;
    margin: 2px;
    padding: 1px 4px 2px 4px;
}

label.toggle-section-label {
    cursor: pointer;
    display: block
}

input.toggle-section-button {
    outline: 0;
    opacity: 0;
    width: 0;
    height: 0
}

input.toggle-section-button:checked+section.toggle-section {
    display: none
}

  </style>
</head>

<body>
  <section class="main">
    <div id="logo">
      <label for="nav-toggle-cb" id="nav-toggle">&#9776;&nbsp;&nbsp;Settings</label>
    </div>
    <div id="content">
      <div id="sidebar">
        <!-- <input type="checkbox" id="nav-toggle-cb" checked="checked"> -->
        <!-- Hide sidebar when loaded -->
        <input type="checkbox" id="nav-toggle-cb">
        <nav id="menu">


          <div class="input-group" id="framesize-group">
            <label for="framesize">Resolution</label>
            <select id="framesize" class="default-action">
              <!-- 2MP -->
              <!-- <option value="13">UXGA(1600x1200)</option>
              <option value="12">SXGA(1280x1024)</option>
              <option value="11">HD(1280x720)</option>
              <option value="10">XGA(1024x768)</option> -->
              <option value="9">SVGA(800x600)</option>
              <option value="8">VGA(640x480)</option>
              <option value="7">HVGA(480x320)</option>
              <option value="6">CIF(400x296)</option>
              <option value="5">QVGA(320x240)</option>
              <!-- <option value="4">240x240</option>
              <option value="3">HQVGA(240x176)</option>
              <option value="2">QCIF(176x144)</option>
              <option value="1">QQVGA(160x120)</option>
              <option value="0">96x96</option> -->
            </select>
          </div>

          <div class="input-group" id="quality-group">
            <label for="quality">Quality</label>
            <div class="range-min">4</div>
            <input type="range" id="quality" min="4" max="63" value="10" class="default-action">
            <div class="range-max">63</div>
          </div>

          <div class="input-group" id="brightness-group">
            <label for="brightness">Brightness</label>
            <div class="range-min">-2</div>
            <input type="range" id="brightness" min="-2" max="2" value="0" class="default-action">
            <div class="range-max">2</div>
          </div>


          <!-- Toggle stream -->
          <section id="buttons">
            <button id="toggle-stream">Start Stream</button>
          </section>


          <div style="margin-top: 8px;">
          </div>
          <hr style="width:100%">
          <label for="nav-toggle-2640pll" class="toggle-section-label">&#9776;&nbsp;&nbsp;Advanced
            Settings</label><input type="checkbox" id="nav-toggle-2640pll" class="hidden toggle-section-button"
            checked="checked">
          <section class="toggle-section">

            <div class="input-group" id="aec_value-group">
              <label for="aec_value">Exposure</label>
              <div class="range-min">0</div>
              <input type="range" id="aec_value" min="0" max="1200" value="204" class="default-action">
              <div class="range-max">1200</div>
            </div>

            <section id="xclk-section" class="nothidden">
              <div class="input-group" id="set-xclk-group">
                <label for="set-xclk">XCLK MHz</label>
                <div class="text">
                  <input id="xclk" type="text" minlength="1" maxlength="2" size="2" value="20">
                </div>
                <button class="inline-button" id="set-xclk">Set</button>
              </div>
            </section>

            <div class="input-group" id="awb-group">
              <label for="awb">AWB</label>
              <div class="switch">
                <input id="awb" type="checkbox" class="default-action" checked="checked">
                <label class="slider" for="awb"></label>
              </div>
            </div>

            <div class="input-group" id="aec-group">
              <label for="aec">AEC SENSOR</label>
              <div class="switch">
                <input id="aec" type="checkbox" class="default-action" checked="checked">
                <label class="slider" for="aec"></label>
              </div>
            </div>

            <div class="input-group" id="aec2-group">
              <label for="aec2">AEC DSP</label>
              <div class="switch">
                <input id="aec2" type="checkbox" class="default-action" checked="checked">
                <label class="slider" for="aec2"></label>
              </div>
            </div>

            <div class="input-group" id="agc-group">
              <label for="agc">AGC</label>
              <div class="switch">
                <input id="agc" type="checkbox" class="default-action" checked="checked">
                <label class="slider" for="agc"></label>
              </div>
            </div>

            <div class="input-group" id="wpc-group">
              <label for="wpc">WPC</label>
              <div class="switch">
                <input id="wpc" type="checkbox" class="default-action" checked="checked">
                <label class="slider" for="wpc"></label>
              </div>
            </div>

            <div class="input-group" id="led-group">
              <label for="led_intensity">LED Intensity</label>
              <div class="range-min">0</div>
              <input type="range" id="led_intensity" min="0" max="255" value="0" class="default-action">
              <div class="range-max">255</div>
            </div>

            <div class="input-group" id="ae_level-group">
              <label for="ae_level">AE Level</label>
              <div class="range-min">-2</div>
              <input type="range" id="ae_level" min="-2" max="2" value="0" class="default-action">
              <div class="range-max">2</div>
            </div>
          </section>

          <div class="input-group hidden" id="agc_gain-group">
            <label for="agc_gain">Gain</label>
            <div class="range-min">1x</div>
            <input type="range" id="agc_gain" min="0" max="30" value="5" class="default-action">
            <div class="range-max">31x</div>
          </div>

        </nav>
      </div>

      <figure>
        <div id="stream-container" class="image-container ">
          <img id="stream" src="/stream" crossorigin>
        </div>
      </figure>
    </div>
  </section>
  
  <script>
    window.addEventListener('load', onLoad)
    function onLoad(event) {
      baseHost = document.location.origin
      streamUrl = baseHost

      view = document.getElementById('stream')
      viewContainer = document.getElementById('stream-container')

      initButton();
    }
    function initButton() {
      document.getElementById('toggle-stream').addEventListener('click', toggle_stream)
    }
    function toggle_stream(){
      console.log("Toggle clicked")
      console.log(streamUrl)

      view.src = `${streamUrl}/stream`
    }
  </script>
  
</body>
</html>
)rawliteral";

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
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(200, "text/plain", "Hello, world");
  // });

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/status", HTTP_GET, getCameraStatus);
  server.on("/stream", HTTP_GET, streamJpg);
  server.onNotFound(notFound);

  AsyncElegantOTA.begin(&server, OTA_USERNAME,
                        OTA_PASSWORD); // enable OTA update
  server.begin();
}

void loop() {}