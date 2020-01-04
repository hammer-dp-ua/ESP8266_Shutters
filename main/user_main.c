/**
 * Pins 4 and 5 on some ESP8266-07 are exchanged on silk screen!!!
 *
 * Required connections:
 * GPIO15 to GND
 * GPIO to "+" OR to "GND" for flashing
 * EN to "+"
 */

#include "user_main.h"

static unsigned int milliseconds_counter_g;
static int signal_strength_g;
static unsigned short errors_counter_g = 0;
static unsigned short repetitive_request_errors_counter_g = 0;
static unsigned char pending_connection_errors_counter_g;
static unsigned int repetitive_ap_connecting_errors_counter_g = 0;
static unsigned int repetitive_tcp_server_errors_counter_g = 0;

static int opened_sockets_g[2];

static os_timer_t millisecons_time_serv_g;
static os_timer_t errors_checker_timer_g;
static os_timer_t blink_both_leds_g;
static os_timer_t status_sender_timer_g;
static os_timer_t shutters_activity_g;
static os_timer_t blink_on_shutters_activity_g;

static EventGroupHandle_t general_event_group_g;

static SemaphoreHandle_t wirelessNetworkActionsSemaphore_g;

static TaskHandle_t tcp_server_task_g;

static void milliseconds_counter() {
   milliseconds_counter_g++;
}

static void start_100millisecons_counter() {
   os_timer_disarm(&millisecons_time_serv_g);
   os_timer_setfn(&millisecons_time_serv_g, (os_timer_func_t *) milliseconds_counter, NULL);
   os_timer_arm(&millisecons_time_serv_g, 1000 / MILLISECONDS_COUNTER_DIVIDER, true); // 1000/10 = 100 ms
}

static void scan_access_point_task(void *pvParameters) {
   long rescan_when_connected_task_delay = 10 * 60 * 1000 / portTICK_RATE_MS; // 10 mins
   long rescan_when_not_connected_task_delay = 10 * 1000 / portTICK_RATE_MS; // 10 secs
   wifi_scan_config_t scan_config;
   unsigned short scanned_access_points_amount = 1;
   wifi_ap_record_t scanned_access_points[1];

   scan_config.ssid = (unsigned char *) ACCESS_POINT_NAME;
   scan_config.bssid = 0;
   scan_config.channel = 0;
   scan_config.show_hidden = false;

   for (;;) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nStart of Wi-Fi scanning... %u\n", milliseconds_counter_g);
      #endif

      xSemaphoreTake(wirelessNetworkActionsSemaphore_g, portMAX_DELAY);

      if (is_connected_to_wifi() && ((xEventGroupGetBits(general_event_group_g) & UPDATE_FIRMWARE_FLAG) == 0) &&
            esp_wifi_scan_start(&scan_config, true) == ESP_OK &&
            esp_wifi_scan_get_ap_records(&scanned_access_points_amount, scanned_access_points) == ESP_OK) {
         signal_strength_g = scanned_access_points[0].rssi;

         #ifdef ALLOW_USE_PRINTF
         printf("Scanned %u access points", scanned_access_points_amount);
         for (unsigned char i = 0; i < scanned_access_points_amount; i++) {
            printf("\nScan index: %u, ssid: %s, rssi: %d", i, scanned_access_points[i].ssid, scanned_access_points[i].rssi);
         }
         printf("\n");
         #endif

         xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
         vTaskDelay(rescan_when_connected_task_delay);
      } else {
         #ifdef ALLOW_USE_PRINTF
         printf("Wi-Fi scanning skipped. %u\n", milliseconds_counter_g);
         #endif

         xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
         vTaskDelay(rescan_when_not_connected_task_delay);
      }
   }
}

static void blink_both_leds() {
   if (gpio_get_level(AP_CONNECTION_STATUS_LED_PIN)) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   } else {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   }
}

static void start_both_leds_blinking() {
   os_timer_disarm(&blink_both_leds_g);
   os_timer_setfn(&blink_both_leds_g, (os_timer_func_t *) blink_both_leds, NULL);
   os_timer_arm(&blink_both_leds_g, 2000 / MILLISECONDS_COUNTER_DIVIDER, true); // 200 ms
}

static void stop_both_leds_blinking() {
   os_timer_disarm(&blink_both_leds_g);

   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
}

static void blink_on_send(gpio_num_t pin) {
   int initial_pin_state = gpio_get_level(pin);
   unsigned char i;

   for (i = 0; i < 4; i++) {
      bool set_pin = initial_pin_state == 1 ? i % 2 == 1 : i % 2 == 0;

      if (set_pin) {
         gpio_set_level(pin, 1);
      } else {
         gpio_set_level(pin, 0);
      }
      vTaskDelay(100 / portTICK_RATE_MS);
   }

   if (pin == AP_CONNECTION_STATUS_LED_PIN) {
      if (is_connected_to_wifi()) {
         gpio_set_level(pin, 1);
      } else {
         gpio_set_level(pin, 0);
      }
   }
}

static void on_response_error() {
   repetitive_request_errors_counter_g++;
   errors_counter_g++;
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   xEventGroupSetBits(general_event_group_g, REQUEST_ERROR_OCCURRED_FLAG);
}

void send_status_info_task(void *pvParameters) {
   xSemaphoreTake(wirelessNetworkActionsSemaphore_g, portMAX_DELAY);
   blink_on_send(SERVER_AVAILABILITY_STATUS_LED_PIN);

   char signal_strength[5];
   snprintf(signal_strength, 5, "%d", signal_strength_g);
   char errors_counter[6];
   snprintf(errors_counter, 6, "%u", errors_counter_g);
   char pending_connection_errors_counter[4];
   snprintf(pending_connection_errors_counter, 4, "%u", pending_connection_errors_counter_g);
   char uptime[11];
   snprintf(uptime, 11, "%u", milliseconds_counter_g / MILLISECONDS_COUNTER_DIVIDER);
   char *build_timestamp = "";
   char free_heap_space[7];
   snprintf(free_heap_space, 7, "%u", esp_get_free_heap_size());
   char *reset_reason = "";
   char *system_restart_reason = "";

   if ((xEventGroupGetBits(general_event_group_g) & FIRST_STATUS_INFO_SENT_FLAG) == 0) {
      char build_timestamp_filled[30];
      snprintf(build_timestamp_filled, 30, "%s", __TIMESTAMP__);
      build_timestamp = build_timestamp_filled;

      esp_reset_reason_t rst_info = esp_reset_reason();

      switch(rst_info) {
         case ESP_RST_UNKNOWN:
            reset_reason = "Unknown";
            break;
         case ESP_RST_POWERON:
            reset_reason = "Power on";
            break;
         case ESP_RST_EXT:
            reset_reason = "Reset by external pin";
            break;
         case ESP_RST_SW:
            reset_reason = "Software";
            break;
         case ESP_RST_PANIC:
            reset_reason = "Exception/panic";
            break;
         case ESP_RST_INT_WDT:
            reset_reason = "Watchdog";
            break;
         case ESP_RST_TASK_WDT:
            reset_reason = "Task watchdog";
            break;
         case ESP_RST_WDT:
            reset_reason = "Other watchdog";
            break;
         case ESP_RST_DEEPSLEEP:
            reset_reason = "Deep sleep";
            break;
         case ESP_RST_BROWNOUT:
            reset_reason = "Brownout";
            break;
         case ESP_RST_SDIO:
            reset_reason = "SDIO";
            break;
      }

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type;

      rtc_mem_read(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);

      if (system_restart_reason_type == ACCESS_POINT_CONNECTION_ERROR) {
         int connection_error_code = 1;
         char system_restart_reason_inner[31];

         rtc_mem_read(CONNECTION_ERROR_CODE_RTC_ADDRESS, &connection_error_code, 4);

         snprintf(system_restart_reason_inner, 31, "AP connections error. Code: %d", connection_error_code);
         system_restart_reason = system_restart_reason_inner;
      } else if (system_restart_reason_type == REQUEST_CONNECTION_ERROR) {
         int connection_error_code = 1;
         char system_restart_reason_inner[25];

         rtc_mem_read(CONNECTION_ERROR_CODE_RTC_ADDRESS, &connection_error_code, 4);

         snprintf(system_restart_reason_inner, 25, "Requests error. Code: %d", connection_error_code);
         system_restart_reason = system_restart_reason_inner;
      } else if (system_restart_reason_type == SOFTWARE_UPGRADE) {
         system_restart_reason = "Software upgrade";
      }
   }

   const char *request_payload_template_parameters[] =
         {signal_strength, DEVICE_NAME, errors_counter, pending_connection_errors_counter, uptime, build_timestamp, free_heap_space,
          reset_reason, system_restart_reason, NULL};
   char *request_payload = set_string_parameters(STATUS_INFO_REQUEST_PAYLOAD_TEMPLATE, request_payload_template_parameters);

   #ifdef ALLOW_USE_PRINTF
   //printf("\nRequest payload: %s\n", request_payload);
   #endif

   unsigned short request_payload_length = strnlen(request_payload, 0xFFFF);
   char request_payload_length_string[6];
   snprintf(request_payload_length_string, 6, "%u", request_payload_length);
   const char *request_template_parameters[] = {request_payload_length_string, SERVER_IP_ADDRESS, request_payload, NULL};
   char *request = set_string_parameters(STATUS_INFO_POST_REQUEST, request_template_parameters);
   FREE(request_payload);

   #ifdef ALLOW_USE_PRINTF
   printf("\nCreated request: %s\nTime: %u\n", request, milliseconds_counter_g);
   #endif

   char *response = send_request(request, 255, &milliseconds_counter_g);

   FREE(request);

   if (response == NULL) {
      on_response_error();
   } else {
      if (strstr(response, RESPONSE_SERVER_SENT_OK)) {
         repetitive_request_errors_counter_g = 0;
         gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);

         if ((xEventGroupGetBits(general_event_group_g) & FIRST_STATUS_INFO_SENT_FLAG) == 0) {
            xEventGroupSetBits(general_event_group_g, FIRST_STATUS_INFO_SENT_FLAG);

            unsigned int overwrite_value = 0xFFFF;
            rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &overwrite_value, 4);
         }

         #ifdef ALLOW_USE_PRINTF
         printf("Response OK, time: %u\n", milliseconds_counter_g);
         #endif

         if (strstr(response, UPDATE_FIRMWARE)) {
            xEventGroupSetBits(general_event_group_g, UPDATE_FIRMWARE_FLAG);
            start_both_leds_blinking();

            SYSTEM_RESTART_REASON_TYPE reason = SOFTWARE_UPGRADE;
            rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &reason, 4);

            // TCP server task will be deleted
            close_opened_sockets();

            update_firmware();
         }
      } else {
         on_response_error();
      }

      FREE(response);
   }

   xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
   vTaskDelete(NULL);
}

static void send_status_info() {
   if (is_connected_to_wifi() == false || xTaskGetHandle(SEND_STATUS_INFO_TASK_NAME) != NULL ||
         (xEventGroupGetBits(general_event_group_g) & UPDATE_FIRMWARE_FLAG)) {
      return;
   }

   xTaskCreate(send_status_info_task, SEND_STATUS_INFO_TASK_NAME, configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);
}

static void schedule_sending_status_info(unsigned int timeout_ms) {
   os_timer_disarm(&status_sender_timer_g);
   os_timer_setfn(&status_sender_timer_g, (os_timer_func_t *) send_status_info, NULL);
   os_timer_arm(&status_sender_timer_g, timeout_ms, true);
}

static void blink_on_shutters_opening() {
   if (gpio_get_level(AP_CONNECTION_STATUS_LED_PIN)) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   } else {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
   }
}

static void start_blinking_on_shutters_opening() {
   os_timer_disarm(&blink_on_shutters_activity_g);
   os_timer_setfn(&blink_on_shutters_activity_g, (os_timer_func_t *) blink_on_shutters_opening, NULL);
   os_timer_arm(&blink_on_shutters_activity_g, 200, true);
}

static void blink_on_shutters_closing() {
   if (gpio_get_level(SERVER_AVAILABILITY_STATUS_LED_PIN)) {
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   } else {
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   }
}

static void start_blinking_on_shutters_closing() {
   os_timer_disarm(&blink_on_shutters_activity_g);
   os_timer_setfn(&blink_on_shutters_activity_g, (os_timer_func_t *) blink_on_shutters_closing, NULL);
   os_timer_arm(&blink_on_shutters_activity_g, 200, true);
}

static void stop_blinking_on_shutters_activity() {
   os_timer_disarm(&blink_on_shutters_activity_g);
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   if (is_connected_to_wifi()) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   }
}

static void stop_shutters_activity() {
   #ifdef ROOM_SHUTTER
   gpio_set_level(RELAY_DOWN_PIN, 0);
   gpio_set_level(RELAY_UP_PIN, 0);
   #endif
   #ifdef KITCHEN_SHUTTER
   gpio_set_level(RELAY1_DOWN_PIN, 0);
   gpio_set_level(RELAY1_UP_PIN, 0);
   gpio_set_level(RELAY2_DOWN_PIN, 0);
   gpio_set_level(RELAY2_UP_PIN, 0);
   #endif
   stop_blinking_on_shutters_activity();
}

static void open_shutters(unsigned char opening_time_sec, unsigned char shutter_no) {
   os_timer_disarm(&shutters_activity_g);
   stop_shutters_activity();
   os_timer_setfn(&shutters_activity_g, (os_timer_func_t *) stop_shutters_activity, NULL);
   os_timer_arm(&shutters_activity_g, ((unsigned int) opening_time_sec) * 1000, false);

   #ifdef ROOM_SHUTTER
   gpio_set_level(RELAY_UP_PIN, 1);
   #endif
   #ifdef KITCHEN_SHUTTER
   if (shutter_no == 1) {
      gpio_set_level(RELAY1_UP_PIN, 1);
   } else if (shutter_no == 2) {
      gpio_set_level(RELAY2_UP_PIN, 1);
   }
   #endif

   start_blinking_on_shutters_opening();
}

static void close_shutters(unsigned char closing_time_sec, unsigned char shutter_no) {
   os_timer_disarm(&shutters_activity_g);
   stop_shutters_activity();
   os_timer_setfn(&shutters_activity_g, (os_timer_func_t *) stop_shutters_activity, NULL);
   os_timer_arm(&shutters_activity_g, ((unsigned int) closing_time_sec) * 1000, false);

   #ifdef ROOM_SHUTTER
   gpio_set_level(RELAY_DOWN_PIN, 1);
   #endif
   #ifdef KITCHEN_SHUTTER
   if (shutter_no == 1) {
      gpio_set_level(RELAY1_DOWN_PIN, 1);
   } else if (shutter_no == 2) {
      gpio_set_level(RELAY2_DOWN_PIN, 1);
   }
   #endif

   start_blinking_on_shutters_closing();
}

static void process_request_and_send_response(char *request, int socket) {
   #ifdef ALLOW_USE_PRINTF
   printf("All data received\n");
   #endif

   char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
   int sent = write(socket, response, strlen(response));

   if (sent < 0) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nError occurred during sending\n");
      #endif

      repetitive_tcp_server_errors_counter_g++;
   } else {
      #ifdef ALLOW_USE_PRINTF
      printf("Sent %d bytes\n", sent);
      #endif
   }

   bool is_duration_value_numeric = false;
   char *opening_activity_duration = get_value_of_get_request_parameter(request, "open", &is_duration_value_numeric, &milliseconds_counter_g);
   bool is_shutter_no_numeric = false;
   char *shutter_no = get_value_of_get_request_parameter(request, "shutter_no", &is_shutter_no_numeric, &milliseconds_counter_g);
   unsigned char shutter_no_numeric = 0;

   if (shutter_no != NULL) {
      if (is_shutter_no_numeric) {
         shutter_no_numeric = (unsigned char) atoi(shutter_no);
      }

      FREE(shutter_no);
   }

   if (opening_activity_duration != NULL) {
      if (is_duration_value_numeric) {
         int opening_time = atoi(opening_activity_duration);

         if (opening_time > 0) {
            open_shutters((unsigned char) opening_time, shutter_no_numeric);
         }
      }

      FREE(opening_activity_duration);
   }

   char *closing_activity_duration = get_value_of_get_request_parameter(request, "close", &is_duration_value_numeric, &milliseconds_counter_g);

   if (closing_activity_duration != NULL) {
      if (is_duration_value_numeric) {
         int closing_time = atoi(closing_activity_duration);

         if (closing_time > 0) {
            close_shutters((unsigned char) closing_time, shutter_no_numeric);
         }
      }

      FREE(closing_activity_duration);
   }
}

static void tcp_server_task(void *pvParameters) {
   while (true) {
      if ((xEventGroupGetBits(general_event_group_g) & UPDATE_FIRMWARE_FLAG) ||
            (xEventGroupGetBits(general_event_group_g) & DELETE_TCP_SERVER_TASK_FLAG)) {
         // If some request is receipted during update
         #ifdef ALLOW_USE_PRINTF
         printf("tcp_server_task is to be removed\n");
         #endif

         xEventGroupClearBits(general_event_group_g, DELETE_TCP_SERVER_TASK_FLAG);
         vTaskDelete(NULL);
      }

      if (!is_connected_to_wifi()) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nNot connected to AP for TCP server, time: %u\n", milliseconds_counter_g);
         #endif

         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

      if (listen_socket == -1) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nError on socket opening, time: %u\n", milliseconds_counter_g);
         #endif

         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      opened_sockets_g[0] = listen_socket;

      #ifdef ALLOW_USE_PRINTF
      printf("\nSocket %d created, time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      struct sockaddr_in sock_addr;
      sock_addr.sin_addr.s_addr = INADDR_ANY;
      sock_addr.sin_family = AF_INET;
      sock_addr.sin_port = htons(LOCAL_SERVER_PORT);

      int ret = bind(listen_socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr));

      if (ret != 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nSocked %d binding error, time: %u\n", listen_socket, milliseconds_counter_g);
         #endif

         shutdown_and_close_socket(listen_socket);
         opened_sockets_g[0] = -1;
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Socket %d binding OK. Listening..., time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      ret = listen(listen_socket, 5);

      if (ret != 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nSocket %d listening error, time: %u\n", listen_socket, milliseconds_counter_g);
         #endif

         shutdown_and_close_socket(listen_socket);
         opened_sockets_g[0] = -1;
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Socket %d listening OK, time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      struct sockaddr_in client_addr;
      unsigned int addr_len = sizeof(client_addr);
      // Blocks here until a request
      int accept_socket = accept(listen_socket, (struct sockaddr *) &client_addr, &addr_len);

      if ((xEventGroupGetBits(general_event_group_g) & UPDATE_FIRMWARE_FLAG) ||
            (xEventGroupGetBits(general_event_group_g) & DELETE_TCP_SERVER_TASK_FLAG)) {
         continue;
      }

      if (accept_socket < 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nUnable to accept connection: errno %d\n", errno);
         #endif

         shutdown_and_close_socket(listen_socket);
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }
      opened_sockets_g[1] = accept_socket;

      #ifdef ALLOW_USE_PRINTF
      printf("\nSocket %d accepted\n", accept_socket);
      #endif

      // Buffer size should always be enough, because for GET requests only the beginning of requests is required
      unsigned short rx_buffer_size = 300;
      char rx_buffer[rx_buffer_size];

      while (true) {
         #ifdef ALLOW_USE_PRINTF
         printf("Receiving..., time: %u\n", milliseconds_counter_g);
         #endif

         int received_bytes = read(accept_socket, rx_buffer, rx_buffer_size - 1);

         if (received_bytes < 0) {
            #ifdef ALLOW_USE_PRINTF
            printf("\nReceive failed. Error no.: %d, time: %u\n", received_bytes, milliseconds_counter_g);
            #endif

            repetitive_tcp_server_errors_counter_g++;
            break;
         } else if (received_bytes == 0) {
            break;
         } else {
            rx_buffer[received_bytes] = 0; // Null-terminate whatever we received and treat like a string

            #ifdef ALLOW_USE_PRINTF
            char addr_str[20];
            inet_ntoa_r(((struct sockaddr_in *) &client_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
            printf("Received %d bytes from %s\n", received_bytes, addr_str);
            printf("Content: %s\n", rx_buffer);
            #endif

            process_request_and_send_response(rx_buffer, accept_socket);
         }
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Shutting down sockets %d and %d, restarting...\n", accept_socket, listen_socket);
      #endif

      close_opened_sockets();
      repetitive_tcp_server_errors_counter_g = 0;
   }
}

static void pins_config() {
   gpio_config_t output_pins;
   output_pins.mode = GPIO_MODE_OUTPUT;

   #ifdef ROOM_SHUTTER
   output_pins.pin_bit_mask = (1<<AP_CONNECTION_STATUS_LED_PIN) | (1<<SERVER_AVAILABILITY_STATUS_LED_PIN)
         | (1<<RELAY_DOWN_PIN) | (1<<RELAY_UP_PIN);
   #endif
   #ifdef KITCHEN_SHUTTER
   output_pins.pin_bit_mask = (1<<AP_CONNECTION_STATUS_LED_PIN) | (1<<SERVER_AVAILABILITY_STATUS_LED_PIN)
            | (1<<RELAY1_DOWN_PIN) | (1<<RELAY1_UP_PIN) | (1<<RELAY2_DOWN_PIN) | (1<<RELAY2_UP_PIN);
   #endif

   output_pins.pull_up_en = GPIO_PULLUP_DISABLE;
   output_pins.pull_down_en = GPIO_PULLDOWN_DISABLE;

   gpio_config(&output_pins);

   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   #ifdef ROOM_SHUTTER
   gpio_set_level(RELAY_DOWN_PIN, 0);
   gpio_set_level(RELAY_UP_PIN, 0);
   #endif
   #ifdef KITCHEN_SHUTTER
   gpio_set_level(RELAY1_DOWN_PIN, 0);
   gpio_set_level(RELAY1_UP_PIN, 0);
   gpio_set_level(RELAY2_DOWN_PIN, 0);
   gpio_set_level(RELAY2_UP_PIN, 0);
   #endif
}

static void close_opened_sockets() {
   shutdown_and_close_socket(opened_sockets_g[0]);
   opened_sockets_g[0] = -1;
   shutdown_and_close_socket(opened_sockets_g[1]);
   opened_sockets_g[1] = -1;
}

void on_wifi_connected_task() {
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
   repetitive_ap_connecting_errors_counter_g = 0;

   xTaskCreate(tcp_server_task, "tcp_server_task", configMINIMAL_STACK_SIZE * 3, NULL, 1, &tcp_server_task_g);
   send_status_info();

   vTaskDelete(NULL);
}

void on_wifi_connected() {
   xTaskCreate(on_wifi_connected_task, "on_wifi_connected_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

void on_wifi_disconnected_task() {
   repetitive_ap_connecting_errors_counter_g++;
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   //xEventGroupSetBits(general_event_group_g, DELETE_TCP_SERVER_TASK_FLAG);

   vTaskDelete(tcp_server_task_g);
   close_opened_sockets();
   vTaskDelete(NULL);
}

void on_wifi_disconnected() {
   xTaskCreate(on_wifi_disconnected_task, "on_wifi_disconnected_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

static void blink_on_wifi_connection_task(void *pvParameters) {
   blink_on_send(AP_CONNECTION_STATUS_LED_PIN);
   vTaskDelete(NULL);
}

void blink_on_wifi_connection() {
   xTaskCreate(blink_on_wifi_connection_task, "blink_on_wifi_connection_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

static void uart_config() {
   uart_config_t uart_config;
   uart_config.baud_rate = 115200;
   uart_config.data_bits = UART_DATA_8_BITS;
   uart_config.parity    = UART_PARITY_DISABLE;
   uart_config.stop_bits = UART_STOP_BITS_1;
   uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
   uart_param_config(UART_NUM_0, &uart_config);
}

/**
 * Created as a workaround to handle unknown issues.
 */
void check_errors_amount() {
   bool restart = false;

   if (repetitive_request_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nRequest errors amount: %u\n", repetitive_request_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = REQUEST_CONNECTION_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   } else if (repetitive_ap_connecting_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nAP connection errors amount: %u\n", repetitive_ap_connecting_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = ACCESS_POINT_CONNECTION_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   } else if (repetitive_tcp_server_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT + 10) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nTCP server errors amount: %u\n", repetitive_ap_connecting_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = TCP_SERVER_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   }

   if (restart) {
      esp_restart();
   }
}

static void blink_gpio_task(void *pvParameters) {
   while (true) {
      if (gpio_get_level(GPIO_NUM_5)) {
         gpio_set_level(GPIO_NUM_5, 0);
      } else {
         gpio_set_level(GPIO_NUM_5, 1);
      }
      vTaskDelay(200 / portTICK_RATE_MS);
   }
}

void app_main(void) {
   general_event_group_g = xEventGroupCreate();

   pins_config();
   uart_config();

   start_both_leds_blinking();
   vTaskDelay(3000 / portTICK_RATE_MS);
   stop_both_leds_blinking();

   #ifdef ALLOW_USE_PRINTF
   const esp_partition_t *running = esp_ota_get_running_partition();
   printf("\nRunning partition type: label: %s, %d, subtype: %d, offset: 0x%X, size: 0x%X, build timestamp: %s\n",
         running->label, running->type, running->subtype, running->address, running->size, __TIMESTAMP__);
   #endif

   tcpip_adapter_init();
   tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA); // Stop DHCP client
   tcpip_adapter_ip_info_t ip_info;
   ip_info.ip.addr = inet_addr(OWN_IP_ADDRESS);
   ip_info.gw.addr = inet_addr(OWN_GETAWAY_ADDRESS);
   ip_info.netmask.addr = inet_addr(OWN_NETMASK);
   tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

   wirelessNetworkActionsSemaphore_g = xSemaphoreCreateBinary();
   xSemaphoreGive(wirelessNetworkActionsSemaphore_g);

   wifi_init_sta(on_wifi_connected, on_wifi_disconnected, blink_on_wifi_connection);

   xTaskCreate(scan_access_point_task, "scan_access_point_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

   os_timer_setfn(&errors_checker_timer_g, (os_timer_func_t *) check_errors_amount, NULL);
   os_timer_arm(&errors_checker_timer_g, ERRORS_CHECKER_INTERVAL_MS, true);

   schedule_sending_status_info(STATUS_REQUESTS_SEND_INTERVAL_MS);

   start_100millisecons_counter();
}
