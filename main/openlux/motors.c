#include "motors.h"

static const gpio_num_t DATA = GPIO_NUM_18;
static const gpio_num_t CLK = GPIO_NUM_21;
static const gpio_num_t LATCH = GPIO_NUM_19;

static const int STEP_PERIOD = 3;
static const int WELL_SPACING = 460;
static const int R_OFFSET = 275;
static const int C_OFFSET = 255;

static int R_TAR = 0;
static int C_TAR = 0;
static int R_POS = 0;
static int C_POS = 0;

void home_motors() {
  set_status(HOMING);
  ESP_LOGI(TAG, "Device is homing...");
  ESP_LOGI(TAG, "Row");
  drive_motors(LOWER_MOTORS, -4000, 2);
  ESP_LOGI(TAG, "Column");
  drive_motors(UPPER_MOTORS, -6000, 2);
  drive_motors(LOWER_MOTORS, R_OFFSET, STEP_PERIOD);
  drive_motors(UPPER_MOTORS, C_OFFSET, STEP_PERIOD);
  shift_byte(0x00);
  ESP_LOGI(TAG, "Homed!");
  revert_status();
  R_POS = 0;
  C_POS = 0;
}

// Make these args meaningful and kill the global vars
void goto_loop(void* args) {
  while (true) {
    int r_err = R_TAR - R_POS;
    int c_err = C_TAR - C_POS;
    if (r_err) {
      drive_motors(LOWER_MOTORS, r_err, STEP_PERIOD);
      set_status(MOVING);
      R_POS += r_err;
    } else if (c_err) {
      drive_motors(UPPER_MOTORS, c_err, STEP_PERIOD);
      set_status(MOVING);
      C_POS += c_err;
    } else if (get_status() == MOVING) {
      ESP_LOGI(TAG, "Done moving!");
      revert_status();
      shift_byte(0x00);
    }
    vTaskDelay(5); // Find a meaningful number to put here
  }
}

void goto_coord(int row, int col) {
  ESP_LOGI(TAG, "Moving to row %d and column %d...", row, col);
  R_TAR = (row - 1) * WELL_SPACING;
  C_TAR = (col - 1) * WELL_SPACING;
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

// This should be combined with another function...
void start_goto_loop() {
  TaskHandle_t goto_handle = NULL;
  xTaskCreate(goto_loop, "MOTOR_MOVEMENT", 4096, NULL, 4, &goto_handle);
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
