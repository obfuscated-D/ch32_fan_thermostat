/******************************************************************************
 * CH32V003 LCD I2C Library
 *
 * Lightweight library to drive HD44780 LCD displays over I2C using the CH32V003
 * microcontroller. This implementation is written in pure C and uses the
 * provided CH32V003 I2C library for communication.
 ******************************************************************************/

#include "lib_i2c.h"
#include <stdint.h>
#include <stddef.h>
#include "lcd_i2c.h"
#include "lcd_constants.h"
#include <stdbool.h>

#define DELAY_US(us) Delay_Us(us)  
#define DELAY_MS(ms) Delay_Ms(ms) 

// I2C Timeout count
#define TIMEOUT_MAX 100000

/*** Private Functions *******************************************************/
static void LCD_Send(uint8_t address, uint8_t data, uint8_t mode);
static uint8_t check_event(uint32_t event_mask);
static uint8_t i2c_error_handler(const char *error_message);

/*** Public Functions ********************************************************/

/**
 * @brief Initializes the LCD over I2C.
 * @param address I2C address of the LCD.
 * @param clk_rate I2C clock rate.
 */

void LCD_Init(uint8_t address, uint32_t clk_rate) {
    // Initialize I2C
    if (i2c_init(clk_rate) != I2C_OK) {
        i2c_error_handler("Failed to initialize I2C");
        return;
    }

    DELAY_MS(50); // Wait for power-up

    // Initial 8-bit sequence
    LCD_WriteCommand(address, 0x33);  // Initialize
    DELAY_MS(5);
    LCD_WriteCommand(address, 0x32);  // Set to 4-bit mode
    DELAY_MS(1);
    
    // Now in 4-bit mode
    LCD_WriteCommand(address, HD44780_FUNCTION_SET | HD44780_4_BIT_MODE | HD44780_2_LINE | HD44780_5x8_DOTS);
    LCD_WriteCommand(address, HD44780_DISPLAY_CONTROL | HD44780_DISPLAY_ON);
    LCD_WriteCommand(address, HD44780_CLEAR_DISPLAY);
    LCD_WriteCommand(address, HD44780_ENTRY_MODE_SET | HD44780_ENTRY_SHIFTINCREMENT);
    
    LCD_SetBacklight(address, 1);  // Turn on backlight
    DELAY_MS(2);
}
/**
 * @brief Writes a command to the LCD.
 * @param address I2C address of the LCD.
 * @param command Command byte to send.
 */
void LCD_WriteCommand(uint8_t address, uint8_t command) {
    LCD_Send(address, command, 0);
    DELAY_US(37);  // Command execution time
}

/**
 * @brief Writes a data byte to the LCD.
 * @param address I2C address of the LCD.
 * @param data Data byte to send.
 */
void LCD_WriteData(uint8_t address, uint8_t data) {
    LCD_Send(address, data, 1);
    DELAY_US(41);  // Data write time
}
/**
 * @brief Writes a string to the LCD.
 * @param address I2C address of the LCD.
 * @param str Null-terminated string to write.
 */

void LCD_WriteString(uint8_t address, const char* str) {
    while (*str) {
        LCD_WriteData(address, *str++);
    }
}

void LCD_WriteChar(uint8_t address, char c) {
    LCD_WriteData(address, c);
};
/**
 * @brief Sets the cursor position on the LCD.
 * @param address I2C address of the LCD.
 * @param col Column number (0-based).
 * @param row Row number (0-based).
 */
void LCD_SetCursor(uint8_t address, uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    uint8_t position = row_offsets[row] + col;
    LCD_WriteCommand(address, HD44780_SET_DDRRAM_ADDR | position);
}

/**
 * @brief Clears the LCD and resets the cursor position.
 * @param address I2C address of the LCD.
 */
void LCD_Clear(uint8_t address) {
    LCD_WriteCommand(address, HD44780_CLEAR_DISPLAY);
    DELAY_MS(2);  // Clear display requires more time
}
/**
 * @brief Clears a single line on the LCD.
 * @param address I2C address of the LCD.
 * @param row Row number (0-based).
 */
void LCD_ClearLine(uint8_t address, uint8_t row) {
    LCD_SetCursor(address, 0, row);
    for(uint8_t i = 0; i < 20; i++) {  // Assumes 20 column display
        LCD_WriteData(address, ' ');
    }
}
/**
 * @brief Prints a string centered on the LCD.
 * @param address I2C address of the LCD.
 * @param row Row number (0-based).
 * @param str Null-terminated string to print.
 */
void LCD_PrintCentered(uint8_t address, uint8_t row, const char* str) {
    uint8_t len = strlen(str);
    uint8_t pos = (20 - len) / 2;  // Assumes 20 column display
    LCD_SetCursor(address, pos, row);
    LCD_WriteString(address, str);
}
/**
 * @brief Scrolls text across the LCD display
 * @param address I2C address of the LCD
 * @param text Text to scroll
 * @param row Row to display the text (0-based)
 * @param delay_ms Delay between each scroll step in milliseconds
 * @param marquee If true, text will loop continuously. If false, scrolls once.
 */
void LCD_ScrollText(uint8_t address, const char* text, uint8_t row, uint16_t delay_ms, bool marquee) {
    size_t text_len = strlen(text);
    size_t display_width = 20;  // Assuming 20 column display, adjust if needed
    
    // If text is shorter than display, no need to scroll
    if (text_len <= display_width && !marquee) {
        LCD_SetCursor(address, 0, row);
        while (*text) {
            LCD_WriteData(address, *text++);
        }
        return;
    }
    
    // Create a buffer for the scrolling text
    // Add padding spaces for smooth scrolling
    char scroll_buffer[64];  // Max buffer size
    size_t buffer_len;
    
    if (marquee) {
        // For marquee, add spaces between repetitions
        sprintf(scroll_buffer, "%s    %s", text, text);
        buffer_len = text_len * 2 + 4;  // Text twice plus 4 spaces
    } else {
        // For single scroll, add padding spaces
        sprintf(scroll_buffer, "%s    ", text);
        buffer_len = text_len + 4;
    }
    
    size_t start_pos = 0;
    
    while (1) {
        // Clear the line first
        LCD_SetCursor(address, 0, row);
        for (size_t i = 0; i < display_width; i++) {
            LCD_WriteData(address, ' ');
        }
        
        // Write the visible portion of text
        LCD_SetCursor(address, 0, row);
        for (size_t i = 0; i < display_width; i++) {
            size_t char_pos = (start_pos + i) % buffer_len;
            LCD_WriteData(address, scroll_buffer[char_pos]);
        }
        
        start_pos = (start_pos + 1) % buffer_len;
        
        // For non-marquee mode, stop when text has scrolled completely
        if (!marquee && start_pos >= text_len) {
            break;
        }
        
        DELAY_MS(delay_ms);
    }
}
/**
 * @brief Enables or disables the LCD backlight.
 * @param address I2C address of the LCD.
 * @param state 1 to enable backlight, 0 to disable.
 */
void LCD_SetBacklight(uint8_t address, uint8_t state) {
    uint8_t backlight = state ? 0x08 : 0x00;
    uint8_t data = 0x00 | backlight;
    if (i2c_write(address, 0x00, &data, 1) != I2C_OK) {
        i2c_error_handler("Failed to set backlight");
    }
}

/*** Private Functions *******************************************************/

/**
 * @brief Sends a command or data to the LCD.
 * @param address I2C address of the LCD.
 * @param data Data byte to send.
 * @param mode Mode (0 for command, 1 for data).
 */
static void LCD_Send(uint8_t address, uint8_t data, uint8_t mode) {
    uint8_t high_nibble = (data & 0xF0) | (mode ? 0x01 : 0x00) | 0x04 | 0x08;  // Include backlight
    uint8_t low_nibble = ((data << 4) & 0xF0) | (mode ? 0x01 : 0x00) | 0x04 | 0x08;
    
    // Send high nibble
    uint8_t buf[3];
    buf[0] = high_nibble;
    buf[1] = high_nibble & ~0x04;  // Toggle EN low
    
    // Send low nibble
    buf[2] = low_nibble;
    buf[3] = low_nibble & ~0x04;   // Toggle EN low
    
    if (i2c_write(address, 0x00, buf, 4) != I2C_OK) {
        i2c_error_handler("Failed to send data to LCD");
    }
}
/**
 * @brief Checks for a specific I2C event.
 * @param event_mask Event mask to check.
 * @return 1 if the event matches, 0 otherwise.
 */
static uint8_t check_event(uint32_t event_mask) {
    uint32_t status = I2C1->STAR1 | (I2C1->STAR2 << 16);
    return (status & event_mask) == event_mask;
}

/**
 * @brief Handles I2C errors and resets the I2C peripheral.
 * @param error_message Error message to log.
 * @return 1 to indicate an error occurred.
 */
static uint8_t i2c_error_handler(const char *error_message) {
    printf("I2C Error: %s\n", error_message);
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;  // Reset I2C peripheral
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
    return 1;
}
