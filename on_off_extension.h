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

#ifndef ON_OFF_EXTENSION_H_
#define ON_OFF_EXTENSION_H_

#include "app/framework/include/af.h"
#include "sl_service_function.h"

typedef enum
{
    OnOffState_Off,
    OnOffState_On,
    OnOffState_TimedOn,
    OnOffState_DelayedOff

} OnOffState;


void on_off_extension_init(void);

uint32_t on_off_extension_handle_cmd(sl_service_opcode_t opcode,
                                     sl_service_function_context_t *context);

void on_off_attribute_written(uint8_t endpoint, EmberAfAttributeId attributeId,
                             uint8_t size, uint8_t *value);

OnOffState on_off_extension_state_get(uint8_t endpoint);

#endif /* ON_OFF_EXTENSION_H_ */
