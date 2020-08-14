#pragma once

extern const char* const c_wifi_ssid;
extern const char* const c_wifi_password;
extern const char* const c_ntp_server;
extern const long int c_gmt_offset_sec;
extern const int c_daylight_offset_sec;

void set_time_from_ntp();

void print_local_time();
