/*
 * release_stubs.c
 *
 *  Created on: Dec 31, 2023
 *      Author: andy
 */

#include <stdint.h>
#include <stdbool.h>

void sl_cli_instances_init(void)
{
}

void sl_cli_instances_tick(void)
{
}

bool sl_cli_instances_is_ok_to_sleep(void)
{
  return true;
}

void sli_zigbee_zcl_cli_init(uint8_t init_level)
{

}
