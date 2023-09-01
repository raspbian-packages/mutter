/*
 * Copyright (C) 2022 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_FRAME_NATIVE_H
#define META_FRAME_NATIVE_H

#include "backends/native/meta-kms-types.h"
#include "clutter/clutter.h"
#include "core/util-private.h"

typedef struct _MetaFrameNative MetaFrameNative;

MetaFrameNative * meta_frame_native_new (void);

META_EXPORT_TEST
MetaFrameNative * meta_frame_native_from_frame (ClutterFrame *frame);

void meta_frame_native_set_kms_update (MetaFrameNative *frame_native,
                                       MetaKmsUpdate   *kms_update);

META_EXPORT_TEST
MetaKmsUpdate * meta_frame_native_ensure_kms_update (MetaFrameNative *frame_native,
                                                     MetaKmsDevice   *kms_device);

MetaKmsUpdate * meta_frame_native_steal_kms_update (MetaFrameNative *frame_native);

META_EXPORT_TEST
gboolean meta_frame_native_has_kms_update (MetaFrameNative *frame_native);

META_EXPORT_TEST
gboolean meta_frame_native_had_kms_update (MetaFrameNative *frame_native);

#endif /* META_FRAME_NATIVE_H */
