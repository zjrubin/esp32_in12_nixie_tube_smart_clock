#pragma once

#define NTP_OK 0
#define NTP_ERROR -1

extern const char* const c_wifi_ssid;
extern const char* const c_wifi_password;
extern const char* const c_ntp_server;
extern const long int c_gmt_offset_sec;
extern const int c_daylight_offset_sec;

void set_time_from_ntp();

int print_local_time();
