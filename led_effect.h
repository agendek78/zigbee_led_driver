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

#ifndef LED_EFFECT_H_
#define LED_EFFECT_H_

#include <stddef.h>
#include <stdbool.h>
#include "led_channel.h"

#define LED_EFFECT_INFINITE     0

typedef enum
{
    LedEffect_None,
    LedEffect_JoiningNetwork,
    LedEffect_NetworkLeft,
    LedEffect_Reboot,
    LedEffect_Identify,
    LedEffect_DeviceJoined,
} LedEffect;

void led_effect_init(void);

/**
 * @brief
 *  Starts selected LED effect requested number of iterations.
 *
 * @param ch - PWM channel on which to run effect
 * @param effect - LED effect to run
 * @param count - number of iterations (if 0 repeat infinite)
 */
void led_effect_run(LedChannel ch, LedEffect effect, size_t count);

#endif /* LED_EFFECT_H_ */
