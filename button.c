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

#include "button.h"
#include "sl_simple_button_instances.h"
#include "zigbee_app_framework_event.h"
#include "global-callback.h"
#include "binding-table.h"

#include "dbg_log.h"
#include "app.h"

#define BUTTON_LONG_PRESS_TIMEOUT   3000

static uint32_t last_press_ts;
static sl_zigbee_event_t long_press_event;
static bool button_event_handled;
static bool initialized = false;

static void button_on_long_press(sl_zigbee_event_t* ev)
{
    (void)ev;

    button_event_handled = true;

    EmberNetworkStatus net_status = emberAfNetworkState();
    if (net_status == EMBER_JOINED_NETWORK ||
        net_status == EMBER_JOINED_NETWORK_NO_PARENT)
    {
       DBG_LOG("Leaving network...");
       emberLeaveNetwork();

       EmberAfOtaStorageStatus status = emberAfOtaStorageClearTempDataCallback();
       DBG_LOG("OTA storage clear with status: 0x%X", status);

       EmberStatus eb_s = emberClearBindingTable();
       DBG_LOG("Binding table clear with status %02x!", eb_s);
    }

    led_drv_reboot_set();
}

void sl_button_on_change(const sl_button_t *handle)
{
    if (initialized == false)
    {
        return;
    }

    sl_button_state_t state = sl_simple_button_get_state(handle);

    DBG_LOG("Button event: %s", state == SL_SIMPLE_BUTTON_PRESSED ? "pressed" : "released");

    if (state == SL_SIMPLE_BUTTON_RELEASED)
    {
        if (last_press_ts == 0)
        {
            /* false release */
            return;
        }

        sl_zigbee_event_set_inactive(&long_press_event);

        last_press_ts = 0;

        if (button_event_handled == true)
        {
            return;
        }

        led_drv_fb_activate();
    }
    else
    {
        last_press_ts = halCommonGetInt32uMillisecondTick();
        sl_zigbee_event_set_delay_ms(&long_press_event, BUTTON_LONG_PRESS_TIMEOUT);
        button_event_handled = false;
    }
}

void button_init(void)
{
    sl_zigbee_event_init(&long_press_event, button_on_long_press);
    initialized = true;
}
