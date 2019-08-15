#include "common.h"

// Name of our application
const char* TAG = "OpenLUX";
// Where on the file system we are storing web data
const char* WEB = "/web";

// This function does not return a value
// This function takes the result of an action (esp_err_t) and checks whether
// an error occurred. If so, the second argument, a string (char pointer / array)
// is displayed as an error before restarting the system (after a delay)
void die_politely(esp_err_t err, char msg[]) {
  // Did the attempted action fail? (non-zero error code)
  if (err) {
    // Log the user-provided error message
    ESP_LOGE(TAG, "%s", msg);
    // Follow up with the received error name and code
    ESP_LOGE(TAG, "The error code was: %s (0x%x)", esp_err_to_name(err), err);
    // Start the restart countdown. Currently the delay is 5 seconds
    for (int t = 5; t > 0; t--) {
      // Display the countdown as info in the log
      ESP_LOGI(TAG, "System will restart in %d seconds...", t);
      // Sleep for one second before continuing
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    // Restart the ESP32
    esp_restart();
  }
}
