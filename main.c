#include "asf.h"
#include "delay.h"

/*************************************************************************/
/* Debugging															 */
/*************************************************************************/

#define STRING_EOL		"\r\n"
#define STRING_HEADER	"-- Short/Long press button detection with signal debouncing for SAMW25 XPLAINED PRO -- " STRING_EOL \
						"-- "BOARD_NAME " --"STRING_EOL	\
						"-- Compiled: "__DATE__ " "__TIME__ " --" STRING_EOL

// UART module for debug
static struct usart_module g_uart_module;

/**
 * UART console configuration
 */
static void configure_console(void) {
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&g_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&g_uart_module);
}
						
/*************************************************************************/
/* Button debouncing using external interrupt							 */
/*************************************************************************/
#define PIN_STATE_HIGH	true
#define PIN_STATE_LOW	false

// Number of GPIO we are going to use
#define DEBOUNCED_PIN_COUNT	1

// Alias name for the actual GPIO pin on the board
#define MY_ACTION_BUTTON			BUTTON_0_PIN

// MUX position the MY_ACTION_BUTTON GPIO pin should be configured to
#define MY_ACTION_BUTTON_EIC_MUX	BUTTON_0_EIC_MUX

// External Interrupt channel to configure for MY_ACTION_BUTTON GPIO pin
#define MY_ACTION_BUTTON_EIC_LINE	BUTTON_0_EIC_LINE

// Alias name for the index of the pin number in the array of pin numbers
#define DEBOUNCED_MY_ACTION_BUTTON_INDEX	0

// The maximum threshold for the short press counter
#define DEBOUNCE_THRESHOLD_SHORT_PRESS_MAX	20

// The threshold that determines when we have indeed a "high" i.e. button press in this example
#define DEBOUNCE_THRESHOLD_SHORT_PRESS_HIGH 16

// The threshold that determines when we have indeed a "low" i.e. button release in this example
#define DEBOUNCE_THRESHOLD_SHORT_PRESS_LOW	4

// The maximum threshold for the long press counter
#define DEBOUNCE_THRESHOLD_LONG_PRESS_MAX	((uint16_t) 1500)

// The threshold for the long press counter that determines when we have a long press
#define DEBOUNCE_THRESHOLD_LONG_PRESS_HIGH	((uint16_t) 1496)

// All pin numbers we are going to debounce
uint8_t g_debounced_pins[DEBOUNCED_PIN_COUNT]					= {MY_ACTION_BUTTON};
	
// Counters for every pin
uint8_t g_debounce_short_press_count[DEBOUNCED_PIN_COUNT]		= {0};
	
// Falling flag for every pin we are debouncing
bool g_debounce_short_press_falling_flag[DEBOUNCED_PIN_COUNT]	= {false};
	
// Current state for every pin we are debouncing
bool g_debounce_short_press_current_state[DEBOUNCED_PIN_COUNT]	= {PIN_STATE_LOW};
	
// Rising flag for every ping we are debouncing
bool g_debounce_press_rising_flag[DEBOUNCED_PIN_COUNT]			= {false};

// Long press counter for every pin we are debouncing
uint16_t g_debounce_long_press_count[DEBOUNCED_PIN_COUNT]		= {0};

// Falling flag for long holds for every pin we are debouncing
bool g_debounce_long_press_falling_flag[DEBOUNCED_PIN_COUNT]	= {false};
	
void debounce_init() {
	bool isCurrentStateHigh;
	for(int i; i < DEBOUNCED_PIN_COUNT; i++) {
		isCurrentStateHigh = port_pin_get_input_level(g_debounced_pins[i]);
		
		if (isCurrentStateHigh) {
			g_debounce_short_press_current_state[i] = PIN_STATE_HIGH;
			g_debounce_short_press_count[i] = DEBOUNCE_THRESHOLD_SHORT_PRESS_MAX;
		} else {
			g_debounce_short_press_current_state[i] = PIN_STATE_LOW;
			g_debounce_short_press_count[i] = 0;
		}
		
		g_debounce_long_press_count[i] = 0;
			
		g_debounce_short_press_falling_flag[i] = false;
		g_debounce_press_rising_flag[i] = false;
	}
}

void debounce_update() {	
	bool isCurrentStateHigh;
	for(int i; i < DEBOUNCED_PIN_COUNT; i++) {
		isCurrentStateHigh = port_pin_get_input_level(g_debounced_pins[i]);
		
		if(isCurrentStateHigh) {
			// High
			
			// Short press
			if (g_debounce_short_press_count[i] < DEBOUNCE_THRESHOLD_SHORT_PRESS_MAX) {
				g_debounce_short_press_count[i]++;
			}
			
			if(g_debounce_short_press_count[i] == DEBOUNCE_THRESHOLD_SHORT_PRESS_HIGH) {
				if(g_debounce_short_press_current_state[i] == PIN_STATE_LOW) {
					g_debounce_short_press_current_state[i] = PIN_STATE_HIGH;
					g_debounce_press_rising_flag[i] = true;
 					
 					// Reset the long press counter
 					g_debounce_long_press_count[i] = 0;
				}
			}
		} else {
			// Low
			
			// Short press
			if(g_debounce_short_press_count[i] > 0) {
				g_debounce_short_press_count[i]--;
			}
			
			if (g_debounce_short_press_count[i] == DEBOUNCE_THRESHOLD_SHORT_PRESS_LOW) {
				if (g_debounce_short_press_current_state[i] == PIN_STATE_HIGH) {
					g_debounce_short_press_current_state[i] = PIN_STATE_LOW;
					g_debounce_short_press_falling_flag[i] = true;
				}
			}
			
			// Long press
			if (g_debounce_long_press_count[i] < DEBOUNCE_THRESHOLD_LONG_PRESS_MAX) {
				g_debounce_long_press_count[i]++;
			}
					
			if(g_debounce_long_press_count[i] == DEBOUNCE_THRESHOLD_LONG_PRESS_HIGH) {
				if (g_debounce_short_press_current_state[i] == PIN_STATE_LOW) {
					g_debounce_long_press_falling_flag[i] = true;
				}
			}
		}	
	}
}

bool debounce_short_press_falling_detected(uint8_t pinIndex) {
	if(g_debounce_short_press_falling_flag[pinIndex]) {
		g_debounce_short_press_falling_flag[pinIndex] = false;
		return true;
	}
	return false;
}

bool debounce_long_press_falling_detected(uint8_t pinIndex) {
	if(g_debounce_long_press_falling_flag[pinIndex]) {
		g_debounce_long_press_falling_flag[pinIndex] = false;
		return true;
	}
	return false;
}

bool debounce_press_rising_detected(uint8_t pinIndex) {
	if(g_debounce_press_rising_flag[pinIndex]) {
		g_debounce_press_rising_flag[pinIndex] = false;
		return true;
	}
	return false;
}

void configure_extint_channel(void) {
	struct extint_chan_conf config_extint_chan;
	extint_chan_get_config_defaults(&config_extint_chan);
	
	config_extint_chan.gpio_pin_pull		= EXTINT_PULL_UP;
	config_extint_chan.detection_criteria	= EXTINT_DETECT_FALLING;
	config_extint_chan.filter_input_signal	= true;
	
	config_extint_chan.gpio_pin		= MY_ACTION_BUTTON;
	config_extint_chan.gpio_pin_mux = MY_ACTION_BUTTON_EIC_MUX;
	extint_chan_set_config(MY_ACTION_BUTTON_EIC_LINE, &config_extint_chan);
}

/**
 * Application entry point
 *
 * Returns program return value
 */
int main(void) {
	// Initialize the board
	system_init();

	// Initialize the UART console
	configure_console();
	
	// Print welcome message
	printf(STRING_HEADER);
	
	// Set the external interrupt channel configuration
	configure_extint_channel();
	
	// Enable global interrupts in the device to fire any enabled interrupt handlers
	system_interrupt_enable_global();
	
	// Initialize the delay routine
	delay_init();
	
	// Initialize the GPIO pin debounce configuration
	debounce_init();

	// Keep the app alive
	while (1) {
		// Update the state of all GPIO pins we are interested in
		debounce_update();
		
		if(debounce_short_press_falling_detected(DEBOUNCED_MY_ACTION_BUTTON_INDEX)) {
			puts("MY_ACTION_BUTTON: Short press detected");
		}
		
		if(debounce_long_press_falling_detected(DEBOUNCED_MY_ACTION_BUTTON_INDEX)) {
			puts("MY_ACTION_BUTTON: Long press detected");
		}
	
		if(debounce_press_rising_detected(DEBOUNCED_MY_ACTION_BUTTON_INDEX)) {
			puts("MY_ACTION_BUTTON: Button released");
		}
		
		// To make this example simple, we are using the main clock for falling/rising detection. However, normally we will use a timer and a slower clock.
		// Since the main clock is too fast, counting even up to 2^16 is too quick, so it doesn't take a lot of time to detect a long press.
		// Therefore, we'll slow down with 300 microseconds using a delay routine. 
		delay_us(300);
	}

	return 0;
}
