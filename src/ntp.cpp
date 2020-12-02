#include "ntp.h"

#include <Arduino.h>
#include <WiFi.h>

#include "arduino_debug.h"
#include "time.h"

// const char* const c_wifi_ssid = "WRONG SSID";
const char* const c_wifi_ssid = "Home Network";
const char* const c_wifi_password = "k38had53";
const char* const c_ntp_server = "pool.ntp.org";
const long int c_gmt_offset_sec = -18000;
const int c_daylight_offset_sec = 3600;

void set_time_from_ntp() {
  // connect to WiFi
  debug_serial_printf("Connecting to %s ", c_wifi_ssid);
  WiFi.begin(c_wifi_ssid, c_wifi_password);

  // Wait up to 30 seconds for the wifi to connect
  bool connected = false;
  for (int i = 0; i < 60; ++i) {
    switch (WiFi.status()) {
      case WL_CONNECTED:
        connected = true;
        break;

      default:
        debug_serial_print(".");
        delay(500);
        break;
    }
  }

  if (!connected) {
    // If not connected, just use the previously set time
    debug_serial_println(" FAILED");
    debug_serial_printfln(
        "Failed to connect to wifi to set time from NTP. Defaulting to "
        "previously set time\n\tssid: %s\n\tpassword: %s\n",
        c_wifi_ssid, c_wifi_password);
    return;
  }

  debug_serial_println(" CONNECTED");

  // init and get the time
  bool ntp_time_configured = false;
  for (int i = 1; i <= 30; ++i) {
    configTime(c_gmt_offset_sec, c_daylight_offset_sec, c_ntp_server);
    vTaskDelay((1000 * i) / portTICK_PERIOD_MS);  // Linear backoff

    if (print_local_time() == NTP_OK) {
      ntp_time_configured = true;
      break;
    }
  }

  if (!ntp_time_configured) {
    debug_serial_printfln(
        "Failed to configure the time from the provided NTP server: "
        "%s",
        c_ntp_server);
  }

  // disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

int print_local_time() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return NTP_ERROR;
  }

  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  return NTP_OK;
}
