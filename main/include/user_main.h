#include <stdio.h>
#include "esp_system.h"

#include "portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/adc.h"

#include "esp8266/rtc_register.h"
#include "internal/esp_system_internal.h"
#include "esp_wifi.h"
#include "string.h"
#include "utils.h"
#include "event_groups.h"
#include "global_definitions.h"
#include "malloc_logger.h"

// components
#include "ota.h"

#ifndef MAIN_HEADER
#define MAIN_HEADER

#define AP_CONNECTION_STATUS_LED_PIN         GPIO_NUM_12
#define SERVER_AVAILABILITY_STATUS_LED_PIN   GPIO_NUM_14
#define RELAY_DOWN_PIN                       GPIO_NUM_4
#define RELAY_UP_PIN                         GPIO_NUM_5

#define SERVER_IS_AVAILABLE_FLAG
#define FIRST_STATUS_INFO_SENT_FLAG (1 << 0)
#define UPDATE_FIRMWARE_FLAG        (1 << 1)
#define REQUEST_ERROR_OCCURRED_FLAG (1 << 2)
#define DELETE_TCP_SERVER_TASK_FLAG (1 << 3)

#define ERRORS_CHECKER_INTERVAL_MS (10 * 1000)
#define STATUS_REQUESTS_SEND_INTERVAL_MS (60 * 1000)

#define MILLISECONDS_COUNTER_DIVIDER 10

#define MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT 15

#define SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS  64
#define CONNECTION_ERROR_CODE_RTC_ADDRESS       SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS + 1

typedef enum {
   ACCESS_POINT_CONNECTION_ERROR = 1,
   REQUEST_CONNECTION_ERROR,
   SOFTWARE_UPGRADE
} SYSTEM_RESTART_REASON_TYPE;

const char SEND_STATUS_INFO_TASK_NAME[] = "send_status_info_task";

const char RESPONSE_SERVER_SENT_OK[] = "\"statusCode\":\"OK\"";
const char STATUS_INFO_POST_REQUEST[] =
      "POST /server/esp8266/statusInfo HTTP/1.1\r\n"
      "Content-Length: <1>\r\n"
      "Host: <2>\r\n"
      "User-Agent: ESP8266\r\n"
      "Content-Type: application/json\r\n"
      "Connection: close\r\n"
      "Accept: application/json\r\n\r\n"
      "<3>\r\n";
const char STATUS_INFO_REQUEST_PAYLOAD_TEMPLATE[] =
      "{"
      "\"gain\":\"<1>\","
      "\"deviceName\":\"<2>\","
      "\"errors\":<3>,"
      "\"pendingConnectionErrors\":<4>,"
      "\"uptime\":<5>,"
      "\"buildTimestamp\":\"<6>\","
      "\"freeHeapSpace\":<7>,"
      "\"resetReason\":\"<8>\","
      "\"systemRestartReason\":\"<9>\""
      "}";
const char UPDATE_FIRMWARE[] = "\"updateFirmware\":true";

static void blink_both_leds();
static void stop_both_leds_blinking();
static void close_opened_sockets();

#endif

