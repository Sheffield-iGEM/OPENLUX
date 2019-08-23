#include <driver/adc.h>

#ifndef SENSORS_H
#define SENSORS_H
// System initialisation
extern void start_sensor_polling(adc1_channel_t, adc_atten_t, unsigned int);
// Getters
extern int get_sensor_value(void);
// Setters
extern void toggle_led(void);
#endif
