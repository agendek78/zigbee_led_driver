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

#include "on_off_extension.h"
#include "led_channel.h"
#include "app.h"
#include "dbg_log.h"

#include "zigbee_app_framework_event.h"

#define ON_OFF_ACCEPT_ONLY_WHEN_ON      0x01

typedef struct
{
    OnOffState          state[APP_EP_COUNT];
    sl_zigbee_event_t   event[APP_EP_COUNT];
    bool                initialized;
} OnOffCtx;

static OnOffCtx ctx;

static uint16_t decrease_time(uint16_t curr_value)
{
    if (curr_value >= 10)
    {
        return curr_value - 10;
    }

    return 0;
}

static void on_off_extension_state_update(uint8_t endpoint, OnOffState new_state)
{
    uint8_t ch = endpoint -1;
    if (ctx.state[ch] != new_state)
    {
        static const char* state_txt[] = { "OFF", "ON", "TIMED_ON", "TIMED_OFF"};
        DBG_LOG("Channel %d state change: [%s] -> [%s]", ch,
                state_txt[ctx.state[ch]],
                state_txt[new_state]);

        ctx.state[ch] = new_state;
    }
}

static void on_off_extension_timed_state_update(uint16_t ep_id, bool dec_time)
{
    uint16_t on_time;
    uint16_t off_wait_time;
    EmberAfStatus status = emberAfReadAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_ON_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t *)&on_time,
                                  sizeof(on_time),
                                  NULL);
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read ON_TIME attribute!");
        return;
    }

    status = emberAfReadAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t *)&off_wait_time,
                                  sizeof(off_wait_time),
                                  NULL);
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read OFF_WAIT_TIME attribute!");
        return;
    }

    uint16_t next_timeout = 1000;

    switch(ctx.state[ep_id - 1])
    {
        case OnOffState_On:
        {
            if (on_time != 0)
            {
                on_off_extension_state_update(ep_id, OnOffState_TimedOn);
            }
            else if (off_wait_time != 0)
            {
                on_off_extension_state_update(ep_id, OnOffState_DelayedOff);
            }
            break;
        }
        default:
        case OnOffState_Off:
        {
            if (on_time != 0)
            {
                emberAfOnOffClusterSetValueCallback(ep_id, ZCL_ON_COMMAND_ID, false);
                on_off_extension_state_update(ep_id, OnOffState_TimedOn);
            }
            else if (off_wait_time != 0)
            {
                on_off_extension_state_update(ep_id, OnOffState_DelayedOff);
            }
            break;
        }
        case OnOffState_TimedOn:
        {
            if (dec_time == true)
            {
                on_time = decrease_time(on_time);

                emberAfWriteAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_ON_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t*) &on_time, ZCL_INT16U_ATTRIBUTE_TYPE);
            }

            if (on_time == 0)
            {
                /* this light level move step is not triggered from
                 * ZCL command, so we have to set fake ZCL command
                 * because moveToLevel handler is using it
                 */
                EmberAfClusterCommand* saved_ptr = emberAfCurrentCommand();
                EmberApsFrame fake_aps = { .destinationEndpoint = ep_id };
                EmberAfClusterCommand fake_cmd = { .apsFrame = &fake_aps };

                emberAfCurrentCommand() = &fake_cmd;
                emberAfOnOffClusterSetValueCallback(ep_id, ZCL_OFF_COMMAND_ID, false);
                emberAfCurrentCommand() = saved_ptr;

                if (off_wait_time != 0)
                {
                    on_off_extension_state_update(ep_id, OnOffState_DelayedOff);
                }
                else
                {
                    on_off_extension_state_update(ep_id, OnOffState_Off);
                    next_timeout = 0;
                }
            }
            else if (on_time < 10)
            {
                next_timeout = on_time * 100;
            }
            break;
        }
        case OnOffState_DelayedOff:
        {
            if (dec_time == true)
            {
                off_wait_time = decrease_time(off_wait_time);

                emberAfWriteAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t*) &off_wait_time, ZCL_INT16U_ATTRIBUTE_TYPE);
            }

            if (off_wait_time == 0)
            {
                on_off_extension_state_update(ep_id, OnOffState_Off);
                next_timeout = 0;
            }
            else if (off_wait_time < 10)
            {
                next_timeout = off_wait_time * 100;
            }
            break;
        }
    }

    if (next_timeout != 0)
    {
        sl_zigbee_endpoint_event_set_delay_ms(ctx.event, ep_id, next_timeout);
    }
}

static void on_off_extension_channel_event_cb(uint8_t ep_id)
{
    if (ep_id > APP_EP_COUNT)
    {
        DBG_LOG("Invalid endpoint ID: %d", ep_id);
        return;
    }

    on_off_extension_timed_state_update(ep_id, true);
}

void emberAfPluginLevelControlTransitionFinishedCallback(uint8_t endpoint)
{
    DBG_LOG("Transition complete for ep %d", endpoint);

    uint8_t ch = endpoint - 1;

    if (ctx.state[ch] == OnOffState_Off ||
        ctx.state[ch] == OnOffState_DelayedOff)
    {
        led_channel_zcl_level_set(endpoint - 1, 0);
    }
    else
    {
        uint8_t current_level = 0;
        EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                            ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                            ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                            &current_level, sizeof(current_level));
        if (status != EMBER_ZCL_STATUS_SUCCESS)
        {
            DBG_LOG("Error reading current level!");
            return;
        }

        led_channel_zcl_level_set(endpoint - 1, current_level);
    }
}

bool on_off_extension_handle_off(uint8_t ep_id, bool currentValue)
{
    OnOffState ch_state = ctx.state[ep_id - 1];

    switch(ch_state)
    {
        default:
        case OnOffState_On:
        {
            on_off_extension_state_update(ep_id, OnOffState_Off);
            emberAfOnOffClusterSetValueCallback(ep_id, ZCL_OFF_COMMAND_ID, false);
            break;
        }
        case OnOffState_Off:
        case OnOffState_DelayedOff:
        {
            break;
        }
        case OnOffState_TimedOn:
        {
            uint16_t time = 0;
            emberAfWriteAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_ON_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t*) &time, ZCL_INT16U_ATTRIBUTE_TYPE);

            sl_zigbee_event_set_inactive(&ctx.event[ep_id - 1]);

            on_off_extension_timed_state_update(ep_id, false);
            break;
        }
    }

    return true;
}

bool on_off_extension_handle_on(uint8_t ep_id, bool currentValue)
{
    OnOffState ch_state = ctx.state[ep_id - 1];
    switch(ch_state)
    {
        default:
        case OnOffState_On:
        {
            break;
        }
        case OnOffState_Off:
        {
            emberAfOnOffClusterSetValueCallback(ep_id, ZCL_ON_COMMAND_ID, false);
            on_off_extension_state_update(ep_id, OnOffState_On);
            break;
        }
        case OnOffState_TimedOn:
        case OnOffState_DelayedOff:
        {
            uint16_t time = 0;

            sl_zigbee_event_set_inactive(&ctx.event[ep_id - 1]);

            emberAfWriteAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_ON_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t*) &time, ZCL_INT16U_ATTRIBUTE_TYPE);

            emberAfWriteAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t*) &time, ZCL_INT16U_ATTRIBUTE_TYPE);

            if (ch_state == OnOffState_DelayedOff)
            {
                emberAfOnOffClusterSetValueCallback(ep_id, ZCL_ON_COMMAND_ID, false);
            }
            on_off_extension_state_update(ep_id, OnOffState_On);
            break;
        }
    }

    return true;
}

bool on_off_extension_handle_toggle(uint8_t ep_id, bool currentValue)
{
    OnOffState ch_state = ctx.state[ep_id - 1];

    switch(ch_state)
    {
        case OnOffState_On:
        case OnOffState_TimedOn:
        {
            on_off_extension_handle_off(ep_id, currentValue);
            break;
        }
        case OnOffState_Off:
        case OnOffState_DelayedOff:
        {
            on_off_extension_handle_on(ep_id, currentValue);
            break;
        }
    }

    return true;
}

bool on_off_extension_handle_on_with_timed_off(bool exec_when_on,
                                               uint16_t on_time,
                                               uint16_t off_wait_time)
{
    uint8_t ep_id = emberAfCurrentEndpoint();

    DBG_LOG("ON_WITH_TIMED_OFF(%d): exec_when_on = %d, on_time = %d, off_wait_time = %d",
            ep_id, exec_when_on, on_time, off_wait_time);

    if (exec_when_on == true && ctx.state[ep_id - 1] != OnOffState_On)
    {
        return true;
    }

    EmberAfStatus status;

    if (ctx.state[ep_id - 1] != OnOffState_DelayedOff)
    {
        status = emberAfWriteAttribute(ep_id,
                                       ZCL_ON_OFF_CLUSTER_ID,
                                       ZCL_ON_TIME_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       (uint8_t*) &on_time, ZCL_INT16U_ATTRIBUTE_TYPE);
        if (status != EMBER_ZCL_STATUS_SUCCESS)
        {
            DBG_LOG("Can't write ON_TIME attribute!");
            return false;
        }
    }

    status = emberAfWriteAttribute(ep_id,
                                   ZCL_ON_OFF_CLUSTER_ID,
                                   ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID,
                                   CLUSTER_MASK_SERVER,
                                   (uint8_t*) &off_wait_time, ZCL_INT16U_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't write OFF_WAIT_TIME attribute!");
        return false;
    }

    on_off_extension_timed_state_update(ep_id, false);

    return true;
}

void on_off_attribute_written(uint8_t endpoint, EmberAfAttributeId attributeId,
                             uint8_t size, uint8_t *value)
{
    if (ctx.initialized == false)
    {
        return;
    }

    if (attributeId == ZCL_ON_OFF_ATTRIBUTE_ID)
    {
        switch(ctx.state[endpoint - 1])
        {
            case OnOffState_On:
            case OnOffState_TimedOn:
            {
                if (*value == false)
                {
                    on_off_extension_state_update(endpoint, OnOffState_Off);
                }
                break;
            }
            case OnOffState_Off:
            case OnOffState_DelayedOff:
            {
                if (*value == true)
                {
                    on_off_extension_state_update(endpoint, OnOffState_On);
                }
                break;
            }
        }
    }
}

void on_off_extension_init(void)
{
    for(int i = 0; i < APP_EP_COUNT; i++)
    {
        uint8_t currentValue = 0;
        emberAfReadAttribute(i + 1,
                             ZCL_ON_OFF_CLUSTER_ID,
                             ZCL_ON_OFF_ATTRIBUTE_ID,
                             CLUSTER_MASK_SERVER,
                             (uint8_t*) &currentValue, sizeof(currentValue),
                             NULL);

        ctx.state[i] = (currentValue != 0) ? OnOffState_On : OnOffState_Off;
        sl_zigbee_endpoint_event_init(&ctx.event[i], on_off_extension_channel_event_cb, i + 1);
    }

    ctx.initialized = true;

    DBG_LOG("ON/OFF extension initialized!");
}

uint32_t on_off_extension_handle_cmd(sl_service_opcode_t opcode,
                                     sl_service_function_context_t *context)
{
    bool wasHandled = false;

    EmberAfClusterCommand* cmd = (EmberAfClusterCommand *)context->data;
    uint8_t ep_id = emberAfCurrentEndpoint();
    bool currentValue;
    EmberAfStatus status = emberAfReadAttribute(ep_id,
                                  ZCL_ON_OFF_CLUSTER_ID,
                                  ZCL_ON_OFF_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t *)&currentValue,
                                  sizeof(currentValue),
                                  NULL); // data type

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read ON_OFF attribute! Status %x, ep %d", status, ep_id);
        return EMBER_ZCL_STATUS_FAILURE;
    }

    switch(cmd->commandId)
    {
        case ZCL_OFF_COMMAND_ID:
        {
            DBG_LOG("ON(%d)", ep_id);
            wasHandled = on_off_extension_handle_off(ep_id, currentValue);
            break;
        }
        case ZCL_ON_COMMAND_ID:
        {
            DBG_LOG("OFF(%d)", ep_id);
            wasHandled = on_off_extension_handle_on(ep_id, currentValue);
            break;
        }
        case ZCL_TOGGLE_COMMAND_ID:
        {
            DBG_LOG("TOGGLE(%d)", ep_id);
            wasHandled = on_off_extension_handle_toggle(ep_id, currentValue);
            break;
        }
        case ZCL_ON_WITH_TIMED_OFF_COMMAND_ID:
        {
            uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

            if (cmd->payloadStartIndex < cmd->bufLen)
            {
                wasHandled = on_off_extension_handle_on_with_timed_off((payload[0] & ON_OFF_ACCEPT_ONLY_WHEN_ON) != 0,
                                                                       *(uint16_t*)&payload[1],
                                                                       *(uint16_t*)&payload[3]);
            }
            break;
        }
    }

    if (wasHandled == true)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    }

    return wasHandled == true ? EMBER_ZCL_STATUS_SUCCESS :
        EMBER_ZCL_STATUS_UNSUP_COMMAND;
}

OnOffState on_off_extension_state_get(uint8_t endpoint)
{
    return ctx.state[endpoint - 1];
}
