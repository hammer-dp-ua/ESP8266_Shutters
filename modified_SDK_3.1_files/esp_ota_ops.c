diff --git "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\esp2E32.tmp\\esp_ota_ops-913a06a-left.c" "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\components\\app_update\\esp_ota_ops.c"
index b9072340..dae6834e 100644
--- "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\esp2E32.tmp\\esp_ota_ops-913a06a-left.c"
+++ "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\components\\app_update\\esp_ota_ops.c"
@@ -77,15 +77,15 @@ static inline int esp_ota_verify_binary(const esp_partition_pos_t *pos, esp_imag
     ESP_LOGD(TAG, "OTA binary start entry 0x%x, partition start from 0x%x to 0x%x\n", entry, pos->offset,
                         pos->offset + pos->size);
 
-    if (pos->offset + pos->size <= 0x100000) {
-        if (entry <= 0 || entry <= pos->offset || entry >= pos->offset + pos->size) {
+    /*if (pos->offset + pos->size <= 0x100000) {
+        if (entry <= 0 || entry >= pos->offset + pos->size) {
             const char *doc_str = "<<ESP8266_RTOS_SDK/examples/system/ota/README.md>>";
 
             ESP_LOGE(TAG, "**Important**: The OTA binary link data is error, "
                           "please refer to document %s for how to generate OTA binaries", doc_str);
             return ESP_ERR_INVALID_ARG;
         }
-    }
+    }*/
 
     return ESP_OK;
 }
