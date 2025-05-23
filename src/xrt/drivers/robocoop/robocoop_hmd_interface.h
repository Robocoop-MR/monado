// Copyright 2020-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to RoboCoop's HMD driver.
 *
 * @author kalmenn <kale@kalmenn.fr>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup drv_robocoop
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "util/u_json.h"

/*!
 * @defgroup drv_robocoop RoboCoop's HMD driver
 * @ingroup drv
 *
 * @brief Driver for RoboCoop's HMD.
 */

/*!
 * @defgroup drv_robocoop_internals
 * @ingroup drv_robocoop
 *
 * @brief Our driver's internals
 */

/*!
 * Create a auto prober for RoboCoop's HMD.
 *
 * @ingroup drv_robocoop
 */
struct xrt_auto_prober *
robocoop_create_auto_prober(void);

/*!
 * Create a Sample HMD.
 *
 * This is only exposed so that the prober (in one source file)
 * can call the construction function (in another)
 * @ingroup drv_robocoop
 */
struct xrt_device *
robocoop_create(const cJSON *config);

/*!
 * @dir drivers/robocoop
 *void
 * @brief @ref drv_robocoop files.
 */


#ifdef __cplusplus
}
#endif
