All this shit with "ota_and_copy" target in Makefile is required only for 1MB FLASH to update a firmware remotely.
To load a firmware using UART "flash" command is used.

Current project is configured for 1MB FLASH with "ota_0" and "ota_1" app partitions.
Used RTOS SDK version is 3.1 (https://github.com/espressif/ESP8266_RTOS_SDK.git release/v3.1)

modified_SDK_3.1_files directory contains modification patches of RTOS SDK 3.1 to correctly work on Windows PC.
msys32\usr\bin\find.exe should be renamed to find_msys.exe, because on Windows there is its own "find" command.

Link_to_ESP8266_Shutters_workspace directory is working workspace for Eclipse. Repository directory do not contain log, history, cache and index files.