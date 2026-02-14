/**
 * @file ui_led.h
 * @brief RGB LED UI controller for GuitarAcc basestation
 * 
 * Provides visual feedback for system state and BLE connection status:
 * - System initialization and startup state
 * - BLE scanning and connection status
 * - MIDI activity indication
 * - Error states
 */

#ifndef UI_LED_H
#define UI_LED_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @brief UI LED color definitions
 */
typedef enum {
    UI_LED_OFF = 0,        /**< All LEDs off */
    UI_LED_RED,            /**< Red only */
    UI_LED_GREEN,          /**< Green only */
    UI_LED_BLUE,           /**< Blue only */
    UI_LED_YELLOW,         /**< Red + Green */
    UI_LED_CYAN,           /**< Green + Blue */
    UI_LED_MAGENTA,        /**< Red + Blue */
    UI_LED_WHITE           /**< All colors on */
} ui_led_color_t;

/**
 * @brief UI LED pattern types
 */
typedef enum {
    UI_LED_PATTERN_SOLID,      /**< Solid color */
    UI_LED_PATTERN_BLINK_SLOW, /**< Blink at 1Hz */
    UI_LED_PATTERN_BLINK_FAST, /**< Blink at 4Hz */
    UI_LED_PATTERN_PULSE       /**< Breathing effect */
} ui_led_pattern_t;

/**
 * @brief UI state indicators
 */
typedef enum {
    UI_STATE_INIT,             /**< System initializing - yellow solid */
    UI_STATE_SCANNING,         /**< BLE scanning - blue slow blink */
    UI_STATE_CONNECTED_1,      /**< 1 client connected - green solid */
    UI_STATE_CONNECTED_2,      /**< 2 clients connected - cyan solid */
    UI_STATE_CONNECTED_3,      /**< 3 clients connected - cyan blink slow */
    UI_STATE_CONNECTED_4,      /**< 4 clients connected - white solid */
    UI_STATE_MIDI_ACTIVE,      /**< MIDI data flowing - brief white flash */
    UI_STATE_ERROR             /**< Error state - red fast blink */
} ui_state_t;

/**
 * @brief Initialize the UI LED subsystem
 * 
 * Configures GPIO pins for RGB LED control and starts the LED update thread.
 * 
 * @return 0 on success, negative error code on failure
 */
int ui_led_init(void);

/**
 * @brief Set the system UI state
 * 
 * Updates LED color and pattern based on current system state.
 * 
 * @param state The UI state to display
 */
void ui_led_set_state(ui_state_t state);

/**
 * @brief Set LED to a specific color and pattern
 * 
 * @param color LED color to display
 * @param pattern LED pattern (solid, blink, pulse)
 */
void ui_led_set_color_pattern(ui_led_color_t color, ui_led_pattern_t pattern);

/**
 * @brief Briefly flash the LED (for activity indication)
 * 
 * Shows a brief flash without changing the current state pattern.
 * Used for indicating MIDI activity.
 * 
 * @param color Color to flash
 * @param duration_ms Flash duration in milliseconds
 */
void ui_led_flash(ui_led_color_t color, uint32_t duration_ms);

/**
 * @brief Update the number of connected clients
 * 
 * Automatically adjusts LED state based on connection count.
 * 
 * @param count Number of connected clients (0-4)
 */
void ui_led_update_connection_count(uint8_t count);

#endif /* UI_LED_H */
