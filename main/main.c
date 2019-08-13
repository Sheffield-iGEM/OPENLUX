#include <freertos/FreeRTOS.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <sys/stat.h>

// Name of our application
static const char TAG[] = "OpenLUX";
// Where on the file system we are storing web data
static const char WEB[] = "/web";
// This determines the size of the HTTP chunks the ESP is sending
static const long CHUNK_SIZE = 4096;

// Declare the functions we'll be defining later. I might move these functions
// into other files at some point in the future.
static void attempt(esp_err_t, char[]);
static void die_slowly(void); // ;-;
static void wifi_start(char[], char[]);
static void mount_webdata();
static char* read_file(char[]);
static esp_err_t static_get(httpd_req_t*);
static httpd_handle_t start_webserver(void);
static void on_disconnect(void*, esp_event_base_t, int32_t, void*);

// !!! Beginning of the rubbish zone !!!
static void die_slowly(void) {
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

static char* malloc_null_str() {
  char* str = malloc(1);
  str[0] = '\0';
  return str;
}

static char* read_file(char fn[]) {
  FILE* fp = fopen(fn, "r");
  if (!fp) {
    ESP_LOGE(TAG, "Failed to open %s", fn);
    return malloc_null_str();
  }
  struct stat s;
  fstat(fileno(fp), &s);
  ESP_LOGI(TAG, "Loaded %ld bytes from %s", s.st_size, fn);
  // Here be ghosts... (No idea why +1 sometimes crashes it)
  char* str = malloc(s.st_size + 2);
  if (!str) {
    ESP_LOGE(TAG, "Failed to allocate %ld bytes on the heap", s.st_size + 2);
    return malloc_null_str();
  } 
  fread(str, 1, s.st_size, fp);
  str[s.st_size + 1] = '\0';
  fclose(fp);
  return str;
}

// Comment me!
static esp_err_t static_get(httpd_req_t* req) {
  // Allloc buffer here Send ref to read_file. Set global chunk size. Return
  // real size as long.  Actually, get rid of read_file and load the file
  // here. Use httpd_resp_send_chunk To gradually send data until we reach the
  // end of the file (EOF). Also write a uri to mime type so setting headers
  // with httpd_resp_send. Do a while (fread). It returns the bytes read.
  char path[HTTPD_MAX_URI_LEN + strlen(WEB) + 1];
  char* path_ptr = (char*) &path;
  strcpy(path_ptr, WEB);
  strcat(path_ptr, req->uri);
  /*if (path[strlen(WEB) + strlen(req->uri) - 1] == '/') {
    strcat(path_ptr, "index.html");
    }*/
  if (!strchr(path, '.')) {
    if (path[strlen(WEB) + strlen(req->uri) - 1] != '/') {
      strcat(path_ptr, "/");
    }
    strcat(path_ptr, "index.html");
  }
  ESP_LOGW(TAG, "Path: %s", path);
  ESP_LOGW(TAG, "End: %c", path[strlen(WEB) + strlen(req->uri) - 1]);
  char* msg = read_file(path);
  attempt(httpd_resp_send(req, msg, strlen(msg) - 1), "Failed to send HTTP response");
  free(msg);
  return ESP_OK;
}

// !!! End of rubbish zone !!!

// This is the entry-point to our program
void app_main(void) {
  // Initialise the default event loop. The WiFi subsystem (and other built-in
  // APIs) post events to this event loop. This allows user-defined functions to
  // be run in response to system events
  attempt(esp_event_loop_create_default(), "Failed to set up the default event loop");
  
  // Register some event handlers to the default loop
  attempt(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                     &on_disconnect, NULL),
          "Failed to register disconnect handler");

  // Mount the SPIFFS filesystem so that web resources are accessible
  mount_webdata();
  
  // Connect to WiFi. Currently the network SSID and password are set in an
  // external configuration program, but could be set here directly.
  wifi_start(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

  // This kicks off the server!
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
    // Restart the ESP after a delay
    die_slowly();
  }
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

// This function does not return any value
// It takes a string that determines the mount location of the webdata partition
// and attempts to mount SPIFFS there.
static void mount_webdata() {
  // Here is the Virtual FileSystem configuration
  esp_vfs_spiffs_conf_t config = {
    .base_path = WEB, // This is the location that webdata is mounted
    .partition_label = "webdata", // This tells VFS we are mounting webdata
    .max_files = 8, // Only allow 8 files to be open simultaneously
    .format_if_mount_failed = false // Don't format the partition if mounting fails
  };

  // Attempt to mount the partition and inform the user of any failure
  attempt(esp_vfs_spiffs_register(&config), "Failed to mount SPIFFS Partition");
}

// Set up some valid URIs
// URI for handling all remaining GET requests
httpd_uri_t static_get_uri = {
  .uri      = "/*", // Root page starts at / and * is a placeholder for the rest
  .method   = HTTP_GET, // GET means sending data from the server
  .handler  = static_get, // Assign the static_get function to this uri
  .user_ctx = NULL // We aren't sending any extra data to static_get
};

// This function returns a handle (pointer) to the server
// The (void) tells C this function has no parameters
static httpd_handle_t start_webserver(void) {
  // This creates an empty variable that will eventually store the server handle
  httpd_handle_t server = NULL;
  // Most of the defaults are fine for now
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // For static web serving, wildcard URIs are important. Let's enable those...
  config.uri_match_fn = httpd_uri_match_wildcard;
  
  // Log status and port number to the user
  ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
  // Try starting the server
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers here
    httpd_register_uri_handler(server, &static_get_uri);
    return server;
  }
  // If we haven't returned by now, starting the server failed. Let the user know:
  ESP_LOGI(TAG, "Failed to start server!");
  return NULL;
}

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
  attempt(esp_wifi_connect(), "Failed to connect to the given SSID");
}
