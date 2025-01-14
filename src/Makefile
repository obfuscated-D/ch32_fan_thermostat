all : flash

TARGET:=main
ADDITIONAL_C_FILES = ../lib/lib_i2c.c ../lib/lcd_i2c.c
ADDITIONAL_HEADERS = max6675.h


include ../ch32v003fun/ch32v003fun.mk

flash : cv_flash
clean : cv_clean

