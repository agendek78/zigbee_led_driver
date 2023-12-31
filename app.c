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

#include "app/framework/include/af.h"
#include "zigbee_app_framework_event.h"
#include "sl_component_catalog.h"
#include "zigbee_sleep_config.h"
#include "network-creator.h"
#include "network-creator-security.h"
#include "network-steering.h"
#include "find-and-bind-target.h"
#include "attribute-storage.h"
#include "ota-client.h"
#include "ota-storage.h"
#include "ota.h"
#include "slot-manager.h"
#include "em_chip.h"
#include "dbg_log.h"
#include "led_channel.h"
#include "led_effect.h"
#include "button.h"
#include "on_off_extension.h"
#include "level_extension.h"
#include "zcl_extension.h"
#include "app.h"

#define LED_DRV_MAX_FB_EP           APP_EP_COUNT
#define LED_DRV_PAIRING_EXIT_DELAY  500

typedef enum
{
    LedDrvState_IDLE,
    LedDrvState_ON_NETWORK,
    LedDrvState_FB_NETWORK_OPEN
} LedDrvState;

typedef struct
{
    LedDrvState     app_state;
    uint8_t         fb_current_ep;
    bool            exec_reboot;

    sl_zigbee_event_t pairing_mode_exit_event;
} LedDrvAppCtx;

static bool initialized = false;

static LedDrvAppCtx    ctx = {0};

static void led_drv_ota_version_init(void)
{
    char sw_build[32] = {0};

    snprintf(&sw_build[1], sizeof(sw_build) - 1, "%d.%03d.%03d",
             EMBER_AF_CUSTOM_FIRMWARE_VERSION >> 24,
             (EMBER_AF_CUSTOM_FIRMWARE_VERSION >> 16) & 0xFF,
             EMBER_AF_CUSTOM_FIRMWARE_VERSION & 0xFFFF
             );

    sw_build[0] = strlen(&sw_build[1]);

    emberAfWriteServerAttribute(1,
                                ZCL_BASIC_CLUSTER_ID,
                                ZCL_SW_BUILD_ID_ATTRIBUTE_ID, (uint8_t*)sw_build,
                                ZCL_CHAR_STRING_ATTRIBUTE_TYPE);

    DBG_LOG("Running app version %s", &sw_build[1]);
}

static void led_drv_fb_stop(uint8_t endpoint)
{
    uint16_t identifyTime = 0;

    if (emberAfContainsServer(endpoint, ZCL_IDENTIFY_CLUSTER_ID))
    {
        emberAfWriteServerAttribute(endpoint,
                                    ZCL_IDENTIFY_CLUSTER_ID,
                                    ZCL_IDENTIFY_TIME_ATTRIBUTE_ID, (uint8_t*) &identifyTime,
                                    ZCL_INT16U_ATTRIBUTE_TYPE);
    }
}

static void led_drv_fb_exit(void)
{
    if (ctx.fb_current_ep > 0 && ctx.fb_current_ep <= LED_DRV_MAX_FB_EP)
    {
        led_drv_fb_stop(ctx.fb_current_ep);
    }

    emberAfPluginNetworkCreatorSecurityCloseNetwork();
}

static bool led_drv_fb_activate_next_endpoint(void)
{
    while(ctx.fb_current_ep <= LED_DRV_MAX_FB_EP)
    {
        if (emberAfEndpointIsEnabled(ctx.fb_current_ep) == true)
        {
            emberAfPluginNetworkCreatorSecurityOpenNetwork();
            emberAfPluginFindAndBindTargetStart(ctx.fb_current_ep);
            return false;
        }

        ctx.fb_current_ep++;
    }

    led_drv_fb_exit();

    return true;
}

void led_drv_fb_activate(void)
{
    if (ctx.app_state == LedDrvState_ON_NETWORK)
    {
        ctx.fb_current_ep = 1;
        ctx.app_state = LedDrvState_FB_NETWORK_OPEN;

        led_drv_fb_activate_next_endpoint();
    }
    else if (ctx.app_state == LedDrvState_FB_NETWORK_OPEN)
    {
        led_drv_fb_stop(ctx.fb_current_ep);
        ctx.fb_current_ep++;

        led_drv_fb_activate_next_endpoint();
    }
}

uint8_t led_drv_active_fb_ep_get(void)
{
    if (ctx.app_state == LedDrvState_FB_NETWORK_OPEN)
    {
        return ctx.fb_current_ep;
    }

    return 0;
}

void led_drv_exit_pairing(void)
{
    sl_zigbee_event_set_delay_ms(&ctx.pairing_mode_exit_event, LED_DRV_PAIRING_EXIT_DELAY);
}

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action. The framework
 * will always process the stack status after the callback returns.
 */
void emberAfStackStatusCallback(EmberStatus status)
{
    switch(status)
    {
        case EMBER_NETWORK_DOWN:
        {
            ctx.app_state = LedDrvState_IDLE;
            if (ctx.exec_reboot == true)
            {
                CHIP_Reset();
            }
            break;
        }
        case EMBER_NETWORK_UP:
        {
            ctx.app_state = LedDrvState_ON_NETWORK;
            break;
        }
        case EMBER_NETWORK_OPENED:
        {
            led_effect_run(LedChannel_AUX, LedEffect_JoiningNetwork, LED_EFFECT_INFINITE);
            break;
        }
        case EMBER_NETWORK_CLOSED:
        {
            led_effect_run(LedChannel_AUX, LedEffect_None, LED_EFFECT_INFINITE);
            ctx.app_state = LedDrvState_ON_NETWORK;
            break;
        }
        default:
        {
            break;
        }
    }
}

static void led_drv_pairing_exit_cb(sl_zigbee_event_t *event)
{
    DBG_LOG("Exiting pairing mode!!!");
    led_drv_fb_exit();
}

/** @brief Init
 * Application init function
 */
void emberAfMainInitCallback(void)
{
    sl_zigbee_event_init(&ctx.pairing_mode_exit_event, led_drv_pairing_exit_cb);
    led_channel_init();
    led_effect_init();

    button_init();
    initialized = true;

    led_effect_run(LedChannel_AUX, LedEffect_Reboot, 1);
}

void app_init(void)
{
    zcl_extension_init();
    led_channel_endpoints_enable();
    led_drv_ota_version_init();

    if (emberAfNetworkState() != EMBER_JOINED_NETWORK)
    {
        EmberStatus status = emberAfPluginNetworkSteeringStart();
        DBG_LOG("%s network %s: 0x%02X", "Join", "start", status);
    }

    DBG_LOG("APP_init!");
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to EMBER_SUCCESS to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt.
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID.
 *
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network.
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete.
 */
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status, uint8_t totalBeacons, uint8_t joinAttempts,
                                                  uint8_t finalState)
{
    DBG_LOG("Join network complete: 0x%02X", status);

    if (status != EMBER_SUCCESS)
    {
        status = emberAfPluginNetworkCreatorStart(false); // distributed
        DBG_LOG("Form network start: 0x%02X", status);
    }
}

/** @brief Complete the network creation process.
 *
 * This callback notifies the user that the network creation process has
 * completed successfully.
 *
 * @param network The network that the network creator plugin successfully
 * formed.
 *
 * @param usedSecondaryChannels Whether or not the network creator wants to
 * form a network on the secondary channels.
 */
void emberAfPluginNetworkCreatorCompleteCallback(const EmberNetworkParameters *network,
                                                 bool usedSecondaryChannels)
{
    DBG_LOG("Form Network Complete!");
}


/** @brief Post Attribute Change
 *
 * This function is called by the application framework after it changes an
 * attribute value. The value passed into this callback is the value to which
 * the attribute was set by the framework.
 */
void emberAfPostAttributeChangeCallback(uint8_t endpoint, EmberAfClusterId clusterId, EmberAfAttributeId attributeId,
                                        uint8_t mask, uint16_t manufacturerCode, uint8_t type, uint8_t size,
                                        uint8_t *value)
{
    if (mask == CLUSTER_MASK_SERVER)
    {
        //DBG_LOG("Cluster %04x attr %04x change", clusterId, attributeId);
        switch(clusterId)
        {
            case ZCL_ON_OFF_CLUSTER_ID:
            {
                on_off_attribute_written(endpoint, attributeId, size, value);
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

bool emberAfPreZDOMessageReceivedCallback(EmberNodeId emberNodeId,
                                               EmberApsFrame* apsFrame,
                                               int8u* message,
                                               int16u length)
{
    if (apsFrame->clusterId == END_DEVICE_ANNOUNCE)
    {
        if (ctx.app_state == LedDrvState_FB_NETWORK_OPEN)
        {
            led_effect_run(ctx.fb_current_ep - 1, LedEffect_DeviceJoined, 3);
        }
        return true;
    }
    return false;
}

/** @brief
 *
 * Application framework equivalent of ::emberRadioNeedsCalibratingHandler
 */
void emberAfRadioNeedsCalibratingCallback(void)
{
  sl_mac_calibrate_current_channel();
}

void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{
    DBG_LOG("Identify started on ep %d, time %d", endpoint, identifyTime);
    if (identifyTime != 0)
    {
        led_effect_run(endpoint - 1, LedEffect_Identify, LED_EFFECT_INFINITE);
    }
}

void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
    DBG_LOG("Identify stopped on endpoint %d", endpoint);
    led_effect_run(endpoint - 1, LedEffect_None, LED_EFFECT_INFINITE);
}

void emberAfOtaClientVersionInfoCallback(EmberAfOtaImageId* currentImageInfo,
                                              int16u* hardwareVersion)
{
   if (currentImageInfo != NULL)
   {
        MEMSET(currentImageInfo, 0, sizeof(EmberAfOtaImageId));
        currentImageInfo->manufacturerId  = EMBER_AF_MANUFACTURER_CODE;
        currentImageInfo->imageTypeId     = EMBER_AF_IMAGE_TYPE_ID;
        currentImageInfo->firmwareVersion = EMBER_AF_CUSTOM_FIRMWARE_VERSION;
   }

   if (hardwareVersion != NULL)
   {
       *hardwareVersion = EMBER_AF_INVALID_HARDWARE_VERSION;
   }
}

bool emberAfOtaClientDownloadCompleteCallback(EmberAfOtaDownloadResult success,
                                                   const EmberAfOtaImageId* id)
{
  if (success != EMBER_AF_OTA_DOWNLOAD_AND_VERIFY_SUCCESS)
  {
      DBG_LOG("Download failed with error 0x%02x", success);
      return false;
  }

  DBG_LOG("Download and verify success. Image type 0x%04x, version 0x%08x",
          id->imageTypeId, id->firmwareVersion);

  return true;
}

void emberAfOtaClientBootloadCallback(const EmberAfOtaImageId* id)
{
  // OTA Server has told us to bootload.
  // Any final preparation prior to the bootload should be done here.
  // It is assumed that the device will reset in most all cases.

  uint32_t offset;
  uint32_t endOffset;
  uint32_t slot;

  if (EMBER_AF_OTA_STORAGE_SUCCESS
      != sli_zigbee_af_ota_storage_get_tag_offset_and_size(id,
                                           OTA_TAG_UPGRADE_IMAGE,
                                           &offset,
                                           &endOffset))
  {
    emberAfCoreFlush();
    otaPrintln("Image does not contain an Upgrade Image Tag (0x%2X). Skipping "
               "upgrade.", OTA_TAG_UPGRADE_IMAGE);
    return;
  }

  otaPrintln("Executing bootload callback.");
  (void) emberSerialWaitSend(APP_SERIAL);

  // If we're using slots, we'll need to use a different set of APIs
  slot = sli_zigbee_af_ota_storage_get_slot();

  // These routines will NOT return unless we failed to launch the bootloader.
  if (INVALID_SLOT != slot)
  {
      emberAfPluginSlotManagerBootSlot(slot);
  }
  else
  {
      emberAfOtaBootloadCallback(id, OTA_TAG_UPGRADE_IMAGE);
  }
}

void led_drv_reboot_set(void)
{
    ctx.exec_reboot = true;
}
