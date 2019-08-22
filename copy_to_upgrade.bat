del Z:\ESP8266_upgrade\firmware.bin
del Z:\ESP8266_upgrade\firmware.app1.bin
del Z:\ESP8266_upgrade\firmware.app2.bin
copy %~dp0\build\ESP8266_Shutters.app1.bin Z:\ESP8266_upgrade\firmware.app1.bin
copy %~dp0\build\ESP8266_Shutters.app2.bin Z:\ESP8266_upgrade\firmware.app2.bin

pause