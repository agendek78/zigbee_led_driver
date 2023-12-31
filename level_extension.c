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
#include "zigbee_app_framework_event.h"
#include "led_channel.h"
#include "on_off_extension.h"

#include "dbg_log.h"

#include <stdint.h>
#include <stdbool.h>

#define LEVEL_STEP_PER_SEC  (20)

typedef struct
{
  sl_zigbee_event_t   transition_event;
  uint8_t             saved_level;
  uint8_t             current_level;
  uint8_t             target_level;
  int16_t             transition_step       : 10;
  bool                with_on_off           : 1;    /* true when command version is WITH_ON_OFF */
  bool                is_direction_up       : 1;    /* true when transition is UP */
  bool                trigerred_by_onoff    : 1;    /* true when triggered by OnOff cluster */
  bool                disable_light_effect  : 1;    /* do only transition without updating PWM output */
  bool                init                  : 1;    /* true on first level_extension_on_level_updated() call */
  bool                with_attribute_update : 1;    /* true will update level attributes when doing transition */

} TransitionCtx;

typedef enum
{
  LevelCmdRunMode_SKIP,               /* do not execute command */
  LevelCmdRunMode_EXECUTE,            /* execute normally */
  LevelCmdRunMode_EXECUTE_NO_EFFECT   /* do only level transition, without PWM update */

} LevelCmdRunMode;

static TransitionCtx tr_ctx[APP_EP_COUNT];

EmberAfStatus emberAfExternalAttributeWriteCallback(int8u endpoint,
                                                         EmberAfClusterId clusterId,
                                                         EmberAfAttributeMetadata *attributeMetadata,
                                                         int16u manufacturerCode,
                                                         int8u *buffer)
{
    if (clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID &&
        attributeMetadata->attributeId == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID)
    {
      tr_ctx[endpoint - 1].current_level = *(uint8_t*)buffer;

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
        TransitionCtx* ctx = &tr_ctx[endpoint - 1];
        uint8_t *level = (uint8_t*)buffer;

        *level = ctx->current_level;

        if (*level < EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL ||
            *level > EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL)
        {
            halCommonGetIndexedToken(level, TOKEN_CURRENT_LEVEL, endpoint - 1);

            if (*level < EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL ||
                *level > EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL)
            {
                *level = CURRENT_LEVEL_DEFAULT;
            }

            ctx->current_level = *level;
            ctx->saved_level = *level;
        }

        return EMBER_ZCL_STATUS_SUCCESS;
    }

    return EMBER_ZCL_STATUS_FAILURE;
}

void level_extension_current_level_save(uint8_t endpoint)
{
  TransitionCtx* ctx = &tr_ctx[endpoint - 1];
  if (ctx->saved_level != ctx->current_level)
  {
      DBG_LOG("Saving current level %d for ep %d", ctx->current_level, endpoint);
      halCommonSetIndexedToken(TOKEN_CURRENT_LEVEL, endpoint - 1, &ctx->current_level);
      ctx->saved_level = ctx->current_level;
  }
}

static void level_extension_on_level_updated(uint8_t ep_id, uint8_t level_value, bool done)
{
  TransitionCtx* ctx = &tr_ctx[ep_id - 1];

  if (ctx->trigerred_by_onoff)
  {
    if (done && ctx->is_direction_up == false)
    {
      level_value = 0;
    }
  }
  else
  {
    if (ctx->is_direction_up)
    {
      if (ctx->init && ctx->with_on_off)
      {
        emberAfOnOffClusterSetValueCallback(ep_id,
                                            ZCL_ON_COMMAND_ID,
                                            true);
      }
    }
    else
    {
      if (done && ctx->with_on_off)
      {
        level_value = 0;
        emberAfOnOffClusterSetValueCallback(ep_id,
                                            ZCL_OFF_COMMAND_ID,
                                            true);
      }
    }
  }

  ctx->init = false;
  led_channel_zcl_level_set(ep_id - 1, level_value);
}

void level_extension_do_transition(uint8_t ep_id, uint8_t target_level,
                                   uint16_t transition_time, bool with_attribute_update,
                                   bool with_onoff)
{
  TransitionCtx* ctx = &tr_ctx[ep_id - 1];

  /* disable previous transition if in progress */
  sl_zigbee_event_set_inactive(&ctx->transition_event);

  ctx->target_level = target_level;
  ctx->with_attribute_update = with_attribute_update;
  ctx->with_on_off = with_onoff;
  ctx->init = true;

  if (transition_time == 0xFFFF ||
      transition_time == 0x0000)
  {
    ctx->transition_step = (target_level - ctx->current_level);
  }
  else
  {
    float tmp = (transition_time * 100.0f * LEVEL_STEP_PER_SEC) / 1000.0f;
    ctx->transition_step = (int16_t)((target_level - ctx->current_level) / tmp + 0.5f);

    if (ctx->transition_step == 0)
    {
      ctx->transition_step = (target_level - ctx->current_level);
    }
  }

  DBG_LOG("DO_TRANSITION: %d - > %d, step %d", ctx->current_level, target_level, ctx->transition_step);
  sl_zigbee_event_set_active(&ctx->transition_event);
}

static LevelCmdRunMode level_extension_can_execute_cmd(uint8_t ep_id, uint8_t options, bool with_on_off)
{
  OnOffState state = on_off_extension_state_get(ep_id);

  if ((state == OnOffState_Off) || (state == OnOffState_DelayedOff))
  {
    if (with_on_off == false)
    {
      if ((options & EMBER_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF) != 0x00)
      {
        DBG_LOG("Execute command without light effect!");
        return LevelCmdRunMode_EXECUTE_NO_EFFECT;
      }

      DBG_LOG("Skipping command due to light is OFF!");
      return LevelCmdRunMode_SKIP;
    }
  }

  return LevelCmdRunMode_EXECUTE;
}

static void level_extension_channel_event_cb(uint8_t ep_id)
{
  if (ep_id > APP_EP_COUNT || ep_id == 0)
  {
      DBG_LOG("Invalid endpoint ID: %d", ep_id);
      return;
  }

  TransitionCtx* ctx = &tr_ctx[ep_id - 1];

  int16_t new_level = ctx->current_level + ctx->transition_step;

  if (ctx->transition_step < 0)
  {
    /* move down*/
    if (new_level <= ctx->target_level)
    {
      ctx->current_level = ctx->target_level;
    }
    else
    {
      ctx->current_level = (uint8_t)new_level;
    }
  }
  else
  {
    if (new_level >= ctx->target_level)
    {
      ctx->current_level = ctx->target_level;
    }
    else
    {
      ctx->current_level = (uint8_t)new_level;
    }
  }

  bool done = ctx->current_level == ctx->target_level;
  level_extension_on_level_updated(ep_id, ctx->current_level,
                                   done);

  if (ctx->with_attribute_update)
  {
    EmberAfStatus status = emberAfWriteServerAttribute (ep_id,
                                          ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                          ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                          &ctx->current_level,
                                          ZCL_INT8U_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
      DBG_LOG("ERR: unable to set current level %x", status);
    }

#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE)
    do {

      uint16_t time_remaining = 0;
      if (!done)
      {
         status = emberAfReadAttribute(ep_id,
                                        ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                        ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                                        CLUSTER_MASK_SERVER,
                                        (uint8_t *)&time_remaining,
                                        sizeof(time_remaining),
                                        NULL); // data type

        if (status != EMBER_ZCL_STATUS_SUCCESS)
        {
            DBG_LOG("Can't read REMAINING TIME attribute! Status %x, ep %d", status, ep_id);
            break;
        }

        int32_t time_ms = time_remaining * 100 - (1000 / LEVEL_STEP_PER_SEC);
        if (time_ms < 0)
        {
          time_remaining = 0;
        }
        else
        {
          time_remaining = time_ms / 100;
        }
      }

      status = emberAfWriteServerAttribute (ep_id,
                                            ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                            ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                                            (uint8_t*) &time_remaining,
                                            ZCL_INT16U_ATTRIBUTE_TYPE);
      if (status != EMBER_ZCL_STATUS_SUCCESS)
      {
        DBG_LOG("ERR: unable to set REMAINING TIME %x", status);
      }
    } while(0);
#endif
  }

  if (!done)
  {
    sl_zigbee_event_set_delay_ms(&ctx->transition_event, (1000 / LEVEL_STEP_PER_SEC));
  }
  else
  {
    if (!ctx->with_attribute_update)
    {
      //restore level
      ctx->current_level = ctx->saved_level;
    }
    else
    {
      level_extension_current_level_save(ep_id);
    }
  }
}

void level_extension_statup_level_setup(uint8_t ep_id, bool with_on_off)
{
  if (with_on_off)
  {
    OnOffState s = on_off_extension_state_get(ep_id);
    if (s == OnOffState_Off || s == OnOffState_DelayedOff)
    {
      TransitionCtx* ctx = &tr_ctx[ep_id - 1];
      ctx->current_level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
    }
  }
}

/**
 * @brief
 *  Called by OnOff cluster plugin
 *
 * @param ep_id
 * @param onoff_state
 */
void emberAfOnOffClusterLevelControlEffectCallback(int8u ep_id,
                                                   bool onoff_state)
{
  TransitionCtx* ctx = &tr_ctx[ep_id - 1];
  uint16_t transition_time = 0xFFFF;
  EmberAfStatus status;

  // Read the OnOffTransitionTime attribute.
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
  status = emberAfReadServerAttribute(ep_id,
                                      ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                      ZCL_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                      (uint8_t *)&transition_time,
                                      sizeof(transition_time));
  if (status != EMBER_ZCL_STATUS_SUCCESS)
  {
    DBG_LOG("ERR: reading current level %x. Using 0xFFFF", status);
  }
#endif

  // Read the OnLevel attribute.
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_LEVEL_ATTRIBUTE
  status = emberAfReadServerAttribute(ep_id,
                                      ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                      ZCL_ON_LEVEL_ATTRIBUTE_ID,
                                      (uint8_t *)&target_level, // OnLevel value
                                      sizeof(target_level));
  if (status != EMBER_ZCL_STATUS_SUCCESS)
  {
    DBG_LOG("ERR: reading current level %x", status);
    target_level = ctx->saved_level;
  }
  else
  {
    if (target_level == 0xFF)
    {
      // OnLevel has undefined value; fall back to CurrentLevel.
      target_level = ctx->saved_level;
    }
  }
#endif

  uint8_t target_level = ctx->saved_level;

  ctx->with_on_off = false;
  ctx->trigerred_by_onoff = true;
  ctx->is_direction_up = (onoff_state != 0);

  DBG_LOG("%s: ep %d, level %d", __FUNCTION__, ep_id, target_level);

  if (onoff_state)
  {
    // If newValue is ZCL_ON_COMMAND_ID...
    // "Set CurrentLevel to minimum level allowed for the device."
    ctx->current_level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;

    // "Move CurrentLevel to OnLevel, or to the stored level if OnLevel is not
    // defined, over the time period OnOffTransitionTime."
    level_extension_do_transition(ep_id, target_level, transition_time, false, true);
  }
  else
  {
    // ...else if newValue is ZCL_OFF_COMMAND_ID...
    // "Move CurrentLevel to the minimum level allowed for the device over the
    // time period OnOffTransitionTime."
    level_extension_do_transition(ep_id, EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL,
                                  transition_time, false, true);
  }
}

/**
 * @brief
 *  MOVE_TO_LEVEL ZCL command handler
 *
 * @param ep_id
 * @param level
 * @param transition_time
 * @param options
 * @param with_on_off
 * @return
 */
bool level_extension_handle_move_to_level(uint8_t ep_id, uint8_t level, uint16_t transition_time, uint8_t options, bool with_on_off)
{
  LevelCmdRunMode exec = level_extension_can_execute_cmd(ep_id, options, with_on_off);
  if (exec == LevelCmdRunMode_SKIP)
  {
    return true;
  }

  if (transition_time == 0xFFFF)
  {
#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE)
    EmberAfStatus status = emberAfReadAttribute(ep_id,
                                  ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                  ZCL_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t *)&transition_time,
                                  sizeof(transition_time),
                                  NULL); // data type
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read ON_OFF_TRANSITION_T attribute! Status %x, ep %d", status, ep_id);
        transition_time = 0;
    }
#else
    transition_time = 0;
#endif
  }

  DBG_LOG("MOVE_TO_LEVEL%s(%d, %d) in %d [ms]", with_on_off ? "_WITH_ONOFF" : "",
          ep_id, level, transition_time * 100);

  TransitionCtx* ctx = &tr_ctx[ep_id - 1];

  level_extension_statup_level_setup(ep_id, with_on_off);

  ctx->trigerred_by_onoff = false;
  ctx->is_direction_up = level > ctx->current_level;
  ctx->disable_light_effect = (exec == LevelCmdRunMode_EXECUTE_NO_EFFECT);
  level_extension_do_transition(ep_id, level, transition_time, true, with_on_off);

  return true;
}

/**
 * @brief
 *  MOVE_LEVEL ZCL command handler
 *
 * @param ep_id
 * @param mode
 * @param rate
 * @param options
 * @param with_on_off
 * @return
 */
bool level_extension_handle_move_level(uint8_t ep_id, EmberAfMoveMode mode, uint8_t rate, uint8_t options, bool with_on_off)
{
  LevelCmdRunMode exec = level_extension_can_execute_cmd(ep_id, options, with_on_off);
  if (exec == LevelCmdRunMode_SKIP)
  {
    return true;
  }

  TransitionCtx* ctx = &tr_ctx[ep_id - 1];
  uint8_t move_diff;
  uint8_t level;

  if (mode == EMBER_ZCL_MOVE_MODE_UP)
  {
    move_diff = EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL - ctx->current_level;
    level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL;
  }
  else
  {
    move_diff = ctx->current_level - EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
    level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
  }

  if (move_diff == 0)
  {
    return true;
  }

  uint16_t transition_time = 0;

  if (rate == 0xFF || rate == 0x00)
  {
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_DEFAULT_MOVE_RATE_ATTRIBUTE
    EmberAfStatus status = emberAfReadServerAttribute(ep_id,
                                        ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                        ZCL_DEFAULT_MOVE_RATE_ATTRIBUTE_ID,
                                        (uint8_t *)&rate,
                                        sizeof(rate));
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
      DBG_LOG("ERR: reading default move rate %x", status);
      transition_time = 1;
    }
    else
    {
      if (rate == 0xFF || rate == 0x00)
      {
        transition_time = 1;
      }
    }
#else
    transition_time = 1;
#endif
  }
  else
  {
    float time = (float)(move_diff * 10) / (float)(rate);

    transition_time = (uint16_t)(time + 0.5f);
    if (transition_time == 0)
    {
      transition_time = 1;
    }
  }

  DBG_LOG("MOVE_LEVEL%s(%d, %s) in %d [ms]", with_on_off ? "_WITH_ONOFF" : "",
          ep_id,
          mode == EMBER_ZCL_MOVE_MODE_UP ? "UP" : "DOWN",
          transition_time * 100);

  level_extension_statup_level_setup(ep_id, with_on_off);

  ctx->trigerred_by_onoff = false;
  ctx->is_direction_up = level > ctx->current_level;
  ctx->disable_light_effect = (exec == LevelCmdRunMode_EXECUTE_NO_EFFECT);
  level_extension_do_transition(ep_id, level, transition_time, true, with_on_off);

  return true;
}

/**
 * @brief
 *  STOP ZCL command handler
 *
 * @param ep_id
 * @param options
 * @param with_on_off
 * @return
 */
bool level_extension_handle_stop(uint8_t ep_id, uint8_t options, bool with_on_off)
{
  LevelCmdRunMode exec = level_extension_can_execute_cmd(ep_id, options, with_on_off);
  if (exec == LevelCmdRunMode_SKIP)
  {
    return true;
  }

  TransitionCtx* ctx = &tr_ctx[ep_id - 1];

  sl_zigbee_event_set_inactive(&ctx->transition_event);

  DBG_LOG("STOP%s(%d)", with_on_off ? "_WITH_ONOFF" : "", ep_id);

  if (ctx->with_attribute_update)
  {
#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE)
      uint16_t time_remaining = 0;
      EmberAfStatus status = emberAfWriteServerAttribute (ep_id,
                                            ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                            ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                                            (uint8_t*) &time_remaining,
                                            ZCL_INT16U_ATTRIBUTE_TYPE);
      if (status != EMBER_ZCL_STATUS_SUCCESS)
      {
        DBG_LOG("ERR: unable to reset REMAINING TIME %x", status);
      }
#endif

    level_extension_current_level_save(ep_id);
  }
  else
  {
    //restore level
    ctx->current_level = ctx->saved_level;
  }

  return true;
}

/**
 * @brief
 *  STEP_LEVEL ZCL command handler
 * @param ep_id
 * @param mode
 * @param size
 * @param transition_time
 * @param options
 * @param with_on_off
 * @return
 */
bool level_extension_handle_step_level(uint8_t ep_id, EmberAfStepMode mode, uint8_t size,
                                       uint16_t transition_time, uint8_t options, bool with_on_off)
{
  LevelCmdRunMode exec = level_extension_can_execute_cmd(ep_id, options, with_on_off);
  if (exec == LevelCmdRunMode_SKIP)
  {
    return true;
  }

  TransitionCtx* ctx = &tr_ctx[ep_id - 1];
  int16_t new_level = 0;

  if (mode == EMBER_ZCL_STEP_MODE_UP)
  {
    new_level = ctx->saved_level + size;
  }
  else
  {
    new_level = ctx->saved_level - size;
  }

  if (new_level > EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL)
  {
    new_level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL;
  }
  else if (new_level < EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL)
  {
    new_level = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
  }

  if (new_level == ctx->saved_level)
  {
    /* nothing to do*/
    return true;
  }

  DBG_LOG("STEP_LEVEL%s(%d, %s) in %d [ms]", with_on_off ? "_WITH_ONOFF" : "", ep_id,
          mode == EMBER_ZCL_STEP_MODE_UP ? "UP" : "DOWN",
          transition_time * 100);

  level_extension_statup_level_setup(ep_id, with_on_off);

  ctx->trigerred_by_onoff = false;
  ctx->is_direction_up = new_level > ctx->current_level;
  ctx->disable_light_effect = (exec == LevelCmdRunMode_EXECUTE_NO_EFFECT);
  level_extension_do_transition(ep_id, (uint8_t)new_level, transition_time, true, with_on_off);

  return true;
}

void level_extension_init(void)
{
  for(int i = 0; i < APP_EP_COUNT; i++)
  {
    TransitionCtx* ctx = &tr_ctx[i];

    sl_zigbee_endpoint_event_init(&ctx->transition_event, level_extension_channel_event_cb, i + 1);

    uint8_t level = 0xFE;
    EmberAfStatus status = emberAfReadAttribute(i + 1,
                                  ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                  ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (uint8_t *)&level,
                                  sizeof(level),
                                  NULL); // data type
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read CURRENT_LEVEL attribute! Status %x, ep %d", status, i + 1);
    }
  }
}

inline uint8_t level_extension_get_options(uint8_t options, uint8_t options_mask, uint8_t options_override)
{
  return  (options & ~options_mask) | (options_override & options_mask);
}

uint32_t level_extension_handle_cmd(sl_service_opcode_t opcode,
                                     sl_service_function_context_t *context)
{
    bool wasHandled = false;
    EmberAfClusterCommand* cmd = (EmberAfClusterCommand *)context->data;
    uint8_t ep_id = emberAfCurrentEndpoint();
    uint8_t options = 0x00;

#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE)
    EmberAfStatus status = emberAfReadAttribute(ep_id,
                                                ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                ZCL_OPTIONS_ATTRIBUTE_ID,
                                                CLUSTER_MASK_SERVER,
                                                (uint8_t *)&options,
                                                sizeof(options),
                                                NULL); // data type

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        DBG_LOG("Can't read OPTIONS attribute! Status %x, ep %d", status, ep_id);
        return EMBER_ZCL_STATUS_FAILURE;
    }
#endif

    switch(cmd->commandId)
    {
      default:
      {
        DBG_LOG("Unknown LEVEL command %02x received for ep %02x", cmd->commandId, ep_id);
        break;
      }
      case ZCL_MOVE_TO_LEVEL_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_move_to_level(ep_id, payload[0],
                                                            *(uint16_t*)&payload[1],
                                                            level_extension_get_options(options, payload[3], payload[4]),
                                                            false);
        }
        break;
      }
      case ZCL_MOVE_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_move_level(ep_id, (EmberAfMoveMode)payload[0],
                                                         payload[1],
                                                         level_extension_get_options(options, payload[2], payload[3]),
                                                         false);
        }
        break;
      }
      case ZCL_STEP_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_step_level(ep_id, (EmberAfStepMode)payload[0],
                                                         payload[1],
                                                         *(uint16_t*)&payload[2],
                                                         level_extension_get_options(options, payload[4], payload[5]),
                                                         false);
        }
        break;
      }
      case ZCL_STOP_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_stop(ep_id,
                                                   level_extension_get_options(options, payload[0], payload[1]),
                                                   false);
        }
        break;
      }
      case ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_move_to_level(ep_id, payload[0],
                                                            *(uint16_t*)&payload[1],
                                                            0x00,
                                                            true);
        }
        break;
      }
      case ZCL_MOVE_WITH_ON_OFF_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_move_level(ep_id, (EmberAfMoveMode)payload[0],
                                                         payload[1],
                                                         level_extension_get_options(options, payload[2], payload[3]),
                                                         true);
        }
        break;
      }
      case ZCL_STEP_WITH_ON_OFF_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_step_level(ep_id, (EmberAfStepMode)payload[0],
                                                         payload[1],
                                                         *(uint16_t*)&payload[2],
                                                         level_extension_get_options(options, payload[4], payload[5]),
                                                         true);
        }
        break;
      }
      case ZCL_STOP_WITH_ON_OFF_COMMAND_ID:
      {
        if (cmd->payloadStartIndex < cmd->bufLen)
        {
          uint8_t* payload = &cmd->buffer[cmd->payloadStartIndex];

          wasHandled = level_extension_handle_stop(ep_id,
                                                   level_extension_get_options(options, payload[0], payload[1]),
                                                   true);
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
