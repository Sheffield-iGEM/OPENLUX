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
// Status getting and setting:
extern void set_status(status_t);
extern void revert_status(void);
extern status_t get_status(void);
// Error handling:
extern void die_politely(esp_err_t, char[]);
// Kill me
int SYNC_KEY;
#endif
