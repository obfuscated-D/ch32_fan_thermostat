
## Description
This was a quick and dirty project to create a temperature controlled fan switch, with an lcd display and encoder knob.
This project uses cnlohr's [ch32v003fun](https://github.com/cnlohr/ch32v003fun) project as a base,
and the [lib_i2c](https://github.com/ADBeta/CH32V003_lib_i2c.) library.

I wrote a library for the i2c backpack hd44780 lcd, based off the Arduino [LCD_I2C](https://github.com/blackhack/LCD_I2C) library,
you can find the lcd library here in lib/lcd_i2c.h and lib/lcd_i2c.c
This is just a personal project but if you find any of the code useful, you're free to use it.
I won't be providing any support for this code, but feel free to ask questions.

## Setup
See the [Installattion guide](https://github.com/cnlohr/ch32v003fun/wiki/Installation) for the ch32v003fun project, you will need the toolchain to flash the code to the ch32v003 board.


## Flashing
To flash, you will need a wchlink programmer, or if you see the [ch32v003fun](https://github.com/cnlohr/ch32v003fun/tree/master?tab=readme-ov-file) readme, they have instructions to flash using and esp32s2, stm32, or an arduino.

WIRING:

I2C LCD:

    SDA -> PC1
    SCL -> PC2

ENCODER:

    CH1 -> PD4
    CH2 -> PD3
    BUTTON -> PC3
    
TEMP SENSORs:

    CLK -> PC5
    MOSI -> PC6
    MISO -> PC7
    CS sensor1 -> PD0
    
