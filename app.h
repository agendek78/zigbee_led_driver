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

#ifndef APP_H_
#define APP_H_

#include <stddef.h>

#define APP_EP_COUNT                4

#define EMBER_AF_IMAGE_TYPE_ID              0x1000
#define EMBER_AF_CUSTOM_FIRMWARE_VERSION    0x01030000

#define ARRAY_SIZE(arr) (size_t)(sizeof(arr) / sizeof((arr)[0]))

void led_drv_fb_activate(void);

void led_drv_reboot_set(void);
uint8_t led_drv_active_fb_ep_get(void);
void led_drv_exit_pairing(void);

#endif /* APP_H_ */
