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

#include "config.h"

#include "backends/native/meta-frame-native.h"

#include "backends/native/meta-kms-update.h"
#include "clutter/clutter-mutter.h"
#include "core/util-private.h"

struct _MetaFrameNative
{
  ClutterFrame base;

  MetaKmsUpdate *kms_update;
  gboolean had_kms_update;
};

static void
meta_frame_native_release (ClutterFrame *frame)
{
  MetaFrameNative *frame_native = meta_frame_native_from_frame (frame);

  g_return_if_fail (!frame_native->kms_update);
}

MetaFrameNative *
meta_frame_native_new (void)
{
  return clutter_frame_new (MetaFrameNative, meta_frame_native_release);
}

MetaFrameNative *
meta_frame_native_from_frame (ClutterFrame *frame)
{
  return META_CONTAINER_OF (frame, MetaFrameNative, base);
}

void
meta_frame_native_set_kms_update (MetaFrameNative *frame_native,
                                  MetaKmsUpdate   *kms_update)
{
  g_return_if_fail (!frame_native->kms_update);

  frame_native->kms_update = kms_update;
  frame_native->had_kms_update = TRUE;
}

MetaKmsUpdate *
meta_frame_native_ensure_kms_update (MetaFrameNative *frame_native,
                                     MetaKmsDevice   *kms_device)
{
  if (frame_native->kms_update)
    {
      g_warn_if_fail (meta_kms_update_get_device (frame_native->kms_update) ==
                      kms_device);
      return frame_native->kms_update;
    }

  frame_native->kms_update = meta_kms_update_new (kms_device);
  frame_native->had_kms_update = TRUE;
  return frame_native->kms_update;
}

MetaKmsUpdate *
meta_frame_native_steal_kms_update (MetaFrameNative *frame_native)
{
  return g_steal_pointer (&frame_native->kms_update);
}

gboolean
meta_frame_native_has_kms_update (MetaFrameNative *frame_native)
{
  return !!frame_native->kms_update;
}

gboolean
meta_frame_native_had_kms_update (MetaFrameNative *frame_native)
{
  return frame_native->had_kms_update;
}
