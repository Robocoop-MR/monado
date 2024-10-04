// Copyright 2020-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  RoboCoop's driver's configuration handling
 *
 * @author kalmenn <kale@kalmenn.fr>
 * @ingroup drv_robocoop_internals
 */

#pragma once

#include "robocoop_devices.h"

#include <cjson/cJSON.h>

#define ROBOCOOP_CONFIG_DIR_FRAGMENT "robocoop"

#define ROBOCOOP_HMD_CONFIG_FRAGMENT "robocoop_hmd.json"

#define ROBOCOOP_HMD_DEFAULT_SCREEN_WIDTH 1920
#define ROBOCOOP_HMD_DEFAULT_SCREEN_HEIGHT 1080

/*!
 * @brief Finds and parses the configuration file for one of RoboCoop's devices
 *
 * @param[in] device The model of the device whose config should be retrieved
 *
 * @returns The configuration for the requested device
 * @retval null The configuration file could not be located and / or read
 * @retval "cJSON *" A pointer to the parsed configuration. Fields are not validated. The caller is responsible for
 * freeing this pointer
 */
cJSON *
robocoop_get_device_config(enum robocoop_device device);

/*!
 * TODO: doc
 * TOOD: check for nulls and stuff
 */
int
robocoop_save_device_config(enum robocoop_device device, cJSON *config);
