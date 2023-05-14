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

#include "level_extension.h"
#include "af.h"
#include "app.h"
#include "sl_custom_token_header.h"
#include "level-control-config.h"

#include "dbg_log.h"

#include <stdint.h>

static uint8_t current_level_saved[APP_EP_COUNT];
static uint8_t current_level_local[APP_EP_COUNT];

EmberAfStatus emberAfExternalAttributeWriteCallback(int8u endpoint,
                                                         EmberAfClusterId clusterId,
                                                         EmberAfAttributeMetadata *attributeMetadata,
                                                         int16u manufacturerCode,
                                                         int8u *buffer)
{
    if (clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID &&
        attributeMetadata->attributeId == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID)
    {
        current_level_local[endpoint - 1] = *(uint8_t*)buffer;

        return EMBER_ZCL_STATUS_SUCCESS;
    }

    return EMBER_ZCL_STATUS_FAILURE;
}

EmberAfStatus emberAfExternalAttributeReadCallback(int8u endpoint,
                                                        EmberAfClusterId clusterId,
                                                        EmberAfAttributeMetadata *attributeMetadata,
                                                        int16u manufacturerCode,
                                                        int8u *buffer,
                                                        int16u maxReadLength)
{
    if (clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID &&
        attributeMetadata->attributeId == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID)
    {
        uint8_t *level = (uint8_t*)buffer;

        *level = current_level_local[endpoint - 1];

        if (*level < EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL ||
            *level > EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL)
        {
            halCommonGetIndexedToken(level, TOKEN_CURRENT_LEVEL, endpoint - 1);
            current_level_saved[endpoint - 1] = *level;
        }
        else
        {
            return EMBER_ZCL_STATUS_SUCCESS;
        }

        if (*level < EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL ||
            *level > EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL)
        {
            *level = CURRENT_LEVEL_DEFAULT;
            current_level_local[endpoint - 1] = CURRENT_LEVEL_DEFAULT;
        }

        return EMBER_ZCL_STATUS_SUCCESS;
    }

    return EMBER_ZCL_STATUS_FAILURE;
}

void level_extension_current_level_save(uint8_t endpoint)
{
    if (current_level_saved[endpoint - 1] != current_level_local[endpoint - 1])
    {
        DBG_LOG("Saving current level %d for ep %d", current_level_local[endpoint - 1], endpoint);
        halCommonSetIndexedToken(TOKEN_CURRENT_LEVEL, endpoint - 1, &current_level_local[endpoint - 1]);
        current_level_saved[endpoint - 1] = current_level_local[endpoint - 1];
    }
}
