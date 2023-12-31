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

#include "led_effect.h"
#include "dbg_log.h"
#include "app.h"

#include "zigbee_app_framework_event.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define LED_EFFECT_TICKS_PER_MSEC       (1000 / 50)

#define LED_EFFECT_MSEC_TO_TICKS(x)     (((x) * LED_EFFECT_TICKS_PER_MSEC) / 1000)
#define LED_EFFECT_TICKS_TO_MSEC(x)     ((x) * ((1000) / LED_EFFECT_TICKS_PER_MSEC))

/**
 * END
 * DELAY        ticks
 * LED_LEVEL    channel, level_percent
 * LED_RAMP     channel, level_start, level_stop, rate_ticks
 *
 */
#define LED_EFFECT_INST_END         0
#define LED_EFFECT_INST_DELAY       1
#define LED_EFFECT_INST_LEVEL       2
#define LED_EFFECT_INST_RAMP        3

#define LED_EFFECT_CHANNEL_R        1
#define LED_EFFECT_CHANNEL_G        2
#define LED_EFFECT_CHANNEL_B        3

#define LED_EFFECT_INST(c)          {.code = (c)}
#define LED_EFFECT_INSTP(c, p, v)   {.code = (c), .params.p = v}

typedef struct __attribute__((packed))
{
    uint32_t code : 3;

    union
    {
        struct
        {
            uint32_t    ticks : 8;
        } delay;

        struct
        {
            uint32_t    level   : 7;
        } level;

        struct
        {
            uint32_t    start_level : 7;
            uint32_t    stop_level  : 7;
            uint32_t    ticks       : 10;
        } ramp;
    } params;

} LedEffectInstruction;

typedef struct
{
    const LedEffectInstruction*    code;
    uint8_t ic;
    union
    {
        struct
        {
            bool    started;
            int16_t  last_level;
            uint16_t ticks;
        } ramp;

    } regs;

    LedEffect           infinite_effect;
    LedEffect           iterative_effect;
    size_t              iterate_count;
    sl_zigbee_event_t   led_effect_tick_event;

} LedEffectExecCtx;

typedef struct
{
    LedEffectExecCtx  execution_ctx[LedChannel_MAX];
    bool              initialized;

} LedEffectCtx;

static const LedEffectInstruction led_effect_none[] =
{
    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction led_effect_joining_network[] =
{
    {
        .code = LED_EFFECT_INST_RAMP,
        .params.ramp =
        {
            1,
            100,
            LED_EFFECT_MSEC_TO_TICKS(500)
        }
    },
    {
        .code = LED_EFFECT_INST_RAMP,
        .params.ramp =
        {
            99,
            0,
            LED_EFFECT_MSEC_TO_TICKS(500)
        }
    },

    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction led_effect_left_network[] =
{
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            50,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(250)
        }
    },
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            0,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(250)
        }
    },

    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction led_effect_reboot[] =
{
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            50,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(150)
        }
    },
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            50,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(150)
        }
    },
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            0,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(150)
        }
    },

    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction led_effect_identify[] =
{
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            100,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(500)
        }
    },
    {
        .code = LED_EFFECT_INST_LEVEL,
        .params.level =
        {
            0,
        }
    },
    {
        .code = LED_EFFECT_INST_DELAY,
        .params.delay =
        {
            LED_EFFECT_MSEC_TO_TICKS(500)
        }
    },

    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction led_effect_device_joined[] =
{
    {
        .code = LED_EFFECT_INST_RAMP,
        .params.ramp =
        {
            1,
            100,
            LED_EFFECT_MSEC_TO_TICKS(150)
        }
    },
    {
        .code = LED_EFFECT_INST_RAMP,
        .params.ramp =
        {
            99,
            0,
            LED_EFFECT_MSEC_TO_TICKS(150)
        }
    },

    LED_EFFECT_INST(LED_EFFECT_INST_END)
};

static const LedEffectInstruction* led_effects[] =
{
    &led_effect_none[0],
    &led_effect_joining_network[0],
    &led_effect_left_network[0],
    &led_effect_reboot[0],
    &led_effect_identify[0],
    &led_effect_device_joined[0]
};

static LedEffectCtx led_effect_ctx;

static void led_effect_start(LedChannel ch, LedEffect effect)
{
    LedEffectExecCtx *ctx = &led_effect_ctx.execution_ctx[ch];

    ctx->code = led_effects[effect];
    sl_zigbee_event_set_active(&ctx->led_effect_tick_event);
}

static void led_effect_next_instr(LedEffectExecCtx *ctx)
{
    ctx->ic++;
    memset(&ctx->regs, 0, sizeof(ctx->regs));
}

static inline int32_t line_approx(int32_t start, int32_t stop, int32_t time, int32_t t)
{
    stop *= 100;
    start *= 100;
    return (((stop - start) * t + (start * time)) / time + 50) / 100;
}

static void led_effect_tick_event_cb(sl_zigbee_event_t *event)
{
    uint32_t delay = 0;

    LedEffectExecCtx *ctx = NULL;
    LedChannel ch = 0;
    for (; ch < ARRAY_SIZE(led_effect_ctx.execution_ctx); ch++)
    {
        if (&led_effect_ctx.execution_ctx[ch].led_effect_tick_event == event)
        {
            ctx = &led_effect_ctx.execution_ctx[ch];
            break;
        }
    }

    if (ctx == NULL)
    {
        DBG_LOG("Unexpected LED effect event call!");
        return;
    }

    while (ctx->code != NULL)
    {
        const LedEffectInstruction *i = &ctx->code[ctx->ic];

        if (i->code == LED_EFFECT_INST_END)
        {
            ctx->code = NULL;
            ctx->ic = 0;
            break;
        }
        else if (i->code == LED_EFFECT_INST_DELAY)
        {
            delay = LED_EFFECT_TICKS_TO_MSEC(i->params.delay.ticks);
            led_effect_next_instr(ctx);
        }
        else if (i->code == LED_EFFECT_INST_LEVEL)
        {
            led_channel_level_set(ch, i->params.level.level);
            led_effect_next_instr(ctx);
        }
        else if (i->code == LED_EFFECT_INST_RAMP)
        {
            int16_t level = 0;

            delay = LED_EFFECT_TICKS_TO_MSEC(1);
            if (ctx->regs.ramp.started == false)
            {
                level = i->params.ramp.start_level;
                ctx->regs.ramp.started = true;
            }
            else
            {
                level = line_approx(i->params.ramp.start_level, i->params.ramp.stop_level,
                                    i->params.ramp.ticks,
                                    ctx->regs.ramp.ticks++);

                //DBG_LOG("RAMP %d, tick %d", level, ctx->regs.ramp.ticks);
                if (level == i->params.ramp.stop_level)
                {
                    led_effect_next_instr(ctx);
                    delay = 0;
                }
            }

            led_channel_level_set(ch, level);
        }
        else
        {
            DBG_LOG("Unexpected LED effect instruction code!");
            break;
        }

        if (delay != 0)
        {
            break;
        }
    }

    if (delay != 0)
    {
        sl_zigbee_event_set_delay_ms(&ctx->led_effect_tick_event, delay);
    }
    else
    {
        if (ctx->iterate_count > 0)
        {
            ctx->iterate_count--;
        }

        if (ctx->iterate_count > 0)
        {
            led_effect_start(ch, ctx->iterative_effect);
        }
        else if (ctx->infinite_effect != LedEffect_None)
        {
            led_effect_start(ch, ctx->infinite_effect);
        }
    }
}

void led_effect_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(led_effect_ctx.execution_ctx); i++)
    {
        sl_zigbee_event_init(&led_effect_ctx.execution_ctx[i].led_effect_tick_event,
                               led_effect_tick_event_cb);
    }

    led_effect_ctx.initialized = true;
}

void led_effect_run(LedChannel ch, LedEffect effect, size_t count)
{
    if (led_effect_ctx.initialized == false)
    {
        return;
    }

    LedEffectExecCtx *ctx = &led_effect_ctx.execution_ctx[ch];

    if (ctx->infinite_effect == LedEffect_None &&
        ctx->iterative_effect == LedEffect_None &&
        effect == LedEffect_None)
    {
      return;
    }

    sl_zigbee_event_set_inactive(&ctx->led_effect_tick_event);

    if (effect == LedEffect_None)
    {
        if (ch == LedChannel_AUX)
        {
            led_channel_level_set(ch, 0);
        }
        else
        {
            uint8_t on_off = 0;

            emberAfReadServerAttribute(ch + 1,
                                       ZCL_ON_OFF_CLUSTER_ID,
                                       ZCL_ON_OFF_ATTRIBUTE_ID, &on_off, sizeof(on_off));

            if (on_off != 0)
            {
                uint8_t current_level = 0;
                emberAfReadServerAttribute(ch + 1,
                                           ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                           ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                           &current_level, sizeof(current_level));

                led_channel_zcl_level_set(ch, current_level);
            }
            else
            {
                led_channel_level_set(ch, 0);
            }
        }

        ctx->infinite_effect = LedEffect_None;
        ctx->iterative_effect = LedEffect_None;
        ctx->iterate_count = 0;
        return;
    }

    ctx->iterate_count = count;
    if (count == 0)
    {
        ctx->infinite_effect = effect;
    }
    else
    {
        ctx->iterative_effect = effect;
    }

    led_effect_start(ch, effect);
}

