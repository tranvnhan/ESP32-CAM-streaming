#ifndef A4EC03FC_88BF_428E_8465_FA8B3C42AC20
#define A4EC03FC_88BF_428E_8465_FA8B3C42AC20

#include <esp_camera.h>
#include <ESPAsyncWebServer.h>

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART =
    "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";
static const char *JPG_CONTENT_TYPE = "image/jpeg";

static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

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


void streamJpg(AsyncWebServerRequest *request);
void cameraInit();
void cameraStop();

#endif /* A4EC03FC_88BF_428E_8465_FA8B3C42AC20 */
