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

#include "identify_extension.h"
#include "app.h"
#include "dbg_log.h"

uint32_t identify_extension_handle_cmd(sl_service_opcode_t opcode,
                                     sl_service_function_context_t *context)
{
    EmberAfClusterCommand* cmd = (EmberAfClusterCommand *)context->data;
    uint8_t ep_id = emberAfCurrentEndpoint();

    switch(cmd->commandId)
    {
        case ZCL_IDENTIFY_QUERY_COMMAND_ID:
        {
            DBG_LOG("IDENTIFY_QUERY(%d)", ep_id);
            if (ep_id == led_drv_active_fb_ep_get())
            {
                led_drv_exit_pairing();
            }
            break;
        }
    }

    return EMBER_ZCL_STATUS_UNSUP_COMMAND;
}
