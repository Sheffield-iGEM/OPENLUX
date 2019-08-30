#include "openlux/sensors.h"
#include "openlux/common.h"
#include "openlux/motors.h"
#include "openlux/web.h"
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <esp_wifi.h>

// Declare the functions we'll be defining later
// Event handlers:
static void on_disconnect(void*, esp_event_base_t, int32_t, void*);
// System initialisation:
static void mount_webdata();
static void wifi_start(char[], char[]);

// This is the entry-point to our program
void app_main(void) {
  // Initialise the default event loop. The WiFi subsystem (and other built-in
  // APIs) post events to this event loop. This allows user-defined functions to
  // be run in response to system events
  die_politely(esp_event_loop_create_default(), "Failed to set up the default event loop");
  
  // Register some event handlers to the default loop
  die_politely(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                     &on_disconnect, NULL),
          "Failed to register disconnect handler");

  // Mount the SPIFFS filesystem so that web resources are accessible
  mount_webdata();
  
  // Connect to WiFi. Currently the network SSID and password are set in an
  // external configuration program, but could be set here directly.
  wifi_start(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

  // This kicks off the server!
  start_webserver();

  // This function call starts polling the light sensor (which is connected to
  // the first analogue channel (ADC1_CHANNEL_0), the range of the channel is
  // set to 0-1.1V (ADC_ATTEN_DB_0), and the sampling period is 200ms.
  start_sensor_polling(ADC1_CHANNEL_0, ADC_ATTEN_DB_0, 200);
  
  // !!! DIRTY CHUNK !!!
  setup_motor_driver();
  // home_motors();
  start_goto_loop();
  ESP_LOGI(TAG, "Initialised!");
  set_status(READY);
  // !!! DIRTY CHUNK !!!
}

// The static means this function can only be used from inside this file
// This function does not return a value
// This function takes four arguments:
// 1) The arg value is a generic pointer to a piece of data passed during
//    handler registration
// 2) event_base is the class of event / event origin
// 3) event_id is the specific event that occurred
// 4) event_data is another generic pointer that holds some additional
//    information about the event
static void on_disconnect(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  // Not doing anything fancy here, if we are disconnected, warn the user and
  // unconditionally attempt to reconnect.
  ESP_LOGW(TAG, "WiFi is disconnected. Attempting to reconnect now...");
  // Attempt to connect to the previously specified SSID
  die_politely(esp_wifi_connect(), "Failed to connect to the given SSID");
}

// This function does not return any value
// It takes a string that determines the mount location of the webdata partition
// and attempt to mount SPIFFS there.
static void mount_webdata() {
  // Here is the Virtual FileSystem configuration
  esp_vfs_spiffs_conf_t config = {
    .base_path = WEB, // This is the location that webdata is mounted
    .partition_label = "webdata", // This tells VFS we are mounting webdata
    .max_files = 8, // Only allow 8 files to be open simultaneously
    .format_if_mount_failed = false // Don't format the partition if mounting fails
  };

  // Attempt to mount the partition and inform the user of any failure
  die_politely(esp_vfs_spiffs_register(&config), "Failed to mount SPIFFS Partition");
}

// This function does not return a value
// Connects to the WiFi network described by the SSID and password provided
static void wifi_start(char ssid[], char pass[]) {
  // This initialises the Non-Volitile Storage partition
  // This API stores key-value pairs on the flash and is required by esp_wifi_init()
  nvs_flash_init();
  // Initialises the TCP/IP stack so that networking can take place
  tcpip_adapter_init();

  // Get the default WiFi configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // Attempt to start the WiFi Driver, signalling error on failure
  die_politely(esp_wifi_init(&cfg), "The WiFi driver failed to start");
  // Attempt to set the WiFi mode to Station mode (ESP32 is a client)
  die_politely(esp_wifi_set_mode(WIFI_MODE_STA), "Failed to set the ESP32 as a WiFi client");

  // Initialise a struct to hold the station (sta) config
  // FIXME: Do this with struct ...? Get rid of the literal.
  wifi_config_t config = { .sta = { } };
  // Set the network name (SSID)
  memcpy(config.sta.ssid, ssid, strlen(ssid));
  // Set the network password
  memcpy(config.sta.password, pass, strlen(pass));
  
  // Set the WiFi configuration as a station (WIFI_IF_STA)
  die_politely(esp_wifi_set_config(WIFI_IF_STA, &config), "Invalid WiFi Configuration");
  // Start WiFi
  die_politely(esp_wifi_start(), "Failed to start the WiFi subsystem");
  // Attempt to connect to the previously specified SSID
  die_politely(esp_wifi_connect(), "Failed to connect to the given SSID");
}
