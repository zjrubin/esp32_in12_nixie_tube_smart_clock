#include "ntp.h"

#include <Arduino.h>
#include <WiFi.h>

#include "arduino_debug.h"
#include "time.h"

const char* const c_wifi_ssid = "YOUR_WIFI_SSID";
const char* const c_wifi_password = "YOUR_WIFI_PASSWORD";
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
  configTime(c_gmt_offset_sec, c_daylight_offset_sec, c_ntp_server);
  print_local_time();

  // disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void print_local_time() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
