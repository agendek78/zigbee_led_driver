/*
 *  Zigbee 3.0 4-channel LED strip driver.
 *  Copyright (C) 2022 Andrzej Gendek
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "led_channel.h"
#include "sl_pwm_led.h"
#include "pin_config.h"
#include "dbg_log.h"
#include "app.h"

#include <stddef.h>

#define LED_CHANNEL_PWM_FREQ        1000
#define LED_CHANNEL_RES             254

const uint8_t cie_100_254[] = {
    0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 4, 4, 4, 5, 5, 6, 6,
    7, 8, 8, 9, 10, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 24, 25,
    26, 28, 29, 31, 33, 34, 36, 38, 40, 41,
    43, 45, 47, 50, 52, 54, 56, 59, 61, 64,
    66, 69, 72, 74, 77, 80, 83, 86, 89, 93,
    96, 99, 103, 106, 110, 114, 117, 121, 125, 129,
    133, 138, 142, 146, 151, 155, 160, 165, 170, 174,
    180, 185, 190, 195, 201, 206, 212, 217, 223, 229,
    235, 254
};

const uint8_t cie_254_254[] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
    3, 3, 4, 4, 4, 4, 4, 4, 4, 5,
    5, 5, 5, 5, 6, 6, 6, 6, 6, 7,
    7, 7, 7, 8, 8, 8, 8, 9, 9, 9,
    9, 10, 10, 10, 11, 11, 11, 11, 12, 12,
    12, 13, 13, 13, 14, 14, 15, 15, 15, 16,
    16, 17, 17, 17, 18, 18, 19, 19, 20, 20,
    21, 21, 21, 22, 22, 23, 23, 24, 25, 25,
    26, 26, 27, 27, 28, 28, 29, 30, 30, 31,
    31, 32, 33, 33, 34, 35, 35, 36, 37, 37,
    38, 39, 40, 40, 41, 42, 43, 43, 44, 45,
    46, 47, 47, 48, 49, 50, 51, 52, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 77, 78, 79, 80, 81, 82, 84, 85,
    86, 87, 88, 90, 91, 92, 94, 95, 96, 97,
    99, 100, 102, 103, 104, 106, 107, 109, 110, 111,
    113, 114, 116, 117, 119, 120, 122, 123, 125, 127,
    128, 130, 131, 133, 135, 136, 138, 140, 141, 143,
    145, 147, 148, 150, 152, 154, 155, 157, 159, 161,
    163, 165, 167, 169, 170, 172, 174, 176, 178, 180,
    182, 184, 186, 188, 191, 193, 195, 197, 199, 201,
    203, 205, 208, 210, 212, 214, 217, 219, 221, 223,
    226, 228, 231, 233, 235, 254,
};

static sl_led_pwm_t channels[] =
{
    {
        .port = LED_CH1_PORT,
        .pin = LED_CH1_PIN,
        .level = LED_CHANNEL_RES - 1,
        .polarity = 0,
        .channel = 0,
        .timer = TIMER1,
        .frequency = LED_CHANNEL_PWM_FREQ,
        .resolution = LED_CHANNEL_RES,
    },
    {
        .port = LED_CH2_PORT,
        .pin = LED_CH2_PIN,
        .level = LED_CHANNEL_RES - 1,
        .polarity = 0,
        .channel = 1,
        .timer = TIMER1,
        .frequency = LED_CHANNEL_PWM_FREQ,
        .resolution = LED_CHANNEL_RES,
    },
    {
        .port = LED_CH3_PORT,
        .pin = LED_CH3_PIN,
        .level = LED_CHANNEL_RES - 1,
        .polarity = 0,
        .channel = 2,
        .timer = TIMER1,
        .frequency = LED_CHANNEL_PWM_FREQ,
        .resolution = LED_CHANNEL_RES,
    },
    {
        .port = LED_CH4_PORT,
        .pin = LED_CH4_PIN,
        .level = LED_CHANNEL_RES - 1,
        .polarity = 0,
        .channel = 0,
        .timer = TIMER2,
        .frequency = LED_CHANNEL_PWM_FREQ,
        .resolution = LED_CHANNEL_RES,
    },
    {
        .port = LED_AUX_PORT,
        .pin = LED_AUX_PIN,
        .level = LED_CHANNEL_RES - 1,
        .polarity = 1,
        .channel = 0,
        .timer = TIMER3,
        .frequency = LED_CHANNEL_PWM_FREQ,
        .resolution = LED_CHANNEL_RES,
    }
};

static uint32_t channels_mask = 0;

void led_channel_init(void)
{
    for(size_t i = 0; i < ARRAY_SIZE(channels); i++)
    {
        if (i < ARRAY_SIZE(channels) - 1)
        {
            GPIO_PinModeSet(channels[i].port,
                            channels[i].pin,
                            gpioModeInputPull,
                            1);
            bool disabled = GPIO_PinInGet(channels[i].port, channels[i].pin) != 0;

            DBG_LOG("Channel %d is %s", i, disabled ? "disabled" : "enabled");
            if (!disabled)
            {
                channels_mask |= (1 << i);
            }
        }

        sl_pwm_led_init(&channels[i]);
    }
}

void led_channel_endpoints_enable(void)
{
    for(size_t i = 0; i < APP_EP_COUNT; i++)
    {
        size_t mask = 1 << i;

        emberAfEndpointEnableDisable(i + 1, (channels_mask & mask) != 0);
    }
}

void led_channel_level_set(LedChannel ch, uint8_t level)
{
    uint8_t pwm_level = cie_254_254[level];

    sl_pwm_led_set_color(&channels[ch], pwm_level);
}

void led_channel_zcl_level_set(LedChannel ch, uint8_t zcl_level)
{
    uint8_t pwm_level = cie_254_254[zcl_level];

    if (pwm_level == channels[ch].level)
    {
        return;
    }

#if defined(DEBUG)
    DBG_LOG("ZCL %d PWM %d level set", zcl_level, pwm_level);
#endif

    sl_pwm_led_set_color(&channels[ch], pwm_level);
}
