#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_http_server.h>

// Name of our application
static const char TAG[] = "OpenLUX";

// Declare the functions we'll be defining later
static void attempt(esp_err_t, char[]);
static void wifi_auth(char[], char[]);
static httpd_handle_t start_webserver(void);

// This is the entry-point to our program
void app_main(void) {  
  // Connect to WiFi. Currently the network SSID and password are set in an
  // external configuration program, but could be set here directly.
  wifi_auth(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

  // This kicks off the server. It doesn't do much yet, but once it's looped
  // into the default event loop and has some handlers attached, we can get
  // things rolling!
  start_webserver();
}

// The static means this function can only be used from inside this file
// This function does not return a value
// This function takes the result of an action (esp_err_t) and checks whether
// an error occurred. If so, the second argument, a string (char pointer / array)
// is displayed as an error before restarting the system (after a delay)
static void attempt(esp_err_t err, char msg[]) {
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

// The static means this function can only be used from inside this file
// This function does not return a value
// Connects to the WiFi network described by the SSID and password provided
static void wifi_auth(char ssid[], char pass[]) {
  // This initialises the Non-Volitile Storage partition
  // This API stores key-value pairs on the flash and is required by esp_wifi_init()
  nvs_flash_init();
  // Initialises the TCP/IP stack so that networking can take place
  tcpip_adapter_init();

  // Get the default WiFi configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // Attempt to start the WiFi Driver, signalling error on failure
  attempt(esp_wifi_init(&cfg), "The WiFi driver failed to start");
  // Attempt to set the WiFi mode to Station mode (ESP32 is a client)
  attempt(esp_wifi_set_mode(WIFI_MODE_STA), "Failed to set the ESP32 as a WiFi client");

  // Initialise a struct to hold the station (sta) config
  // FIXME: Do this with struct ...? Get rid of the literal.
  wifi_config_t config = { .sta = { } };
  // Set the network name (SSID)
  memcpy(config.sta.ssid, ssid, strlen(ssid));
  // Set the network password
  memcpy(config.sta.password, pass, strlen(pass));
  
  // Set the WiFi configuration as a station (WIFI_IF_STA)
  attempt(esp_wifi_set_config(WIFI_IF_STA, &config), "Invalid WiFi Configuration");
  // Start WiFi
  attempt(esp_wifi_start(), "Failed to start the WiFi subsystem");
  // Attempt to connect to the previously specified SSID
  attempt(esp_wifi_connect(), "Failed to connect to the given SSID");
}

// The static means this function can only be used from inside this file
// This function returns a handle (pointer) to the server
// The (void) tells C this function has no parameters
static httpd_handle_t start_webserver(void) {
  // This creates an empty variable that will eventually store the server handle
  httpd_handle_t server = NULL;
  // The default server configuration is fine for now
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Log status and port number to the user
  ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
  // Try starting the server
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers here
    return server;
  }
  // If we haven't returned by now, starting the server failed. Let the user know:
  ESP_LOGI(TAG, "Failed to start server!");
  return NULL;
}
