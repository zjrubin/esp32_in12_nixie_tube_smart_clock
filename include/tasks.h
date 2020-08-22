#pragma once

#include <Arduino.h>

extern TaskHandle_t g_task_special_modes_handle;
extern TaskHandle_t g_task_configure_handle;
extern TaskHandle_t g_task_blink_dot_separators_handle;
extern TaskHandle_t g_task_display_time_handle;
extern TaskHandle_t g_task_display_date_handle;
