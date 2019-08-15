#include <freertos/FreeRTOS.h>
#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_log.h>

#ifndef COMMON_H
#define COMMON_H
// Global constants:
extern const char* TAG;
extern const char* WEB;
// Error handling:
extern void die_politely(esp_err_t, char[]);
#endif
