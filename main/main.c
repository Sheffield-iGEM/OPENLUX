#include <freertos/FreeRTOS.h>
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_log.h>
// Don't keep this here
#include <driver/adc.h>

// Name of our application
static const char TAG[] = "OpenLUX";
// Where on the file system we are storing web data
static const char WEB[] = "/web";
// This determines the size of the HTTP chunks the ESP is sending
// This can be tweaked for better large-file performance
static const long CHUNK_SIZE = 24576; // 24KiB

// Declare the functions we'll be defining later. These might be separated into
// different files / components in the future.
// Event handlers:
static void on_disconnect(void*, esp_event_base_t, int32_t, void*);
// System initialisation:
static void mount_webdata();
static void wifi_start(char[], char[]);
static httpd_handle_t start_webserver(void);
// URI handlers:
static esp_err_t static_get(httpd_req_t*);
// Helper functions
static char* file_to_mime(char[]);
static void uri_to_path(char*, const char*);
static void send_file_as_chunks(httpd_req_t*, char[]);
// Error handling:
static void die_politely(esp_err_t, char[]);

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

  // !!! Start Dirty Chunk !!!
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC_CHANNEL_0, ADC_ATTEN_DB_0);
  while (true) {
    ESP_LOGI(TAG, "Light reading: %d", adc1_get_raw(ADC1_CHANNEL_0));
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  // !!! End Dirty Chunk!!!
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

// Set up some valid URIs
// URI for handling all remaining GET requests
httpd_uri_t static_get_uri = {
  .uri      = "/*", // Root page starts at / and * is a placeholder for the rest
  .method   = HTTP_GET, // GET means sending data from the server
  .handler  = static_get, // Assign the static_get function to this uri
  .user_ctx = NULL // We aren't sending any extra data to static_get
};

// This is the catch-all handler for static web pages. This function takes a
// pointer to a request and returns ESP_OK if all goes well.
static esp_err_t static_get(httpd_req_t* req) {
  // SPIFFS limits filenames to 32 bytes, so we don't need to worry about
  // anything longer than that.
  char path[32];
  // Extract the URI from the request pointer and convert it into a path
  uri_to_path(path, req->uri);
  // Attempt to set the content-type of the response, exiting on failure
  die_politely(httpd_resp_set_type(req, file_to_mime(path)), "Failed to set response type");
  // Send the requested file back to the client in chunks
  send_file_as_chunks(req, path);
  // Return ESP_OK so the connection isn't killed
  return ESP_OK;
}

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

// This function takes a filename as a char array, tests for different fill
// extensions, and returns the proper mime-type.
static char* file_to_mime(char fn[]) {
  // Test if ".html" is part of the filename
  if (strstr(fn, ".html")) {
    // If so, return the HTML mime-type
    return "text/html";
  } else if (strstr(fn, ".css")) {
    return "text/css";
  } else if (strstr(fn, ".js")) {
    return "application/javascript";
  } else if (strstr(fn, ".png")) {
    return "image/png";
  } else if (strstr(fn, ".jpg") || strstr(fn, ".JPG") || strstr(fn, ".jpeg")) {
    return "image/jpeg";
  } else if (strstr(fn, ".svg")) {
    return "image/svg+xml";
  } else {
    // Unsupported file-types default to octet-streams
    return "application/octet-stream";
  }
}

// This function takes two strings (char pointers), the first is a buffer for
// constructing the path and the second is the request URI. This function adds
// the WEB prefix so that data can be accessed via SPIFFS and also adds an
// implicit index.html to URIs without extensions. The resultant path is stored
// in path_buf.
static void uri_to_path(char* path_buf, const char* uri) {
  // Start by copying the WEB prefix into the path
  strcpy(path_buf, WEB);
  // Add on the URI
  strcat(path_buf, uri);
  // Check if the current path contains a dot. This is indicative of a file
  // extension, but isn't foolproof. It'll do for now though
  if (!strchr(path_buf, '.')) {
    // Check if the last character is a slash, if not, add one.
    if (path_buf[strlen(path_buf) - 1] != '/') {
      // Add a trailing slash
      strcat(path_buf, "/");
    }
    // Add the implied index.html to the end of the path
    strcat(path_buf, "index.html");
  }
}

// This function takes a request pointer and file path (as a char array) to
// serve back to the client
static void send_file_as_chunks(httpd_req_t* req, char path[]) {
  // Kick things off by opening the requested file as read-only
  FILE* fp = fopen(path, "r");
  // If fopen failed, let the user know and return early.
  if (!fp) {
    // Log which file couldn't be opened
    ESP_LOGE(TAG, "Failed to open %s", path);
    return;
  }
  // Attempt to allocate a chunk of memory to read the file into
  char* msg = malloc(CHUNK_SIZE);
  // If allocation failed, let the user know and return early
  if (!msg) {
    // Log the amount of memory that was requested for allocation
    ESP_LOGE(TAG, "Failed to allocate %ld bytes on the heap", CHUNK_SIZE);
    return;
  }
  // Read a chunk of the file into msg and record the number of bytes read. 1 is
  // the size of each array element in msg (a char is 1 byte) and CHUNK_SIZE
  // tells fread how many of these elements to read.
  long bytes_read = fread(msg, 1, CHUNK_SIZE, fp);
  // Did everything fit into one chunk?
  if (bytes_read < CHUNK_SIZE) {
    // If so, just send a complete response, reporting and handling any failure
    die_politely(httpd_resp_send(req, msg, bytes_read), "Failed to send HTTP response");
    // Log the size of the response that was sent
    ESP_LOGI(TAG, "Sent %s in %ld bytes", path, bytes_read);
  } else {
    // Otherwise, send the response in chunks.
    // Use this to record the number of chunks sent (for logging purposes)
    unsigned int chunks = 0;
    // Send chunks so long as data is still being read
    while (bytes_read > 0) {
      // Send a chunk of data, reporting and handling any failure
      die_politely(httpd_resp_send_chunk(req, msg, bytes_read), "Failed to send chunked HTTP response");
      // Log the size of the chunk that was sent
      ESP_LOGI(TAG, "Sent a chunk of %ld bytes from %s", bytes_read, path);
      // Update the number of chunks sent
      chunks++;
      // Read in the next chunk of data and report the number of bytes read
      bytes_read = fread(msg, 1, CHUNK_SIZE, fp);
    }
    // Once all chunks have been sent, chunk transmission must be terminated
    // with a transmission of zero bytes. Don't forget to catch errors...
    die_politely(httpd_resp_send_chunk(req, "", 0), "Failed to terminate chunked HTTP response");
    // Log the number of chunks and bytes sent
    ESP_LOGI(TAG, "Sent %s in %ld bytes (%d chunks)", path,
             chunks * CHUNK_SIZE - bytes_read, chunks);
  }
  // The msg buffer is no longer needed, so the memory is freed
  free(msg);
  // The file pointer is no longer needed, so the file is closed
  fclose(fp);
}

// This function does not return a value
// This function takes the result of an action (esp_err_t) and checks whether
// an error occurred. If so, the second argument, a string (char pointer / array)
// is displayed as an error before restarting the system (after a delay)
static void die_politely(esp_err_t err, char msg[]) {
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
