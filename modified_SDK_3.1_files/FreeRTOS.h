diff --git "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\Fre25E3.tmp\\FreeRTOS-913a06a-left.h" "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\components\\freertos\\include\\freertos\\FreeRTOS.h"
index 78d176a0..d713a7aa 100644
--- "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\Fre25E3.tmp\\FreeRTOS-913a06a-left.h"
+++ "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\components\\freertos\\include\\freertos\\FreeRTOS.h"
@@ -149,7 +149,7 @@ extern "C" {
 #endif
 
 #ifndef INCLUDE_xTaskGetHandle
-	#define INCLUDE_xTaskGetHandle 0
+	#define INCLUDE_xTaskGetHandle 1
 #endif
 
 #ifndef INCLUDE_uxTaskGetStackHighWaterMark
