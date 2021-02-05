/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

/*
 * This includes all headers and is used only by the generated
 * vn_entrypoints.c and vn_extensions.c.
 */
#ifndef VN_PRIVATE_H
#define VN_PRIVATE_H

#include "vn_common.h"

#include "vn_cs.h"
#include "vn_device.h"
#include "vn_icd.h"
#include "vn_renderer.h"
#include "vn_wsi.h"

/* make vn_entrypoints.c happy */

const char *
vn_get_instance_entry_name(int index);

const char *
vn_get_physical_device_entry_name(int index);

const char *
vn_get_device_entry_name(int index);

#endif /* VN_PRIVATE_H */
