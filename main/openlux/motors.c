#include "motors.h"

static const gpio_num_t DATA = GPIO_NUM_16;
static const gpio_num_t CLK = GPIO_NUM_17;
static const gpio_num_t LATCH = GPIO_NUM_18;

void setup_motor_driver() {
  // Error handle me...
  gpio_pad_select_gpio(DATA);
  gpio_pad_select_gpio(CLK);
  gpio_pad_select_gpio(LATCH);
  gpio_set_direction(DATA, GPIO_MODE_OUTPUT);
  gpio_set_direction(CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction(LATCH, GPIO_MODE_OUTPUT);
}
  
void shift_byte(char byte) {
  gpio_set_level(LATCH, 0);
  for (int n = 0; n < 8; n++) {
    // Get the nth bit
    uint32_t bit = ((byte & (1 << n)) == 0) ? 0 : 1;
    gpio_set_level(DATA, bit);
    gpio_set_level(CLK, 1);
    gpio_set_level(CLK, 0);
  }
  gpio_set_level(LATCH, 1);
}

void drive_motors(motor_set_t ms, int stp, int per) {
  char mask = (ms == LOWER_MOTORS) ? 0x0F : 0xF0;
  for (int n = 0; n < abs(stp); n++) {
    int sh = (stp > 0) ? n % 4 : 3 - n % 4;
    char byte = ((1 << (sh + 4)) + (1 << sh)) & mask;
    vTaskDelay(per / portTICK_PERIOD_MS);
    shift_byte(byte);
  }
}
