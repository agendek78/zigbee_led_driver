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

#include "zcl_extension.h"
#include "sl_service_function.h"
#include "af.h"
#include "app.h"
#include "on_off_extension.h"
#include "identify_extension.h"

const sl_service_function_entry_t zcl_extension_items[] =
{
    { SL_SERVICE_FUNCTION_TYPE_ZCL_COMMAND, ZCL_IDENTIFY_CLUSTER_ID, (NOT_MFG_SPECIFIC | (SL_CLUSTER_SERVICE_SIDE_SERVER << 16)), identify_extension_handle_cmd },
    { SL_SERVICE_FUNCTION_TYPE_ZCL_COMMAND, ZCL_ON_OFF_CLUSTER_ID, (NOT_MFG_SPECIFIC | (SL_CLUSTER_SERVICE_SIDE_SERVER << 16)), on_off_extension_handle_cmd },
};

static sl_service_function_block_t zcl_extension_block[] =
{
    ARRAY_SIZE(zcl_extension_items),
    (sl_service_function_entry_t *)zcl_extension_items,
    NULL
};

void zcl_extension_init(void)
{
    sl_service_function_register_block(zcl_extension_block);

    on_off_extension_init();
}
