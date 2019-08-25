#ifndef GENERAL_UTILS
#define GENERAL_UTILS

#include "device_settings.h"
#include "global_definitions.h"
#include "malloc_logger.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "string.h"
#include "lwip/ip4_addr.h"
#include "lwip/sys.h"
#include "lwip/inet.h"
#include "sys/socket.h"

#define HEXADECIMAL_ADDRESS_FORMAT "%08x"
#define WI_FI_RECONNECTION_INTERVAL_MS (10 * 1000)

#define RTC_MEM_BASE 0x60001000

void *set_string_parameters(const char string[], const char *parameters[]);
char *generate_post_request(char *request);
char *generate_reset_reason();
void wifi_init_sta(void (*on_connected)(), void (*on_disconnected)(), void (*on_connection)());
bool is_connected_to_wifi();
void rtc_mem_read(unsigned int addr, void *dst, unsigned int length);
void rtc_mem_write(unsigned int dst, const void *src, unsigned int length);
int connect_to_http_server();
char *send_request(char *request, unsigned short response_buffer_length, unsigned int *milliseconds_counter);
int get_request_content_length(char *request);
char *get_request_payload(char *already_read_request_content_part, char *request, unsigned int *milliseconds_counter);
void shutdown_and_close_socket(int socket);
char *get_gson_element_value(char *json_string, char *json_element_to_find, bool *is_numeric_param, unsigned int *milliseconds_counter);
void disable_wifi_event_handler();
char *get_value_of_get_request_parameter(char *request, char *parameter, bool *is_numeric_param_value, unsigned int *milliseconds_counter);
#endif
