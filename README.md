# ESP32-CAM-streaming

## Desription
The camera stream for 3D printer.

## TODO
- [ ] Multi-clients stream (using FreeRTOS)
- [ ] LED Flash:
  - [ ] signal when device initializes successfully (2 or 3 blinks)
  - [ ] signal when received streaming request
  - [ ] allow to turn on/off via website's sidebar
- [x] OTA upload (currently has issue with uploading, seems like the file size is too big?)
- [ ] Save WiFi credentials in ESP32's filesystem, or using WiFi Manager library
- [ ] 3D print the case: can pivot
- [ ] Build website
  - [ ] Can adjust stream settings via sidebar
- [ ] Deep sleep to save power, wakeup by http request
- [x] Async stream

## References
- https://github.com/me-no-dev/ESPAsyncWebServer/issues/647

