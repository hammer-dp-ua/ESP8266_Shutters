#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ESP8266_Shutters

include $(IDF_PATH)/make/project.mk

ota_and_copy: ota copy_for_upgrading
copy_for_upgrading:
	cp ./build/${PROJECT_NAME}.app1.bin Z:/ESP8266_upgrade/firmware.app1.bin
	cp ./build/${PROJECT_NAME}.app2.bin Z:/ESP8266_upgrade/firmware.app2.bin
