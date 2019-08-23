#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ESP8266_Shutters

include $(IDF_PATH)/make/project.mk

ota_and_copy: delete_some_build_files ota copy_for_upgrading

# Without removing the files newly created ${PROJECT_NAME}.app1.bin and ${PROJECT_NAME}.app2.bin have the same entry address (esp_image_header_t->entry_addr)
delete_some_build_files:
	-rm -rf ./build/esp8266/*
	-rm -f ./build/*

copy_for_upgrading:
	cp ./build/${PROJECT_NAME}.app1.bin Z:/ESP8266_upgrade/firmware.app1.bin
	cp ./build/${PROJECT_NAME}.app2.bin Z:/ESP8266_upgrade/firmware.app2.bin
