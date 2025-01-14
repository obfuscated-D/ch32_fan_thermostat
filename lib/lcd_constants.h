#ifndef LCD_CONSTANTS_H
#define LCD_CONSTANTS_H

// HD44780 Instruction Set
#define HD44780_CLEAR_DISPLAY         0x01
#define HD44780_CURSOR_HOME           0x02

// Entry Mode Set
#define HD44780_ENTRY_MODE_SET        0x04
#define HD44780_ENTRY_SHIFTINCREMENT  0x02
#define HD44780_ENTRY_SHIFTDECREMENT  0x00

// Display Control
#define HD44780_DISPLAY_CONTROL       0x08
#define HD44780_DISPLAY_ON            0x04
#define HD44780_DISPLAY_OFF           0x00
#define HD44780_CURSOR_ON             0x02
#define HD44780_CURSOR_OFF            0x00
#define HD44780_BLINK_ON              0x01
#define HD44780_BLINK_OFF             0x00

// Cursor or Display Shift
#define HD44780_CURSOR_OR_DISPLAY_SHIFT 0x10
#define HD44780_DISPLAY_SHIFT          0x08
#define HD44780_CURSOR_MOVE            0x00
#define HD44780_SHIFT_RIGHT            0x04
#define HD44780_SHIFT_LEFT             0x00

// Function Set
#define HD44780_FUNCTION_SET          0x20
#define HD44780_8_BIT_MODE            0x10
#define HD44780_4_BIT_MODE            0x00
#define HD44780_2_LINE                0x08
#define HD44780_1_LINE                0x00
#define HD44780_5x10_DOTS             0x04
#define HD44780_5x8_DOTS              0x00

// Set CGRAM Address
#define HD44780_SET_CGRAM_ADDR        0x40

// Set DDRAM Address
#define HD44780_SET_DDRRAM_ADDR       0x80

#endif // LCD_CONSTANTS_H
