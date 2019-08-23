#include "sensors.h"
#include "common.h"
#include <math.h>

typedef struct poll_args {
  adc1_channel_t channel;
  unsigned int samples;
} poll_args;

// Purge this, make it an argument
static const int LED = 17;

static int SENSOR_VALUE = 1;
static int LED_STATUS = 0;
static void poll_avg(void*);

// Return the task handle
void start_sensor_polling(adc1_channel_t ch, adc_atten_t atn, unsigned int per) {
  die_politely(adc1_config_width(ADC_WIDTH_BIT_12), "Failed to set ADC bus width");
  die_politely(adc1_config_channel_atten(ch, atn), "Failed to set channel attenuation");
  // Fail check this
  gpio_pad_select_gpio(LED);
  gpio_set_direction(LED, GPIO_MODE_OUTPUT);
  ESP_LOGI(TAG, "Started sensor polling on channel %d at %.2fHz", ch, 1000/((double) per));
  poll_args* args = (poll_args*) malloc(sizeof(poll_args));
  args->channel = ch;
  args->samples = per / (unsigned int) portTICK_PERIOD_MS;
  TaskHandle_t poll_handle = NULL;
  xTaskCreate(poll_avg, "SENSOR_POLLING", 4096, args, 3, &poll_handle);
}

int get_sensor_value(void) {
  return SENSOR_VALUE;
}

void toggle_led() {
  LED_STATUS = !LED_STATUS;
  gpio_set_level(LED, LED_STATUS);
  DEVICE_STATUS = (LED_STATUS) ? READING : READY;
}
  
static void poll_avg(void* args) {
  poll_args* opts = (poll_args*)args;
  while (true) {
    float avg = 0;
    for (int i = 0; i < opts->samples; i++) {
      avg += adc1_get_raw(opts->channel);
      vTaskDelay(1);
    }
    SENSOR_VALUE = round(avg / opts->samples);
  }
}
