/*
Author: Obfuscated-d
Date: 2025-1-13
License: MIT
This is a project to monitor temperatures of up to two sensors, and turn on some fans at the set temp.
I wrote a library for those cheap i2c backpack hd44780 lcds, and I'm using it here.
this uses cnlohr's https://github.com/cnlohr/ch32v003fun library,
and the lib_i2c library https://github.com/ADBeta/CH32V003_lib_i2c.
you can find the lcd library here in lcd_i2c.h and lcd_i2c.c
this is just a personal project but if you find any of the code useful, you're free to use it.
I won't be providing any support for this code, but feel free to ask questions.

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
RELAYS:
	FAN1 -> PD1
	FAN2 -> PD2


*/

#include "ch32v003fun.h"
#include "funconfig.h"
#include "../lib/lcd_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TICK_NS 120
#define PULSES_PER_DETENT 4
#define BUTTON_PIN GPIO_Pin_3
#define DEBOUNCE_TIME 50 // Debounce time in milliseconds
#define SCREEN_TIMOUT 10000 // Screen timeout in milliseconds
#define SCK_PORT GPIOC
#define SCK_PIN 5 // PC5 for clock
#define MISO_PORT GPIOC
#define MISO_PIN 7 // PC7 for data
#define CS1_PORT GPIOD
#define CS1_PIN 0 // PD0 for first CS
#define FAN_1 1
#define FAN_2 2

// Menu states
typedef enum
{
	DISPLAYING_DATA,
	IN_MENU,
	EDITING_VALUE
} MenuState;

// Menu items
typedef enum
{
	SET_TEMP1,
	SET_TEMP2,
	SET_UNITS,
	EXIT,
	MENU_ITEMS_COUNT
} MenuItem;

// Global variables
MenuState currentState = DISPLAYING_DATA;
MenuItem selectedMenuItem = SET_TEMP1;
int menuOffset = 0;
const int MENU_DISPLAY_LINES = 4;
bool fahrenheit = true;
char units[3] = "F";
uint8_t counter;

// I2C LCD settings
int lcd_address = 0x27;
uint32_t i2c_clk_rate = 400000;

// Settings variables
int temperature1 = 80;
int temperature2 = 90;
uint16_t sensor1Value = 0;
uint16_t sensor2Value = 0;
uint32_t last_sensor_check = 0;

// Button handling
uint32_t lastInteractionTime = 0; // for screen timeout
volatile uint32_t lastButtonPress = 0;
volatile uint8_t buttonPressed = 0;
volatile bool backlight_state = 1;

// Encoder variables
volatile int oldPos = 0;
uint16_t initial_count;

// Function prototypes
void timer2_encoder_init( void );
const char *getMenuItemText( MenuItem item );
void updateMenu( uint8_t lcd_address );
void handleEncoder( uint8_t lcd_address, int32_t position );
void readSensors( void );
uint8_t checkButton( void );
uint32_t get_Time( void );

void setup_temp_sensor( void )
{
	// Enable GPIO ports
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;

	// Configure SCK (PC5) as output
	SCK_PORT->CFGLR &= ~( 0xf << ( 4 * SCK_PIN ) );
	SCK_PORT->CFGLR |= ( GPIO_Speed_10MHz | GPIO_CNF_OUT_PP ) << ( 4 * SCK_PIN );

	// Configure MISO (PC7) as input floating
	MISO_PORT->CFGLR &= ~( 0xf << ( 4 * MISO_PIN ) );
	MISO_PORT->CFGLR |= GPIO_CNF_IN_FLOATING << ( 4 * MISO_PIN );

	// Configure CS1 (PD0) as output
	CS1_PORT->CFGLR &= ~( 0xf << ( 4 * CS1_PIN ) );
	CS1_PORT->CFGLR |= ( GPIO_Speed_10MHz | GPIO_CNF_OUT_PP ) << ( 4 * CS1_PIN );

	// Set initial states
	SCK_PORT->OUTDR &= ~( 1 << SCK_PIN ); // SCK starts low
	CS1_PORT->OUTDR |= ( 1 << CS1_PIN ); // CS1 starts high

	Delay_Ms( 1 ); // Short delay after setup
}

// BITTY BITTY BANG BANG !!!
uint16_t read_single_sensor( GPIO_TypeDef *cs_port, uint8_t cs_pin )
{
	uint16_t raw_value = 0;

	
	cs_port->OUTDR |= ( 1 << cs_pin );
	Delay_Us( 50 );

	cs_port->OUTDR &= ~( 1 << cs_pin );
	Delay_Us( 50 );

	for ( int i = 15; i >= 0; i-- )
	{
		SCK_PORT->OUTDR &= ~( 1 << SCK_PIN );
		Delay_Us( 10 );

		if ( MISO_PORT->INDR & ( 1 << MISO_PIN ) )
		{
			raw_value |= ( 1 << i );
		}

		SCK_PORT->OUTDR |= ( 1 << SCK_PIN );
		Delay_Us( 10 );
	}

	cs_port->OUTDR |= ( 1 << cs_pin );
	Delay_Us( 50 );

	return raw_value;
}

void readSensors()
{
	char debug_buf[32];
	uint16_t raw1 = 0;


	raw1 = read_single_sensor( CS1_PORT, CS1_PIN );

	// Process readings
	if ( !( raw1 & 0x4 ) )
	{
		sensor1Value = ( raw1 >> 3 ) / 4;
		if ( fahrenheit )
		{
			sensor1Value = ( sensor1Value * 9.0 / 5.0 ) + 32;
		}
		else
		{
			sensor1Value = sensor1Value;
		}
	}
	else
	{
		sensor1Value = 0;
	}

	// debug output
	// LCD_SetCursor(0x27, 0, 3);
	// sprintf(debug_buf, "S1:%d%s", sensor1Value,units);
	// LCD_WriteString(0x27, debug_buf);
}


// simulated EEPROM with optinbytes
// Settings structure to pack/unpack from Option bytes
typedef struct
{
	uint8_t temperature1; // First temperature threshold
	uint8_t temperature2; // Second temperature threshold
	uint8_t reserved : 4; // Reserved for future use
} Settings;

void FlashOptionData( uint8_t data0, uint8_t data1 )
{
	volatile uint16_t hold[6];
	uint32_t *hold32p = (uint32_t *)hold;
	uint32_t *ob32p = (uint32_t *)OB_BASE;

	// Save current Option bytes
	hold32p[0] = ob32p[0]; // Copy RDPR and USER
	hold32p[1] = data0 + ( data1 << 16 ); // Copy in the two Data values to be written
	hold32p[2] = ob32p[2]; // Copy WRPR0 and WEPR1

	// Unlock Flash and Option bytes
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->OBKEYR = FLASH_KEY1;
	FLASH->OBKEYR = FLASH_KEY2;

	// Erase Option bytes
	FLASH->CTLR |= CR_OPTER_Set;
	FLASH->CTLR |= CR_STRT_Set;
	while ( FLASH->STATR & FLASH_BUSY );
	FLASH->CTLR &= CR_OPTER_Reset;

	// Write back values
	FLASH->CTLR |= CR_OPTPG_Set;
	uint16_t *ob16p = (uint16_t *)OB_BASE;
	for ( int i = 0; i < sizeof( hold ) / sizeof( hold[0] ); i++ )
	{
		ob16p[i] = hold[i];
		while ( FLASH->STATR & FLASH_BUSY );
	}

	FLASH->CTLR &= CR_OPTPG_Reset;
	FLASH->CTLR |= CR_LOCK_Set;
}

// Function to save all settings
void SaveSettings( Settings *settings )
{
	// Pack settings into data0
	uint8_t data0 = settings->temperature1;

	// Pack settings into data1
	uint8_t data1 = settings->temperature2 & 0xFF;

	FlashOptionData( data0, data1 );
}

// Function to load all settings
void LoadSettings( Settings *settings )
{
	// Read from Option bytes
	settings->temperature1 = OB->Data0;
	settings->temperature2 = OB->Data1 & 0xFF;
}


void timer2_encoder_init( void )
{
	// Enable GPIOD, TIM2, and AFIO
	RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOD;
	RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

	// Use NOREMAP (default) for D4/D3
	AFIO->PCFR1 &= ~( GPIO_PartialRemap1_TIM2 | GPIO_PartialRemap2_TIM2 | GPIO_FullRemap_TIM2 );

	// Configure D4 (CH1) as input with pullup
	GPIOD->CFGLR &= ~( 0xf << ( 4 * 4 ) );
	GPIOD->CFGLR |= ( GPIO_CNF_IN_PUPD ) << ( 4 * 4 );
	GPIOD->OUTDR |= 1 << 4; // Enable pullup

	// Configure D3 (CH2) as input with pullup
	GPIOD->CFGLR &= ~( 0xf << ( 4 * 3 ) );
	GPIOD->CFGLR |= ( GPIO_CNF_IN_PUPD ) << ( 4 * 3 );
	GPIOD->OUTDR |= 1 << 3; // Enable pullup

	// Reset Timer2
	RCC->APB1PRSTR |= RCC_APB1Periph_TIM2;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_TIM2;

	// Set encoder mode on Timer2
	TIM2->SMCFGR |= TIM_EncoderMode_TI12;

	// Initialize timer
	TIM2->SWEVGR |= TIM_UG;

	// Set initial count to mid-range
	TIM2->CNT = 0x8fff;
	initial_count = TIM2->CNT;

	// Enable Timer2
	TIM2->CTLR1 |= TIM_CEN;
}

const char *getMenuItemText( MenuItem item )
{
	switch ( item )
	{
		case SET_TEMP1: return "Set Temp 1";
		case SET_TEMP2: return "Set Temp 2";
		case SET_UNITS: return "Set Units";
		case EXIT: return "Exit Menu";
		default: return "";
	}
}


void updateMenu( uint8_t lcd_address )
{
	char buf[16];
	char temp_buf[16];

	LCD_Clear( lcd_address );

	switch ( currentState )
	{
		case IN_MENU:
			// Adjust menuOffset if selected item would be off screen
			if ( selectedMenuItem < menuOffset )
			{
				menuOffset = selectedMenuItem;
			}
			else if ( selectedMenuItem >= menuOffset + MENU_DISPLAY_LINES )
			{
				menuOffset = selectedMenuItem - MENU_DISPLAY_LINES + 1;
			}

			// Display visible menu items
			for ( int i = 0; i < MENU_DISPLAY_LINES && ( menuOffset + i ) < MENU_ITEMS_COUNT; i++ )
			{
				MenuItem currentItem = menuOffset + i;
				LCD_SetCursor( lcd_address, 0, i );

				// Show cursor for selected item
				if ( currentItem == selectedMenuItem )
				{
					LCD_WriteString( lcd_address, ">" );
				}
				else
				{
					LCD_WriteString( lcd_address, " " );
				}

				LCD_WriteString( lcd_address, " " );
				LCD_WriteString( lcd_address, getMenuItemText( currentItem ) );
			}
			break;

		case EDITING_VALUE:
			LCD_SetCursor( lcd_address, 0, 0 );
			switch ( selectedMenuItem )
			{
				case SET_TEMP1:
					LCD_WriteString( lcd_address, "Set Temp 1:" );
					LCD_SetCursor( lcd_address, 0, 1 );
					LCD_WriteString( lcd_address, "> " );
					sprintf( buf, "%d", temperature1 );
					LCD_WriteString( lcd_address, buf );
					LCD_WriteChar( lcd_address, 223 ); // Degree symbol
					break;

				case SET_TEMP2:
					LCD_WriteString( lcd_address, "Set Temp 2:" );
					LCD_SetCursor( lcd_address, 0, 1 );
					LCD_WriteString( lcd_address, "> " );
					sprintf( buf, "%d", temperature2 );
					LCD_WriteString( lcd_address, buf );
					LCD_WriteChar( lcd_address, 223 );
					break;

				case SET_UNITS:
					LCD_WriteString( lcd_address, "Set Units:" );
					LCD_SetCursor( lcd_address, 0, 1 );
					LCD_WriteString( lcd_address, "> " );
					sprintf( buf, "%s", units );
					LCD_WriteString( lcd_address, units );
					break;

				case EXIT: currentState = DISPLAYING_DATA; break;
				case MENU_ITEMS_COUNT: break; // Should never happen
			}
			break;

		case DISPLAYING_DATA:
			// First temperature setting
			LCD_SetCursor( lcd_address, 0, 0 );
			sprintf( temp_buf, "T1:%d", temperature1 );
			LCD_WriteString( lcd_address, temp_buf );
			LCD_WriteChar( lcd_address, 223 ); // Degree symbol

			// Second temperature setting
			LCD_SetCursor( lcd_address, 0, 1 );
			sprintf( temp_buf, "T2:%d", temperature2 );
			LCD_WriteString( lcd_address, temp_buf );
			LCD_WriteChar( lcd_address, 223 );

			// Display sensor readings
			LCD_SetCursor( lcd_address, 0, 3 );
			sprintf( temp_buf, "Reading:%d%s    ", sensor1Value, units );
			LCD_WriteString( lcd_address, temp_buf );
			break;
	}
}


// Handle encoder input and update menu state
void handleEncoder( uint8_t lcd_address, int32_t position )
{
	lastInteractionTime = get_Time();
	static int32_t lastPosition = 0;
	if ( position != lastPosition )
	{

		int8_t delta = ( position > lastPosition ) ? 1 : -1;
		lastPosition = position;

		if ( currentState == DISPLAYING_DATA )
		{
			currentState = IN_MENU;
			selectedMenuItem = SET_TEMP1;
			menuOffset = 0;
			updateMenu( lcd_address );
		}
		else if ( currentState == IN_MENU )
		{
			int8_t newItem = selectedMenuItem + delta;
			if ( newItem >= 0 && newItem < MENU_ITEMS_COUNT )
			{
				selectedMenuItem = newItem;
				updateMenu( lcd_address );
			}
		}
		else if ( currentState == EDITING_VALUE )
		{
			switch ( selectedMenuItem )
			{
				case SET_TEMP1:
					if ( delta > 0 && temperature1 < 255 )
						temperature1++;
					else if ( delta < 0 && temperature1 > 0 )
						temperature1--;
					break;

				case SET_TEMP2:
					if ( delta > 0 && temperature2 < 255 )
						temperature2++;
					else if ( delta < 0 && temperature2 > 0 )
						temperature2--;
					break;
				case SET_UNITS:
					if ( delta > 0 )
					{
						fahrenheit = !fahrenheit;
						if ( fahrenheit )
						{
							units[0] = 'F';
						}
						else
						{
							units[0] = 'C';
						}
					}
					break;
			}
			updateMenu( lcd_address );
		}
	}
}

uint32_t get_Time( void )
{
	uint32_t currTick = SysTick->CNT;
	return ( currTick * TICK_NS ) / 1e6;
}

uint8_t checkButton( void )
{
	static uint32_t lastDebounceTime = 0;
	static uint8_t lastButtonState = 1;
	static uint8_t buttonState = 1;

	uint8_t reading = ( GPIOC->INDR & BUTTON_PIN ) ? 1 : 0;
	uint32_t currentTime = get_Time();

	if ( reading != lastButtonState )
	{
		lastDebounceTime = currentTime;
	}

	lastButtonState = reading;

	if ( ( currentTime - lastDebounceTime ) > DEBOUNCE_TIME )
	{
		if ( reading != buttonState )
		{
			buttonState = reading;

			if ( buttonState == 0 )
			{ // Button pressed
				lastInteractionTime = currentTime; 

				// If backlight is off, turn it on and consume press
				if ( !backlight_state )
				{
					backlight_state = true;
					LCD_SetBacklight( 0x27, 1 );
					return 0;
				}
				return 1; // Return 1 to process menu action
			}
		}
	}
	return 0;
}

int main()
{
	SystemInit();
	backlight_state = true;
	lastInteractionTime = get_Time();
	setup_temp_sensor();
	// Enable GPIO for LCD and button
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC;

	// Initialize encoder using Timer2
	timer2_encoder_init();


	LCD_Init( lcd_address, i2c_clk_rate );
	LCD_Clear( lcd_address );
	LCD_SetBacklight( lcd_address, 1 );


	// Configure PC3 as input with pull-up
	GPIOC->CFGLR &= ~( 0xF << ( 4 * 3 ) ); // Clear PC3 configuration
	GPIOC->CFGLR |= ( GPIO_CNF_IN_PUPD << ( 4 * 3 ) ); // Set as input with pull-up/down
	GPIOC->BSHR = GPIO_Pin_3; // Set PC3 high to enable pull-up
	// Load settings from EEPROM
	Settings settings;
	LoadSettings( &settings );

	// Configure output pins
	GPIOD->CFGLR &= ~( 0xF << ( 4 * FAN_1 ) ); // Clear pin1 configuration
	GPIOD->CFGLR |= ( GPIO_CNF_OUT_PP << ( 4 * FAN_1 ) ); // Set as output push-pull
	GPIOD->OUTDR |= 0 << FAN_1; // Set pin1 low
	GPIOD->CFGLR &= ~( 0xF << ( 4 * FAN_2 ) ); // Clear pin2 configuration
	GPIOD->CFGLR |= ( GPIO_CNF_OUT_PP << ( 4 * FAN_2 ) ); // Set as output push-pull
	GPIOD->OUTDR |= 0 << FAN_2; // Set pin2 low

	// Update global variables with loaded settings
	temperature1 = settings.temperature1;
	temperature2 = settings.temperature2;

	// Initial display update
	updateMenu( lcd_address );

	while ( 1 )
	{
		// Handle encoder
		uint16_t current_count = TIM2->CNT;
		int32_t position = (int32_t)current_count - initial_count;
		int current_pos = position / PULSES_PER_DETENT;

		if ( oldPos != current_pos )
		{
			lastInteractionTime = get_Time();
			handleEncoder( lcd_address, current_pos );
			oldPos = current_pos;
		}

		// Handle button
		if ( checkButton() )
		{
			lastInteractionTime = get_Time();
			switch ( currentState )
			{
				case IN_MENU:
					if ( selectedMenuItem == EXIT )
					{
						// Save settings before exiting menu
						settings.temperature1 = temperature1;
						settings.temperature2 = temperature2;
						SaveSettings( &settings );
						currentState = DISPLAYING_DATA;
					}
					else
					{
						currentState = EDITING_VALUE;
					}
					break;

				case EDITING_VALUE: currentState = IN_MENU; break;

				case DISPLAYING_DATA:
					currentState = IN_MENU;
					selectedMenuItem = SET_TEMP1;
					menuOffset = 0;
					break;
			}
			updateMenu( lcd_address );
		}


		uint32_t current_time = get_Time();

		if ( current_time - last_sensor_check > 1000 )
		{ 
			readSensors();
			counter++;
			if ( counter > 10 )
			{
				if ( current_time - lastInteractionTime > 10000 )
				{
					backlight_state = false;
					counter = 0;
					if(currentState == DISPLAYING_DATA)
					{
					LCD_SetBacklight( 0x27, 0 );
					}
					else{
						lastInteractionTime = get_Time();
						backlight_state = true;
					}
				}
			}
			last_sensor_check = current_time;
			if ( backlight_state )
			{
				updateMenu( lcd_address );
			}
		}
		Delay_Ms( 10 );
	}
}