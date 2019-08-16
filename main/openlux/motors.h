#include <driver/gpio.h>
#include "common.h"

#ifndef MOTORS_H
#define MOTORS_H
// System initialisation
extern void setup_motor_driver();
// Hide this
extern void shift_byte(char);
typedef enum motor_set { LOWER_MOTORS, UPPER_MOTORS } motor_set_t;
extern void drive_motors(motor_set_t, int, int);
#endif
