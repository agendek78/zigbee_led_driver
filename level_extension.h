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

#ifndef LEVEL_EXTENSION_H_
#define LEVEL_EXTENSION_H_

#include "app/framework/include/af.h"
#include "sl_service_function.h"

#include <stdint.h>
#include <stdbool.h>

#define EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL   (1)
#define EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL   (254)

void level_extension_init(void);

uint32_t level_extension_handle_cmd(sl_service_opcode_t opcode,
                                    sl_service_function_context_t *context);

void level_extension_do_transition(uint8_t ep_id, uint8_t target_level,
                                   uint16_t transition_time, bool with_attribute_update,
                                   bool with_onoff);

#endif /* LEVEL_EXTENSION_H_ */
