// Copyright 2020-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  "auto-prober" for RoboCoop devices
 *
 * @author kalmenn <kale@kalmenn.fr>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_robocoop
 */

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"

#include "robocoop_hmd_interface.h"


/*!
 * @implements xrt_auto_prober
 */
struct robocoop_auto_prober
{
	struct xrt_auto_prober base;
};

//! @private @memberof robocoop_auto_prober
static inline struct robocoop_auto_prober *
robocoop_auto_prober(struct xrt_auto_prober *xap)
{
	return (struct robocoop_auto_prober *)xap;
}

//! @private @memberof robocoop_auto_prober
static void
robocoop_auto_prober_destroy(struct xrt_auto_prober *p)
{
	struct robocoop_auto_prober *ap = robocoop_auto_prober(p);

	free(ap);
}

//! @public @memberof robocoop_auto_prober
static int
robocoop_auto_prober_autoprobe(struct xrt_auto_prober *xap,
                             cJSON *attached_data,
                             bool no_hmds,
                             struct xrt_prober *xp,
                             struct xrt_device **out_xdevs)
{
	struct robocoop_auto_prober *ap = robocoop_auto_prober(xap);
	(void)ap;

	// Do not create an HMD device if we are not looking for HMDs.
	if (no_hmds) {
		return 0;
	}

	out_xdevs[0] = robocoop_create();
	return 1;
}

struct xrt_auto_prober *
robocoop_create_auto_prober(void)
{
	struct robocoop_auto_prober *ap = U_TYPED_CALLOC(struct robocoop_auto_prober);
	ap->base.name = "Robocoop HMD Auto-Prober";
	ap->base.destroy = robocoop_auto_prober_destroy;
	ap->base.lelo_dallas_autoprobe = robocoop_auto_prober_autoprobe;

	return &ap->base;
}
