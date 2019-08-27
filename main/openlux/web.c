#include "sensors.h"
#include "motors.h"
#include "common.h"
#include "web.h"

// This determines the size of the HTTP chunks the ESP is sending
// This can be tweaked for better large-file performance
static const long CHUNK_SIZE = 24576; // 24KiB

// Declare some static functions we'll be defining later.
// URI handlers:
static esp_err_t status_get(httpd_req_t*);
static esp_err_t command_post(httpd_req_t*);
static esp_err_t static_get(httpd_req_t*);
// Helper functions:
static char* file_to_mime(char[]);
static void uri_to_path(char*, const char*);
static void send_file_as_chunks(httpd_req_t*, char[]);

// Set up some valid URIs
httpd_uri_t status_get_uri = {
  .uri      = "/status", // Sensor readings & status live here
  .method   = HTTP_GET, // We are sending data from the server
  .handler  = status_get, // Assign the status_get function to this uri
  .user_ctx = NULL  // No extra data needed
};

httpd_uri_t command_post_uri = {
  .uri      = "/command",
  .method   = HTTP_POST,
  .handler  = command_post,
  .user_ctx = NULL
};

// URI for handling all remaining GET requests
httpd_uri_t static_get_uri = {
  .uri      = "/*", // Root page starts at / and * is a placeholder for the rest
  .method   = HTTP_GET, // GET means sending data from the server
  .handler  = static_get, // Assign the static_get function to this uri
  .user_ctx = NULL // We aren't sending any extra data to static_get
};

// This handler is very simple. When it receives a request, it sends back the
// latest available sensor reading and device status. It takes a pointer to the
// request (so that it can respond to it) and returns ESP_OK if all goes well.
static esp_err_t status_get(httpd_req_t* req) {
  // 4 (sensor) + 1 (;) + 1 (status) + 1 (null)
  char msg[7];
  sprintf(msg, "%d;%d", get_status(), get_sensor_value());
  // Our text isn't HTML, so just set the response type to text/plain
  die_politely(httpd_resp_set_type(req, "text/plain"), "Failed to set response type");
  // Send the value back in an HTTP response, handling failure
  die_politely(httpd_resp_send(req, msg, strlen(msg)), "Failed to send HTTP response");
  // Return ESP_OK so the connection isn't killed
  return ESP_OK;
}

static esp_err_t command_post(httpd_req_t* req) {
  char data[req->content_len + 1];
  httpd_req_recv(req, data, req->content_len);
  //data[sizeof(data) - 1] = '\0';
  char *coord = strtok(data, ";");
  int led = atoi(strtok(NULL, ";"));
  int row = atoi(strtok(coord, ","));
  int col = atoi(strtok(NULL, ","));
  goto_coord(row,col);
  set_led(led);
  httpd_resp_send(req, "", 0);
  return ESP_OK;
}

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
httpd_handle_t start_webserver(void) {
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
    httpd_register_uri_handler(server, &status_get_uri);
    httpd_register_uri_handler(server, &command_post_uri);
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
