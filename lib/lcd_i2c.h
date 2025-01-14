#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>
#include <stddef.h>
#include "lib_i2c.h"
#include "ch32v003fun.h"
#include <stdbool.h>
/**
 * @brief Initializes the LCD over I2C.
 * @param address I2C address of the LCD.
 * @param clk_rate I2C clock rate.
 */
void LCD_Init(uint8_t address, uint32_t clk_rate);

/**
 * @brief Writes a command to the LCD.
 * @param address I2C address of the LCD.
 * @param command Command byte to send.
 */
void LCD_WriteCommand(uint8_t address, uint8_t command);

/**
 * @brief Writes a data byte to the LCD.
 * @param address I2C address of the LCD.
 * @param data Data byte to send.
 */
void LCD_WriteData(uint8_t address, uint8_t data);

void LCD_WriteChar(uint8_t address, char c);

/**
 * @brief Writes a string to the LCD.
 * @param address I2C address of the LCD.
 * @param str Null-terminated string to write.
 */

void LCD_WriteString(uint8_t address, const char* str);

/**
 * @brief Sets the cursor position on the LCD.
 * @param address I2C address of the LCD.
 * @param col Column number (0-based).
 * @param row Row number (0-based).
 */
void LCD_SetCursor(uint8_t address, uint8_t col, uint8_t row);

/**
 * @brief Clears the LCD and resets the cursor position.
 * @param address I2C address of the LCD.
 */
void LCD_Clear(uint8_t address);

/**
 * @brief Clears a single line on the LCD.
 * @param address I2C address of the LCD.
 * @param row Row number (0-based).
 */
void LCD_ClearLine(uint8_t address, uint8_t row);

/**
 * @brief Prints a string centered on the LCD.
 * @param address I2C address of the LCD.
 * @param row Row number (0-based).
 * @param str Null-terminated string to print.
 */
void LCD_PrintCentered(uint8_t address, uint8_t row, const char* str);
/**
 * @brief Scrolls text across the LCD display
 * @param address I2C address of the LCD
 * @param text Text to scroll
 * @param row Row to display the text (0-based)
 * @param delay_ms Delay between each scroll step in milliseconds
 * @param marquee If true, text will loop continuously. If false, scrolls once.
 */
void LCD_ScrollText(uint8_t address, const char* text, uint8_t row, uint16_t delay_ms, bool marquee);
/**
 * @brief Enables or disables the LCD backlight.
 * @param address I2C address of the LCD.
 * @param state 1 to enable backlight, 0 to disable.
 */
void LCD_SetBacklight(uint8_t address, uint8_t state);

#endif 
