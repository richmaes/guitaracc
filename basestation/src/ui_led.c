/**
 * @file ui_led.c
 * @brief RGB LED UI controller implementation for nRF5340 Audio DK
 * 
 * Uses the onboard RGB LEDs to provide visual feedback for system state.
 * The nRF5340 Audio DK has RGB LED1 available.
 */

#include "ui_led.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui_led, LOG_LEVEL_DBG);

/* LED device tree nodes - nRF5340 Audio DK has RGB LEDs */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

/* Validate LED nodes exist */
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED1_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED2_NODE, okay)
#error "LED device tree nodes not properly defined"
#endif

/* LED GPIO specifications */
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

/* Current UI state */
static struct {
    ui_led_color_t current_color;
    ui_led_pattern_t current_pattern;
    uint8_t connection_count;
    bool flash_active;
    k_tid_t led_thread_tid;
} ui_state = {
    .current_color = UI_LED_OFF,
    .current_pattern = UI_LED_PATTERN_SOLID,
    .connection_count = 0,
    .flash_active = false,
};

/* LED update thread stack */
#define LED_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY 7
K_THREAD_STACK_DEFINE(led_thread_stack, LED_THREAD_STACK_SIZE);
static struct k_thread led_thread_data;

/**
 * @brief Set the physical RGB LED state
 * 
 * @param color Color to display
 */
static void set_led_hardware(ui_led_color_t color)
{
    bool red_on   = (color == UI_LED_RED || color == UI_LED_YELLOW || 
                     color == UI_LED_MAGENTA || color == UI_LED_WHITE);
    bool green_on = (color == UI_LED_GREEN || color == UI_LED_YELLOW || 
                     color == UI_LED_CYAN || color == UI_LED_WHITE);
    bool blue_on  = (color == UI_LED_BLUE || color == UI_LED_CYAN || 
                     color == UI_LED_MAGENTA || color == UI_LED_WHITE);

    gpio_pin_set_dt(&led_red, red_on);
    gpio_pin_set_dt(&led_green, green_on);
    gpio_pin_set_dt(&led_blue, blue_on);
}

/**
 * @brief LED update thread function
 * 
 * Handles LED patterns (blinking, pulsing, etc.)
 */
static void led_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    uint32_t counter = 0;
    bool led_on = true;

    while (1) {
        if (ui_state.flash_active) {
            /* Flash is handled externally, just sleep */
            k_msleep(10);
            counter = 0;
            continue;
        }

        switch (ui_state.current_pattern) {
            case UI_LED_PATTERN_SOLID:
                set_led_hardware(ui_state.current_color);
                k_msleep(100);
                break;

            case UI_LED_PATTERN_BLINK_SLOW:
                /* 1Hz blink: 500ms on, 500ms off */
                if (counter % 10 == 0) {
                    led_on = !led_on;
                    set_led_hardware(led_on ? ui_state.current_color : UI_LED_OFF);
                }
                k_msleep(50);
                counter++;
                break;

            case UI_LED_PATTERN_BLINK_FAST:
                /* 4Hz blink: 125ms on, 125ms off */
                if (counter % 3 == 0) {
                    led_on = !led_on;
                    set_led_hardware(led_on ? ui_state.current_color : UI_LED_OFF);
                }
                k_msleep(40);
                counter++;
                break;

            case UI_LED_PATTERN_PULSE:
                /* Breathing effect - simplified as slow blink for now */
                if (counter % 10 == 0) {
                    led_on = !led_on;
                    set_led_hardware(led_on ? ui_state.current_color : UI_LED_OFF);
                }
                k_msleep(50);
                counter++;
                break;

            default:
                set_led_hardware(UI_LED_OFF);
                k_msleep(100);
                break;
        }
    }
}

int ui_led_init(void)
{
    int ret;

    /* Configure LED GPIOs */
    if (!device_is_ready(led_red.port)) {
        LOG_ERR("Red LED GPIO device not ready");
        return -ENODEV;
    }

    if (!device_is_ready(led_green.port)) {
        LOG_ERR("Green LED GPIO device not ready");
        return -ENODEV;
    }

    if (!device_is_ready(led_blue.port)) {
        LOG_ERR("Blue LED GPIO device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure red LED: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure green LED: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure blue LED: %d", ret);
        return ret;
    }

    /* Start LED update thread */
    ui_state.led_thread_tid = k_thread_create(&led_thread_data,
                                               led_thread_stack,
                                               K_THREAD_STACK_SIZEOF(led_thread_stack),
                                               led_thread_fn,
                                               NULL, NULL, NULL,
                                               LED_THREAD_PRIORITY,
                                               0, K_NO_WAIT);

    k_thread_name_set(ui_state.led_thread_tid, "ui_led");

    LOG_INF("UI LED subsystem initialized");

    /* Set initial state */
    ui_led_set_state(UI_STATE_INIT);

    return 0;
}

void ui_led_set_state(ui_state_t state)
{
    switch (state) {
        case UI_STATE_INIT:
            ui_state.current_color = UI_LED_YELLOW;
            ui_state.current_pattern = UI_LED_PATTERN_SOLID;
            break;

        case UI_STATE_SCANNING:
            ui_state.current_color = UI_LED_BLUE;
            ui_state.current_pattern = UI_LED_PATTERN_BLINK_SLOW;
            break;

        case UI_STATE_CONNECTED_1:
            ui_state.current_color = UI_LED_GREEN;
            ui_state.current_pattern = UI_LED_PATTERN_SOLID;
            break;

        case UI_STATE_CONNECTED_2:
            ui_state.current_color = UI_LED_CYAN;
            ui_state.current_pattern = UI_LED_PATTERN_SOLID;
            break;

        case UI_STATE_CONNECTED_3:
            ui_state.current_color = UI_LED_CYAN;
            ui_state.current_pattern = UI_LED_PATTERN_BLINK_SLOW;
            break;

        case UI_STATE_CONNECTED_4:
            ui_state.current_color = UI_LED_WHITE;
            ui_state.current_pattern = UI_LED_PATTERN_SOLID;
            break;

        case UI_STATE_MIDI_ACTIVE:
            /* Brief flash handled by ui_led_flash() */
            break;

        case UI_STATE_ERROR:
            ui_state.current_color = UI_LED_RED;
            ui_state.current_pattern = UI_LED_PATTERN_BLINK_FAST;
            break;

        default:
            ui_state.current_color = UI_LED_OFF;
            ui_state.current_pattern = UI_LED_PATTERN_SOLID;
            break;
    }

    LOG_DBG("UI state changed: color=%d, pattern=%d", 
            ui_state.current_color, ui_state.current_pattern);
}

void ui_led_set_color_pattern(ui_led_color_t color, ui_led_pattern_t pattern)
{
    ui_state.current_color = color;
    ui_state.current_pattern = pattern;
}

void ui_led_flash(ui_led_color_t color, uint32_t duration_ms)
{
    ui_state.flash_active = true;
    set_led_hardware(color);
    k_msleep(duration_ms);
    ui_state.flash_active = false;
}

void ui_led_update_connection_count(uint8_t count)
{
    ui_state.connection_count = count;

    switch (count) {
        case 0:
            ui_led_set_state(UI_STATE_SCANNING);
            break;
        case 1:
            ui_led_set_state(UI_STATE_CONNECTED_1);
            break;
        case 2:
            ui_led_set_state(UI_STATE_CONNECTED_2);
            break;
        case 3:
            ui_led_set_state(UI_STATE_CONNECTED_3);
            break;
        case 4:
            ui_led_set_state(UI_STATE_CONNECTED_4);
            break;
        default:
            LOG_WRN("Invalid connection count: %d", count);
            break;
    }

    LOG_INF("Connection count updated: %d", count);
}
