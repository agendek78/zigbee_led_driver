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

#ifndef LED_CHANNEL_H_
#define LED_CHANNEL_H_

#include <stdint.h>

typedef enum
{
    LedChannel_CH1,
    LedChannel_CH2,
    LedChannel_CH3,
    LedChannel_CH4,
    LedChannel_AUX,

    LedChannel_MAX
} LedChannel;


void led_channel_init(void);

void led_channel_level_set(LedChannel ch, uint8_t level);

void led_channel_zcl_level_set(LedChannel ch, uint8_t zcl_level);

void led_channel_endpoints_enable(void);

#endif /* LED_CHANNEL_H_ */
