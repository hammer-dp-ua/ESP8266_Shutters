#include "utils.h"

// FreeRTOS event group to signal when we are connected
// Max 24 bits when configUSE_16_BIT_TICKS is 0
static EventGroupHandle_t wifi_event_group;

/*
 * The event group allows multiple bits for each event,
 * but we only care about one event - are we connected
 * to the AP with an IP?
 */
static const int WIFI_CONNECTED_BIT = (1 << 0);

static void (*on_wifi_connected)();
static void (*on_wifi_disconnected)();
static void (*on_wifi_connection)();

static os_timer_t wi_fi_reconnection_timer_g;

/**
 * Do not forget to call free() function on returned pointer when it's no longer needed.
 *
 * *parameters - array of pointers to strings. The last parameter has to be NULL
 */
void *set_string_parameters(const char string[], const char *parameters[]) {
   bool open_brace_found = false;
   unsigned char parameters_amount = 0;
   unsigned short result_string_length = 0;

   for (; parameters[parameters_amount] != NULL; parameters_amount++) {
   }

   // Calculate the length without symbols to be replaced ('<x>')
   for (const char *string_pointer = string; *string_pointer != '\0'; string_pointer++) {
      if (*string_pointer == '<') {
         assert(!open_brace_found);

         open_brace_found = true;
         continue;
      }
      if (*string_pointer == '>') {
         assert(open_brace_found);

         open_brace_found = false;
         continue;
      }
      if (open_brace_found) {
         continue;
      }

      result_string_length++;
   }

   assert(!open_brace_found);

   for (unsigned char i = 0; parameters[i] != NULL; i++) {
      result_string_length += strnlen(parameters[i], 0xFFFF);
   }
   // 1 is for the last \0 character
   result_string_length++;

   char *allocated_result = MALLOC(result_string_length, 0xFFFF); // (string_length + 1) * sizeof(char)
   assert(allocated_result != NULL);

   for (unsigned short result_string_index = 0, input_string_index = 0;
         result_string_index < result_string_length - 1;
         result_string_index++) {
      char input_string_char = string[input_string_index];

      if (input_string_char == '<') {
         input_string_index++;
         input_string_char = string[input_string_index];
         assert(input_string_char >= '0' && input_string_char <= '9');

         unsigned short parameter_numeric_value = input_string_char - '0';
         assert(parameter_numeric_value <= parameters_amount);

         input_string_index++;
         input_string_char = string[input_string_index];

         if (input_string_char >= '0' && input_string_char <= '9') {
            parameter_numeric_value = parameter_numeric_value * 10 + input_string_char - '0';
            input_string_index++;
         }
         input_string_index++;

         // Parameters are starting with 1
         const char *parameter = parameters[parameter_numeric_value - 1];

         for (; *parameter != '\0'; parameter++, result_string_index++) {
            *(allocated_result + result_string_index) = *parameter;
         }
         result_string_index--;
      } else {
         *(allocated_result + result_string_index) = string[input_string_index];
         input_string_index++;
      }
   }
   *(allocated_result + result_string_length - 1) = '\0';
   return allocated_result;
}

bool compare_strings(char *string1, char *string2) {
   if (string1 == NULL || string2 == NULL) {
      return false;
   }

   bool result = true;
   unsigned short i = 0;

   while (result) {
      char string1_character = *(string1 + i);
      char string2_character = *(string2 + i);

      if (string1_character == '\0' && string2_character == '\0') {
         break;
      } else if (string1_character == '\0' || string2_character == '\0' || string1_character != string2_character) {
         result = false;
      }
      i++;
   }
   return result;
}

char *put_flash_string_into_heap(const char *flash_string, unsigned int allocated_time) {
   assert(flash_string != NULL);

   unsigned short string_length = strlen(flash_string);

   char *heap_string = MALLOC(string_length + 1, allocated_time);
   assert(heap_string != NULL);

   for (unsigned short i = 0; i < string_length; i++) {
      *(heap_string + i) = flash_string[i];
   }
   *(heap_string + string_length) = '\0';
   return heap_string;
}

static void connect_to_ap() {
   #ifdef ALLOW_USE_PRINTF
   printf("\nConnect to AP...\n");
   #endif

   esp_wifi_connect();
}

static esp_err_t esp_event_handler(void *ctx, system_event_t *event) {
   switch(event->event_id) {
      case SYSTEM_EVENT_STA_START:
         #ifdef ALLOW_USE_PRINTF
         printf("\nSYSTEM_EVENT_STA_START event\n");
         #endif

         connect_to_ap();
         on_wifi_connection();

         break;
      case SYSTEM_EVENT_STA_STOP:
         #ifdef ALLOW_USE_PRINTF
         printf("\nSYSTEM_EVENT_STA_STOP event\n");
         #endif

         break;
      case SYSTEM_EVENT_SCAN_DONE:
         #ifdef ALLOW_USE_PRINTF
         printf("\nScan status: %u, amount: %u, scan id: %u\n",
               event->event_info.scan_done.status, event->event_info.scan_done.number, event->event_info.scan_done.scan_id);
         #endif

         break;
      case SYSTEM_EVENT_STA_GOT_IP:
         #ifdef ALLOW_USE_PRINTF
         printf("\nGot IP: %s\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
         #endif

         os_timer_disarm(&wi_fi_reconnection_timer_g);
         xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
         on_wifi_connected();
         break;
      case SYSTEM_EVENT_STA_CONNECTED:
         #ifdef ALLOW_USE_PRINTF
         printf("\nStation: "MACSTR" join, AID=%d\n", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
         #endif

         break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
         #ifdef ALLOW_USE_PRINTF
         // See reason info in wifi_err_reason_t of esp_wifi_types.h
         printf("\nDisconnected from %s, reason: %u\n", event->event_info.disconnected.ssid, event->event_info.disconnected.reason);
         #endif

         on_wifi_disconnected();
         on_wifi_connection();
         xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

         os_timer_disarm(&wi_fi_reconnection_timer_g);
         os_timer_setfn(&wi_fi_reconnection_timer_g, (os_timer_func_t *) connect_to_ap, NULL);
         os_timer_arm(&wi_fi_reconnection_timer_g, WI_FI_RECONNECTION_INTERVAL_MS, true);

         break;
      default:
         break;
   }
   return ESP_OK;
}

void wifi_init_sta(void (*on_connected)(), void (*on_disconnected)(), void (*on_connection)()) {
   on_wifi_connected = on_connected;
   on_wifi_disconnected = on_disconnected;
   on_wifi_connection = on_connection;

   wifi_event_group = xEventGroupCreate();

   ESP_ERROR_CHECK(esp_event_loop_init(esp_event_handler, NULL));

   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));

   wifi_config_t wifi_config;
   memcpy(&wifi_config.sta.ssid, ACCESS_POINT_NAME, 32);
   memcpy(&wifi_config.sta.password, ACCESS_POINT_PASSWORD, 64);

   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
   ESP_ERROR_CHECK(esp_wifi_start());

   #ifdef ALLOW_USE_PRINTF
   printf("\nwifi_init_sta finished\n");
   #endif
}

void disable_wifi_event_handler() {
   esp_event_loop_set_cb(NULL, NULL);
}

bool is_connected_to_wifi() {
   return (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT;
}

/**
  * @brief     Read user data from the RTC memory.
  *
  *            The user data segment (512 bytes, as shown below) is used to store user data.
  *
  *             |<---- system data(256 bytes) ---->|<----------- user data(512 bytes) --------->|
  *
  * @attention Read and write unit for data stored in the RTC memory is 4 bytes.
  * @attention src_block is the block number (4 bytes per block).
  *            So when reading data at the beginning of the user data segment, src_block will be 256/4 = 64.
  *
  * @param     unsigned short src_block : source address block of RTC memory, src_block >= 64
  * @param     void *dst :                data pointer
  * @param     unsigned short length :    data length, unit: byte
  */
void rtc_mem_read(unsigned short src_block, void *dst, unsigned short length) {
   // validate reading a user block
   assert(src_block >= 64);
   assert(src_block);
   assert(dst != NULL);
   // validate length is multiple of 4
   assert(length > 0);
   assert(length % 4 == 0);

   // check valid length from specified starting point
   assert(length <= ((256 + 512) - (src_block * 4)));

   // copy the data
   for (short blocks = (length >> 2) - 1; blocks >= 0; blocks--) {
      volatile unsigned int *ram = ((unsigned int *) dst) + blocks;
      volatile unsigned int *rtc = ((unsigned int *) PERIPHS_RTC_BASEADDR) + src_block + blocks;
      *ram = *rtc;

      #ifdef ALLOW_USE_PRINTF
      printf("\nValue read from 0x%X RTC address is 0x%X\n", (unsigned int) rtc, *rtc);
      #endif
   }
}

/**
  * @brief     Write user data to the RTC memory.
  *
  *            During deep-sleep, only RTC is working. So users can store their data
  *            in RTC memory if it is needed. The user data segment below (512 bytes)
  *            is used to store the user data.
  *
  *            |<---- system data(256 bytes) ---->|<----------- user data(512 bytes) --------->|
  *
  * @attention Read and write unit for data stored in the RTC memory is 4 bytes.
  * @attention dst_block is the block number (4 bytes per block).
  *            So when storing data at the beginning of the user data segment, dst_block will be 256/4 = 64.
  *
  * @param     unsigned short dst_block : destination address of RTC memory, dst_block >= 64
  * @param     const void *src :          data pointer
  * @param     unsigned short length :    data length, unit: byte
  */
void rtc_mem_write(unsigned short dst_block, const void *src, unsigned short length) {
   // validate writing a user block
   assert(dst_block >= 64);
   assert(dst_block < (256 * 512 / 4));
   assert(src != NULL);
   // validate length is multiple of 4
   assert(length > 0);
   assert(length % 4 == 0);

   // check valid length from specified starting point
   assert(length <= ((256 + 512) - (dst_block * 4)));

   // copy the data
   for (short blocks = (length >> 2) - 1; blocks >= 0; blocks--) {
      volatile unsigned int *ram = ((unsigned int *) src) + blocks;
      volatile unsigned int *rtc = ((unsigned int *) PERIPHS_RTC_BASEADDR) + dst_block + blocks;
      *rtc = *ram;

      #ifdef ALLOW_USE_PRINTF
      printf("\nValue written to 0x%X RTC address is 0x%X\n", (unsigned int) rtc, *ram);
      #endif
   }
}

int connect_to_http_server() {
   if (!is_connected_to_wifi()) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nNot connected to Wi-Fi. To be deleted task\n");
      #endif

      return -1;
   }

   int socket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // SOCK_STREAM - TCP

   if (socket_id < 0) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nFailed to allocate socket\n");
      #endif

      return -1;
   }
   #ifdef ALLOW_USE_PRINTF
   printf("Socket %d has been allocated\n", socket_id);
   #endif

   struct sockaddr_in destination_address;
   destination_address.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);
   destination_address.sin_family = AF_INET;
   destination_address.sin_port = htons(SERVER_PORT);

   int connection_result = connect(socket_id, (struct sockaddr *) &destination_address, sizeof(destination_address));

   if (connection_result != 0) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nSocket connection failed. Error: %d\n", connection_result);
      #endif

      close(socket_id);
      return -1;
   }

   #ifdef ALLOW_USE_PRINTF
   printf("Socket %d has been connected\n", socket_id);
   #endif
   return socket_id;
}

char *send_request(char *request, unsigned short response_buffer_size, unsigned int *milliseconds_counter) {
   assert(request != NULL);
   assert(response_buffer_size > 0);

   int socket_id = connect_to_http_server();

   if (socket_id < 0) {
      return NULL;
   }

   unsigned short received_bytes_amount = 0;
   char *final_response_result = MALLOC(response_buffer_size, invocation_time);
   assert(final_response_result != NULL);

   for (;;) {
      int send_result = write(socket_id, request, strlen(request));

      if (send_result < 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nError occurred during sending. Error no.: %d, time: %u\n", send_result, *milliseconds_counter);
         #endif

         break;
      }
      #ifdef ALLOW_USE_PRINTF
      printf("Request has been sent. Socket: %d, time: %u\n", socket_id, *milliseconds_counter);
      #endif

      unsigned char tmp_buffer_size = response_buffer_size <= 255 ? response_buffer_size : 255;
      char tmp_buffer[tmp_buffer_size];
      int len = read(socket_id, tmp_buffer, tmp_buffer_size - 1);

      if (len < 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nReceive failed. Error no.: %d, time: %u\n", len, *milliseconds_counter);
         #endif

         FREE(final_response_result);
         final_response_result = NULL;

         break;
      } else if (len == 0) {
         final_response_result[received_bytes_amount] = 0;

         #ifdef ALLOW_USE_PRINTF
         printf("Final response: %s\nlength: %u, time: %u\n", final_response_result, received_bytes_amount, *milliseconds_counter);
         #endif

         break;
      } else {
         bool max_length_exceed = false;

         for (unsigned short i = 0; i < len; i++) {
            unsigned short addend = received_bytes_amount + i;

            if (addend >= response_buffer_size) {
               max_length_exceed = true;
               received_bytes_amount = response_buffer_size;
               break;
            }

            *(final_response_result + addend) = tmp_buffer[i];
         }
         tmp_buffer[len] = 0;
         received_bytes_amount += max_length_exceed ? 0 : len;

         #ifdef ALLOW_USE_PRINTF
         //printf("Received %d bytes, time: %u\n", len, *milliseconds_counter);
         //printf("Response: %s\n", tmp_buffer);
         #endif
      }
   }

   #ifdef ALLOW_USE_PRINTF
   printf("Shutting down socket and restarting...\n");
   #endif

   shutdown_and_close_socket(socket_id);
   return final_response_result;
}

int get_request_content_length(char *request) {
   char *content_length_header = "Content-Length: ";
   char *content_length_header_pointer = strstr(request, content_length_header);
   int value = -1;

   if (content_length_header_pointer != NULL) {
      char *value_first_digit = (content_length_header_pointer + strlen(content_length_header));

      for(int i = 0; *(value_first_digit + i) >= '0' && *(value_first_digit + i) <= '9'; i++) {
         if (value == -1) {
            value = 0;
         }

         value *= 10;
         value += *(value_first_digit + i) - '0';
      }
   }
   return value;
}

char *get_request_payload(char *already_read_request_content_part, char *request, unsigned int *milliseconds_counter) {
   if (request == NULL) {
      return already_read_request_content_part;
   }

   if (already_read_request_content_part == NULL) {
      // First part
      char *request_content = NULL;
      char *request_content_new_line = strstr(request, "\r\n\r\n");

      if (request_content_new_line != NULL) {
         request_content = request_content_new_line + 4;
      } else {
         request_content_new_line = strstr(request, "\n\n");

         if (request_content_new_line != NULL) {
            request_content = request_content_new_line + 2;
         }
      }

      if (request_content == NULL) {
         return NULL;
      }

      unsigned int request_content_length = strlen(request_content);
      char *allocated = MALLOC(request_content_length + 1, *milliseconds_counter);
      memcpy(allocated, request_content, request_content_length);
      allocated[request_content_length] = 0;
      return allocated;
   } else {
      unsigned int already_read_request_content_part_length = strlen(already_read_request_content_part);
      unsigned int current_request_part_content_length = strlen(request);
      char *allocated = MALLOC(already_read_request_content_part_length + current_request_part_content_length + 1, *milliseconds_counter);

      memcpy(allocated, already_read_request_content_part, already_read_request_content_part_length);
      memcpy(allocated + already_read_request_content_part_length, request, current_request_part_content_length);
      allocated[already_read_request_content_part_length + current_request_part_content_length] = 0;
      FREE(already_read_request_content_part);
      return allocated;
   }
}

char *get_gson_element_value(char *json_string, char *json_element_to_find, bool *is_numeric_param, unsigned int *milliseconds_counter) {
   if (json_string == NULL || json_element_to_find == NULL) {
      return NULL;
   }

   char *json_element_to_find_in_string = strstr(json_string, json_element_to_find);
   unsigned int json_element_to_find_length = (unsigned int) strlen(json_element_to_find);
   char *value = NULL;

   if (json_element_to_find_in_string != NULL) {
      value = json_element_to_find_in_string + json_element_to_find_length;
      value++; // For closing '"' character
   } else {
      return NULL;
   }

   if (*value != ':') {
      return NULL;
   }
   value++;
   if (*value == '\"') {
      value++;
   }

   unsigned int value_length = 0;
   bool is_numeric = true;

   while (*value != '\0' && *value != '\"' && *value != '}') {
      if ((*value < '0' || *value > '9') && !(value_length == 0 && *value == '-') && *value != '.') { // Exceptions for "-" sign and "."
         is_numeric = false;
      }

      value_length++;
      value++;
   }
   value -= value_length; // Return to the beginning

   char *returning_value = MALLOC(value_length + 1, *milliseconds_counter);

   memcpy(returning_value, value, value_length);
   returning_value[value_length] = 0;
   *is_numeric_param = is_numeric;
   return returning_value;
}

void shutdown_and_close_socket(int socket) {
   if (socket >= 0) {
      shutdown(socket, SHUT_RDWR);
      close(socket);
   }
}
