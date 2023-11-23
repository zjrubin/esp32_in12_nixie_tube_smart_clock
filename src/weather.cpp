#include "weather.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "arduino_debug.h"
#include "credentials.h"
#include "ntp.h"

bool get_local_temperature(double *temperature) {
  if (!connect_to_wifi()) {
    return false;
  }

  IPAddress dns_ip = WiFi.dnsIP();
  debug_serial_printf("DNS IP: %s\n", dns_ip.toString().c_str());

  // Get the location-based on the IP address
  HTTPClient locationHttp;
  locationHttp.begin("http://ip-api.com/json");

  bool got_location = false;
  for (int i = 0; i < 10; i++) {
    int locationHttpCode = locationHttp.GET();
    if (locationHttpCode == 200) {
      got_location = true;
      break;
    }
    delay(500);
  }

  if (!got_location) {
    // TODO: error handling
    return false;
  }

  String lat;
  String lon;
  {
    StaticJsonDocument<5000> locationDoc;

    deserializeJson(locationDoc, locationHttp.getString());

    lat = locationDoc["lat"].as<String>();
    lon = locationDoc["lon"].as<String>();
  }

  debug_serial_printf("lat: %s\tlon: %s\n", lat.c_str(), lon.c_str());

  // Get the weather information
  WiFiClientSecure https_client;
  HTTPClient weatherHttp;
  String api_url =
      "https://api.openweathermap.org/data/3.0/onecall?lat=" + String(lat) +
      "&lon=" + String(lon) +
      "&exclude=minutely,hourly,daily,alerts&units=imperial&appid=" +
      c_open_weather_api_key;

  weatherHttp.begin(https_client, api_url);

  bool got_weather = false;
  for (int i = 0; i < 10; i++) {
    int weatherHttpCode = weatherHttp.GET();
    if (weatherHttpCode == 200) {
      got_weather = true;
      break;
    }
    delay(500);
  }

  if (!got_weather) {
    // TODO: error handling
    return false;
  }

  {
    StaticJsonDocument<5000> weatherDoc;
    deserializeJson(weatherDoc, weatherHttp.getString());
    *temperature = weatherDoc["current"]["temp"];
  }

  debug_serial_printf("Temperature: %.2f °F\n", *temperature);

  locationHttp.end();
  weatherHttp.end();
  disconnect_from_wifi();

  UBaseType_t StackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  debug_serial_printf("StackHighWaterMark: %u\n", StackHighWaterMark);

  return true;
}
