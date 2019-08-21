#include <freertos/FreeRTOS.h>
#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_log.h>

#ifndef COMMON_H
#define COMMON_H
// Global constants:
extern const char* TAG;
extern const char* WEB;
// Global Types:
typedef enum status {
  INITIALISING,
  READY,
  HOMING,
  MOVING,
  READING
} status_t;
// Global State:
// Maybe change this to getters and setters?
extern status_t DEVICE_STATUS;
// Error handling:
extern void die_politely(esp_err_t, char[]);
#endif
