#include "motors.h"

static const gpio_num_t DATA = GPIO_NUM_18;
static const gpio_num_t CLK = GPIO_NUM_19;
static const gpio_num_t LATCH = GPIO_NUM_21;

static const int STEP_PERIOD = 3;
static const int WELL_SPACING = 460;
static const int R_OFFSET = 250;
static const int C_OFFSET = 260;

static int R_POS = 0;
static int C_POS = 0;

void home_motors() {
  drive_motors(LOWER_MOTORS, -4000, 2);
  drive_motors(UPPER_MOTORS, -6000, 2);
  drive_motors(LOWER_MOTORS, R_OFFSET, 2);
  drive_motors(UPPER_MOTORS, C_OFFSET, 2);
  R_POS = 0;
  C_POS = 0;
  ESP_LOGI(TAG, "Homed!");
  shift_byte(0x00);
}

void goto_coord(int row, int col) {
  ESP_LOGI(TAG, "Goto row %d, column %d", row, col);
  int r_steps = (row - 1) * WELL_SPACING - R_POS;
  int c_steps = (col - 1) * WELL_SPACING - C_POS;
  drive_motors(LOWER_MOTORS, r_steps, STEP_PERIOD);
  drive_motors(UPPER_MOTORS, c_steps, STEP_PERIOD);
  R_POS += r_steps;
  C_POS += c_steps;
  ESP_LOGI(TAG, "Done moving!");
  shift_byte(0x00);
}

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
