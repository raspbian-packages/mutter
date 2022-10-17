/*
 * Copyright (C) 2019 Red Hat
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

#include "backends/meta-output.h"
#include "backends/native/meta-kms-connector.h"
#include "backends/native/meta-kms-connector-private.h"

#include <errno.h>

#include "backends/native/meta-kms-crtc.h"
#include "backends/native/meta-kms-device-private.h"
#include "backends/native/meta-kms-impl-device.h"
#include "backends/native/meta-kms-mode-private.h"
#include "backends/native/meta-kms-update-private.h"

typedef struct _MetaKmsConnectorPropTable
{
  MetaKmsProp props[META_KMS_CONNECTOR_N_PROPS];
  MetaKmsEnum dpms_enum[META_KMS_CONNECTOR_DPMS_N_PROPS];
  MetaKmsEnum underscan_enum[META_KMS_CONNECTOR_UNDERSCAN_N_PROPS];
  MetaKmsEnum privacy_screen_sw_enum[META_KMS_CONNECTOR_PRIVACY_SCREEN_N_PROPS];
  MetaKmsEnum privacy_screen_hw_enum[META_KMS_CONNECTOR_PRIVACY_SCREEN_N_PROPS];
  MetaKmsEnum scaling_mode_enum[META_KMS_CONNECTOR_SCALING_MODE_N_PROPS];
  MetaKmsEnum panel_orientation_enum[META_KMS_CONNECTOR_PANEL_ORIENTATION_N_PROPS];
} MetaKmsConnectorPropTable;

struct _MetaKmsConnector
{
  GObject parent;

  MetaKmsDevice *device;

  uint32_t id;
  uint32_t type;
  uint32_t type_id;
  char *name;

  drmModeConnection connection;
  MetaKmsConnectorState *current_state;

  MetaKmsConnectorPropTable prop_table;

  uint32_t edid_blob_id;
  uint32_t tile_blob_id;

  gboolean fd_held;
};

G_DEFINE_TYPE (MetaKmsConnector, meta_kms_connector, G_TYPE_OBJECT)

typedef enum _MetaKmsPrivacyScreenHwState
{
  META_KMS_PRIVACY_SCREEN_HW_STATE_DISABLED,
  META_KMS_PRIVACY_SCREEN_HW_STATE_ENABLED,
  META_KMS_PRIVACY_SCREEN_HW_STATE_DISABLED_LOCKED,
  META_KMS_PRIVACY_SCREEN_HW_STATE_ENABLED_LOCKED,
} MetaKmsPrivacyScreenHwState;

MetaKmsDevice *
meta_kms_connector_get_device (MetaKmsConnector *connector)
{
  return connector->device;
}

uint32_t
meta_kms_connector_get_prop_id (MetaKmsConnector     *connector,
                                MetaKmsConnectorProp  prop)
{
  return connector->prop_table.props[prop].prop_id;
}

const char *
meta_kms_connector_get_prop_name (MetaKmsConnector     *connector,
                                  MetaKmsConnectorProp  prop)
{
  return connector->prop_table.props[prop].name;
}

uint64_t
meta_kms_connector_get_prop_drm_value (MetaKmsConnector     *connector,
                                       MetaKmsConnectorProp  property,
                                       uint64_t              value)
{
  MetaKmsProp *prop = &connector->prop_table.props[property];
  return meta_kms_prop_convert_value (prop, value);
}

uint32_t
meta_kms_connector_get_connector_type (MetaKmsConnector *connector)
{
  return connector->type;
}

uint32_t
meta_kms_connector_get_id (MetaKmsConnector *connector)
{
  return connector->id;
}

const char *
meta_kms_connector_get_name (MetaKmsConnector *connector)
{
  return connector->name;
}

gboolean
meta_kms_connector_can_clone (MetaKmsConnector *connector,
                              MetaKmsConnector *other_connector)
{
  MetaKmsConnectorState *state = connector->current_state;
  MetaKmsConnectorState *other_state = other_connector->current_state;

  if (state->common_possible_clones == 0 ||
      other_state->common_possible_clones == 0)
    return FALSE;

  if (state->encoder_device_idxs != other_state->encoder_device_idxs)
    return FALSE;

  return TRUE;
}

MetaKmsMode *
meta_kms_connector_get_preferred_mode (MetaKmsConnector *connector)
{
  const MetaKmsConnectorState *state;
  GList *l;

  state = meta_kms_connector_get_current_state (connector);
  for (l = state->modes; l; l = l->next)
    {
      MetaKmsMode *mode = l->data;
      const drmModeModeInfo *drm_mode;

      drm_mode = meta_kms_mode_get_drm_mode (mode);
      if (drm_mode->type & DRM_MODE_TYPE_PREFERRED)
        return mode;
    }

  return NULL;
}

const MetaKmsConnectorState *
meta_kms_connector_get_current_state (MetaKmsConnector *connector)
{
  return connector->current_state;
}

gboolean
meta_kms_connector_is_underscanning_supported (MetaKmsConnector *connector)
{
  uint32_t underscan_prop_id;

  underscan_prop_id =
    meta_kms_connector_get_prop_id (connector,
                                    META_KMS_CONNECTOR_PROP_UNDERSCAN);

  return underscan_prop_id != 0;
}

gboolean
meta_kms_connector_is_privacy_screen_supported (MetaKmsConnector *connector)
{
  return meta_kms_connector_get_prop_id (connector,
    META_KMS_CONNECTOR_PROP_PRIVACY_SCREEN_HW_STATE) != 0;
}

static gboolean
has_privacy_screen_software_toggle (MetaKmsConnector *connector)
{
  return meta_kms_connector_get_prop_id (connector,
    META_KMS_CONNECTOR_PROP_PRIVACY_SCREEN_SW_STATE) != 0;
}

const MetaKmsRange *
meta_kms_connector_get_max_bpc (MetaKmsConnector *connector)
{
  const MetaKmsRange *range = NULL;

  if (connector->current_state &&
      meta_kms_connector_get_prop_id (connector,
                                      META_KMS_CONNECTOR_PROP_MAX_BPC))
    range = &connector->current_state->max_bpc;

  return range;
}

static void
sync_fd_held (MetaKmsConnector  *connector,
              MetaKmsImplDevice *impl_device)
{
  gboolean should_hold_fd;

  should_hold_fd =
    connector->current_state &&
    connector->current_state->current_crtc_id != 0;

  if (connector->fd_held == should_hold_fd)
    return;

  if (should_hold_fd)
    meta_kms_impl_device_hold_fd (impl_device);
  else
    meta_kms_impl_device_unhold_fd (impl_device);

  connector->fd_held = should_hold_fd;
}

static void
set_panel_orientation (MetaKmsConnectorState *state,
                       MetaKmsProp           *panel_orientation)
{
  MetaMonitorTransform transform;
  MetaKmsConnectorPanelOrientation orientation = panel_orientation->value;

  switch (orientation)
    {
    case META_KMS_CONNECTOR_PANEL_ORIENTATION_UPSIDE_DOWN:
      transform = META_MONITOR_TRANSFORM_180;
      break;
    case META_KMS_CONNECTOR_PANEL_ORIENTATION_LEFT_SIDE_UP:
      transform = META_MONITOR_TRANSFORM_90;
      break;
    case META_KMS_CONNECTOR_PANEL_ORIENTATION_RIGHT_SIDE_UP:
      transform = META_MONITOR_TRANSFORM_270;
      break;
    default:
      transform = META_MONITOR_TRANSFORM_NORMAL;
      break;
    }

  state->panel_orientation_transform = transform;
}

static void
set_privacy_screen (MetaKmsConnectorState *state,
                    MetaKmsConnector      *connector,
                    MetaKmsProp           *hw_state)
{
  MetaKmsConnectorPrivacyScreen privacy_screen = hw_state->value;

  if (!meta_kms_connector_is_privacy_screen_supported (connector))
    return;

  switch (privacy_screen)
    {
    case META_KMS_PRIVACY_SCREEN_HW_STATE_DISABLED:
      state->privacy_screen_state = META_PRIVACY_SCREEN_DISABLED;
      break;
    case META_KMS_PRIVACY_SCREEN_HW_STATE_DISABLED_LOCKED:
      state->privacy_screen_state = META_PRIVACY_SCREEN_DISABLED;
      state->privacy_screen_state |= META_PRIVACY_SCREEN_LOCKED;
      break;
    case META_KMS_PRIVACY_SCREEN_HW_STATE_ENABLED:
      state->privacy_screen_state = META_PRIVACY_SCREEN_ENABLED;
      break;
    case META_KMS_PRIVACY_SCREEN_HW_STATE_ENABLED_LOCKED:
      state->privacy_screen_state = META_PRIVACY_SCREEN_ENABLED;
      state->privacy_screen_state |= META_PRIVACY_SCREEN_LOCKED;
      break;
    default:
      state->privacy_screen_state = META_PRIVACY_SCREEN_DISABLED;
      g_warning ("Unknown privacy screen state: %u", privacy_screen);
    }

  if (!has_privacy_screen_software_toggle (connector))
    state->privacy_screen_state |= META_PRIVACY_SCREEN_LOCKED;
}

static void
state_set_properties (MetaKmsConnectorState *state,
                      MetaKmsImplDevice     *impl_device,
                      MetaKmsConnector      *connector,
                      drmModeConnector      *drm_connector)
{
  MetaKmsProp *props = connector->prop_table.props;
  MetaKmsProp *prop;

  prop = &props[META_KMS_CONNECTOR_PROP_SUGGESTED_X];
  if (prop->prop_id)
    state->suggested_x = prop->value;

  prop = &props[META_KMS_CONNECTOR_PROP_SUGGESTED_Y];
  if (prop->prop_id)
    state->suggested_y = prop->value;

  prop = &props[META_KMS_CONNECTOR_PROP_HOTPLUG_MODE_UPDATE];
  if (prop->prop_id)
    state->hotplug_mode_update = prop->value;

  prop = &props[META_KMS_CONNECTOR_PROP_SCALING_MODE];
  if (prop->prop_id)
    state->has_scaling = TRUE;

  prop = &props[META_KMS_CONNECTOR_PROP_PANEL_ORIENTATION];
  if (prop->prop_id)
    set_panel_orientation (state, prop);

  prop = &props[META_KMS_CONNECTOR_PROP_NON_DESKTOP];
  if (prop->prop_id)
    state->non_desktop = prop->value;

  prop = &props[META_KMS_CONNECTOR_PROP_PRIVACY_SCREEN_HW_STATE];
  if (prop->prop_id)
    set_privacy_screen (state, connector, prop);

  prop = &props[META_KMS_CONNECTOR_PROP_MAX_BPC];
  if (prop->prop_id)
    {
      state->max_bpc.value = prop->value;
      state->max_bpc.min_value = prop->range_min;
      state->max_bpc.max_value = prop->range_max;
    }
}

static CoglSubpixelOrder
drm_subpixel_order_to_cogl_subpixel_order (drmModeSubPixel subpixel)
{
  switch (subpixel)
    {
    case DRM_MODE_SUBPIXEL_NONE:
      return COGL_SUBPIXEL_ORDER_NONE;
      break;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
      return COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;
      break;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
      return COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR;
      break;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
      return COGL_SUBPIXEL_ORDER_VERTICAL_RGB;
      break;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
      return COGL_SUBPIXEL_ORDER_VERTICAL_BGR;
      break;
    case DRM_MODE_SUBPIXEL_UNKNOWN:
      return COGL_SUBPIXEL_ORDER_UNKNOWN;
    }
  return COGL_SUBPIXEL_ORDER_UNKNOWN;
}

static void
state_set_edid (MetaKmsConnectorState *state,
                MetaKmsConnector      *connector,
                MetaKmsImplDevice     *impl_device,
                uint32_t               blob_id)
{
  int fd;
  drmModePropertyBlobPtr edid_blob;
  GBytes *edid_data;

  fd = meta_kms_impl_device_get_fd (impl_device);
  edid_blob = drmModeGetPropertyBlob (fd, blob_id);
  if (!edid_blob)
    {
      g_warning ("Failed to read EDID of connector %s: %s",
                 connector->name, g_strerror (errno));
      return;
    }

   edid_data = g_bytes_new (edid_blob->data, edid_blob->length);
   drmModeFreePropertyBlob (edid_blob);

   state->edid_data = edid_data;
}

static void
state_set_tile_info (MetaKmsConnectorState *state,
                     MetaKmsConnector      *connector,
                     MetaKmsImplDevice     *impl_device,
                     uint32_t               blob_id)
{
  int fd;
  drmModePropertyBlobPtr tile_blob;

  state->tile_info = (MetaTileInfo) { 0 };

  fd = meta_kms_impl_device_get_fd (impl_device);
  tile_blob = drmModeGetPropertyBlob (fd, blob_id);
  if (!tile_blob)
    {
      g_warning ("Failed to read TILE of connector %s: %s",
                 connector->name, strerror (errno));
      return;
    }

  if (tile_blob->length > 0)
    {
      if (sscanf ((char *) tile_blob->data, "%d:%d:%d:%d:%d:%d:%d:%d",
                  &state->tile_info.group_id,
                  &state->tile_info.flags,
                  &state->tile_info.max_h_tiles,
                  &state->tile_info.max_v_tiles,
                  &state->tile_info.loc_h_tile,
                  &state->tile_info.loc_v_tile,
                  &state->tile_info.tile_w,
                  &state->tile_info.tile_h) != 8)
        {
          g_warning ("Couldn't understand TILE property blob of connector %s",
                     connector->name);
          state->tile_info = (MetaTileInfo) { 0 };
        }
    }

  drmModeFreePropertyBlob (tile_blob);
}

static void
state_set_blobs (MetaKmsConnectorState *state,
                 MetaKmsConnector      *connector,
                 MetaKmsImplDevice     *impl_device,
                 drmModeConnector      *drm_connector)
{
  MetaKmsProp *prop;

  prop = &connector->prop_table.props[META_KMS_CONNECTOR_PROP_EDID];
  if (prop->prop_id && prop->value)
    state_set_edid (state, connector, impl_device, prop->value);

  prop = &connector->prop_table.props[META_KMS_CONNECTOR_PROP_TILE];
  if (prop->prop_id && prop->value)
    state_set_tile_info (state, connector, impl_device, prop->value);
}

static void
state_set_physical_dimensions (MetaKmsConnectorState *state,
                               drmModeConnector      *drm_connector)
{
  state->width_mm = drm_connector->mmWidth;
  state->height_mm = drm_connector->mmHeight;
}

static void
state_set_modes (MetaKmsConnectorState *state,
                 MetaKmsImplDevice     *impl_device,
                 drmModeConnector      *drm_connector)
{
  int i;

  for (i = 0; i < drm_connector->count_modes; i++)
    {
      MetaKmsMode *mode;

      mode = meta_kms_mode_new (impl_device, &drm_connector->modes[i],
                                META_KMS_MODE_FLAG_NONE);
      state->modes = g_list_prepend (state->modes, mode);
    }
  state->modes = g_list_reverse (state->modes);
}

static void
set_encoder_device_idx_bit (uint32_t          *encoder_device_idxs,
                            uint32_t           encoder_id,
                            MetaKmsImplDevice *impl_device,
                            drmModeRes        *drm_resources)
{
  int fd;
  int i;

  fd = meta_kms_impl_device_get_fd (impl_device);

  for (i = 0; i < drm_resources->count_encoders; i++)
    {
      drmModeEncoder *drm_encoder;

      drm_encoder = drmModeGetEncoder (fd, drm_resources->encoders[i]);
      if (!drm_encoder)
        continue;

      if (drm_encoder->encoder_id == encoder_id)
        {
          *encoder_device_idxs |= (1 << i);
          drmModeFreeEncoder (drm_encoder);
          break;
        }

      drmModeFreeEncoder (drm_encoder);
    }
}

static void
state_set_crtc_state (MetaKmsConnectorState *state,
                      drmModeConnector      *drm_connector,
                      MetaKmsImplDevice     *impl_device,
                      drmModeRes            *drm_resources)
{
  int fd;
  int i;
  uint32_t common_possible_crtcs;
  uint32_t common_possible_clones;
  uint32_t encoder_device_idxs;

  fd = meta_kms_impl_device_get_fd (impl_device);

  common_possible_crtcs = UINT32_MAX;
  common_possible_clones = UINT32_MAX;
  encoder_device_idxs = 0;
  for (i = 0; i < drm_connector->count_encoders; i++)
    {
      drmModeEncoder *drm_encoder;

      drm_encoder = drmModeGetEncoder (fd, drm_connector->encoders[i]);
      if (!drm_encoder)
        continue;

      common_possible_crtcs &= drm_encoder->possible_crtcs;
      common_possible_clones &= drm_encoder->possible_clones;

      set_encoder_device_idx_bit (&encoder_device_idxs,
                                  drm_encoder->encoder_id,
                                  impl_device,
                                  drm_resources);

      if (drm_connector->encoder_id == drm_encoder->encoder_id)
        state->current_crtc_id = drm_encoder->crtc_id;

      drmModeFreeEncoder (drm_encoder);
    }

  state->common_possible_crtcs = common_possible_crtcs;
  state->common_possible_clones = common_possible_clones;
  state->encoder_device_idxs = encoder_device_idxs;
}

static MetaKmsConnectorState *
meta_kms_connector_state_new (void)
{
  MetaKmsConnectorState *state;

  state = g_new0 (MetaKmsConnectorState, 1);
  state->suggested_x = -1;
  state->suggested_y = -1;

  return state;
}

static void
meta_kms_connector_state_free (MetaKmsConnectorState *state)
{
  g_clear_pointer (&state->edid_data, g_bytes_unref);
  g_list_free_full (state->modes, (GDestroyNotify) meta_kms_mode_free);
  g_free (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MetaKmsConnectorState,
                               meta_kms_connector_state_free);

static gboolean
kms_modes_equal (GList *modes,
                 GList *other_modes)
{
  GList *l;

  if (g_list_length (modes) != g_list_length (other_modes))
    return FALSE;

  for (l = modes; l; l = l->next)
    {
      GList *k;
      MetaKmsMode *mode = l->data;

      for (k = other_modes; k; k = k->next)
        {
          MetaKmsMode *other_mode = k->data;

          if (!meta_kms_mode_equal (mode, other_mode))
            return FALSE;
        }
    }

  return TRUE;
}

static MetaKmsResourceChanges
meta_kms_connector_state_changes (MetaKmsConnectorState *state,
                                  MetaKmsConnectorState *new_state)
{
  if (state->current_crtc_id != new_state->current_crtc_id)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->common_possible_crtcs != new_state->common_possible_crtcs)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->common_possible_clones != new_state->common_possible_clones)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->encoder_device_idxs != new_state->encoder_device_idxs)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->width_mm != new_state->width_mm)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->height_mm != new_state->height_mm)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->has_scaling != new_state->has_scaling)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->non_desktop != new_state->non_desktop)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->subpixel_order != new_state->subpixel_order)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->suggested_x != new_state->suggested_x)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->suggested_y != new_state->suggested_y)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->hotplug_mode_update != new_state->hotplug_mode_update)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->panel_orientation_transform !=
      new_state->panel_orientation_transform)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (!meta_tile_info_equal (&state->tile_info, &new_state->tile_info))
    return META_KMS_RESOURCE_CHANGE_FULL;

  if ((state->edid_data && !new_state->edid_data) || !state->edid_data ||
      !g_bytes_equal (state->edid_data, new_state->edid_data))
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (!kms_modes_equal (state->modes, new_state->modes))
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->max_bpc.value != new_state->max_bpc.value ||
      state->max_bpc.min_value != new_state->max_bpc.min_value ||
      state->max_bpc.max_value != new_state->max_bpc.max_value)
    return META_KMS_RESOURCE_CHANGE_FULL;

  if (state->privacy_screen_state != new_state->privacy_screen_state)
    return META_KMS_RESOURCE_CHANGE_PRIVACY_SCREEN;

  return META_KMS_RESOURCE_CHANGE_NONE;
}

static void
meta_kms_connector_update_state_changes (MetaKmsConnector       *connector,
                                         MetaKmsResourceChanges  changes,
                                         MetaKmsConnectorState  *new_state)
{
  MetaKmsConnectorState *current_state = connector->current_state;

  g_return_if_fail (changes != META_KMS_RESOURCE_CHANGE_FULL);

  if (changes & META_KMS_RESOURCE_CHANGE_PRIVACY_SCREEN)
    current_state->privacy_screen_state = new_state->privacy_screen_state;
}

static MetaKmsResourceChanges
meta_kms_connector_read_state (MetaKmsConnector  *connector,
                               MetaKmsImplDevice *impl_device,
                               drmModeConnector  *drm_connector,
                               drmModeRes        *drm_resources)
{
  g_autoptr (MetaKmsConnectorState) state = NULL;
  g_autoptr (MetaKmsConnectorState) current_state = NULL;
  MetaKmsResourceChanges connector_changes;
  MetaKmsResourceChanges changes;

  current_state = g_steal_pointer (&connector->current_state);
  changes = META_KMS_RESOURCE_CHANGE_NONE;

  meta_kms_impl_device_update_prop_table (impl_device,
                                          drm_connector->props,
                                          drm_connector->prop_values,
                                          drm_connector->count_props,
                                          connector->prop_table.props,
                                          META_KMS_CONNECTOR_N_PROPS);

  if (!drm_connector)
    {
      if (current_state)
        changes = META_KMS_RESOURCE_CHANGE_FULL;
      goto out;
    }

  if (drm_connector->connection != DRM_MODE_CONNECTED)
    {
      if (drm_connector->connection != connector->connection)
        {
          connector->connection = drm_connector->connection;
          changes |= META_KMS_RESOURCE_CHANGE_FULL;
        }

      goto out;
    }

  state = meta_kms_connector_state_new ();

  state_set_blobs (state, connector, impl_device, drm_connector);

  state_set_properties (state, impl_device, connector, drm_connector);

  state->subpixel_order =
    drm_subpixel_order_to_cogl_subpixel_order (drm_connector->subpixel);

  state_set_physical_dimensions (state, drm_connector);

  state_set_modes (state, impl_device, drm_connector);

  state_set_crtc_state (state, drm_connector, impl_device, drm_resources);

  if (drm_connector->connection != connector->connection)
    {
      connector->connection = drm_connector->connection;
      changes |= META_KMS_RESOURCE_CHANGE_FULL;
    }

  if (!current_state)
    connector_changes = META_KMS_RESOURCE_CHANGE_FULL;
  else
    connector_changes = meta_kms_connector_state_changes (current_state, state);

  changes |= connector_changes;

  if (!(changes & META_KMS_RESOURCE_CHANGE_FULL))
    {
      meta_kms_connector_update_state_changes (connector,
                                               connector_changes,
                                               state);
      connector->current_state = g_steal_pointer (&current_state);
    }
  else
    {
      connector->current_state = g_steal_pointer (&state);
    }

out:
  sync_fd_held (connector, impl_device);

  return changes;
}

MetaKmsResourceChanges
meta_kms_connector_update_state (MetaKmsConnector *connector,
                                 drmModeRes       *drm_resources)
{
  MetaKmsImplDevice *impl_device;
  drmModeConnector *drm_connector;
  MetaKmsResourceChanges changes;

  impl_device = meta_kms_device_get_impl_device (connector->device);
  drm_connector = drmModeGetConnector (meta_kms_impl_device_get_fd (impl_device),
                                       connector->id);

  changes = meta_kms_connector_read_state (connector, impl_device,
                                           drm_connector,
                                           drm_resources);
  g_clear_pointer (&drm_connector, drmModeFreeConnector);

  return changes;
}

void
meta_kms_connector_disable (MetaKmsConnector *connector)
{
  MetaKmsConnectorState *current_state;

  current_state = connector->current_state;
  if (!current_state)
    return;

  current_state->current_crtc_id = 0;
}

MetaKmsResourceChanges
meta_kms_connector_predict_state (MetaKmsConnector *connector,
                                  MetaKmsUpdate    *update)
{
  MetaKmsImplDevice *impl_device;
  MetaKmsConnectorState *current_state;
  GList *mode_sets;
  GList *l;
  MetaKmsResourceChanges changes = META_KMS_RESOURCE_CHANGE_NONE;

  current_state = connector->current_state;
  if (!current_state)
    return META_KMS_RESOURCE_CHANGE_NONE;

  mode_sets = meta_kms_update_get_mode_sets (update);
  for (l = mode_sets; l; l = l->next)
    {
      MetaKmsModeSet *mode_set = l->data;
      MetaKmsCrtc *crtc = mode_set->crtc;

      if (current_state->current_crtc_id == meta_kms_crtc_get_id (crtc))
        {
          if (g_list_find (mode_set->connectors, connector))
            break;
          else
            current_state->current_crtc_id = 0;
        }
      else
        {
          if (g_list_find (mode_set->connectors, connector))
            {
              current_state->current_crtc_id = meta_kms_crtc_get_id (crtc);
              break;
            }
        }
    }

  if (has_privacy_screen_software_toggle (connector))
    {
      GList *connector_updates;

      connector_updates = meta_kms_update_get_connector_updates (update);
      for (l = connector_updates; l; l = l->next)
        {
          MetaKmsConnectorUpdate *connector_update = l->data;

          if (connector_update->connector != connector)
            continue;

          if (connector_update->privacy_screen.has_update &&
              !(current_state->privacy_screen_state &
                META_PRIVACY_SCREEN_LOCKED))
            {
              if (connector_update->privacy_screen.is_enabled)
                {
                  if (current_state->privacy_screen_state !=
                      META_PRIVACY_SCREEN_ENABLED)
                    changes |= META_KMS_RESOURCE_CHANGE_PRIVACY_SCREEN;

                  current_state->privacy_screen_state =
                    META_PRIVACY_SCREEN_ENABLED;
                }
              else
                {
                  if (current_state->privacy_screen_state !=
                      META_PRIVACY_SCREEN_DISABLED)
                    changes |= META_KMS_RESOURCE_CHANGE_PRIVACY_SCREEN;

                  current_state->privacy_screen_state =
                    META_PRIVACY_SCREEN_DISABLED;
                }
            }
        }
    }

  impl_device = meta_kms_device_get_impl_device (connector->device);
  sync_fd_held (connector, impl_device);

  return changes;
}

static void
init_properties (MetaKmsConnector  *connector,
                 MetaKmsImplDevice *impl_device,
                 drmModeConnector  *drm_connector)
{
  MetaKmsConnectorPropTable *prop_table = &connector->prop_table;

  *prop_table = (MetaKmsConnectorPropTable) {
    .props = {
      [META_KMS_CONNECTOR_PROP_CRTC_ID] =
        {
          .name = "CRTC_ID",
          .type = DRM_MODE_PROP_OBJECT,
        },
      [META_KMS_CONNECTOR_PROP_DPMS] =
        {
          .name = "DPMS",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->dpms_enum,
          .num_enum_values = META_KMS_CONNECTOR_DPMS_N_PROPS,
        },
      [META_KMS_CONNECTOR_PROP_UNDERSCAN] =
        {
          .name = "underscan",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->underscan_enum,
          .num_enum_values = META_KMS_CONNECTOR_UNDERSCAN_N_PROPS,
          .default_value = META_KMS_CONNECTOR_UNDERSCAN_UNKNOWN,
        },
      [META_KMS_CONNECTOR_PROP_UNDERSCAN_HBORDER] =
        {
          .name = "underscan hborder",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_UNDERSCAN_VBORDER] =
        {
          .name = "underscan vborder",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_PRIVACY_SCREEN_SW_STATE] =
        {
          .name = "privacy-screen sw-state",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->privacy_screen_sw_enum,
          .num_enum_values = META_KMS_CONNECTOR_PRIVACY_SCREEN_N_PROPS,
          .default_value = META_KMS_CONNECTOR_PRIVACY_SCREEN_UNKNOWN,
        },
      [META_KMS_CONNECTOR_PROP_PRIVACY_SCREEN_HW_STATE] =
        {
          .name = "privacy-screen hw-state",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->privacy_screen_hw_enum,
          .num_enum_values = META_KMS_CONNECTOR_PRIVACY_SCREEN_N_PROPS,
          .default_value = META_KMS_CONNECTOR_PRIVACY_SCREEN_UNKNOWN,
        },
      [META_KMS_CONNECTOR_PROP_EDID] =
        {
          .name = "EDID",
          .type = DRM_MODE_PROP_BLOB,
        },
      [META_KMS_CONNECTOR_PROP_TILE] =
        {
          .name = "TILE",
          .type = DRM_MODE_PROP_BLOB,
        },
      [META_KMS_CONNECTOR_PROP_SUGGESTED_X] =
        {
          .name = "suggested X",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_SUGGESTED_Y] =
        {
          .name = "suggested Y",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_HOTPLUG_MODE_UPDATE] =
        {
          .name = "hotplug_mode_update",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_SCALING_MODE] =
        {
          .name = "scaling mode",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->scaling_mode_enum,
          .num_enum_values = META_KMS_CONNECTOR_SCALING_MODE_N_PROPS,
          .default_value = META_KMS_CONNECTOR_SCALING_MODE_UNKNOWN,
        },
      [META_KMS_CONNECTOR_PROP_PANEL_ORIENTATION] =
        {
          .name = "panel orientation",
          .type = DRM_MODE_PROP_ENUM,
          .enum_values = prop_table->panel_orientation_enum,
          .num_enum_values = META_KMS_CONNECTOR_PANEL_ORIENTATION_N_PROPS,
          .default_value = META_KMS_CONNECTOR_PANEL_ORIENTATION_UNKNOWN,
        },
      [META_KMS_CONNECTOR_PROP_NON_DESKTOP] =
        {
          .name = "non-desktop",
          .type = DRM_MODE_PROP_RANGE,
        },
      [META_KMS_CONNECTOR_PROP_MAX_BPC] =
        {
          .name = "max bpc",
          .type = DRM_MODE_PROP_RANGE,
        },
    },
    .dpms_enum = {
      [META_KMS_CONNECTOR_DPMS_ON] =
        {
          .name = "On",
        },
      [META_KMS_CONNECTOR_DPMS_STANDBY] =
        {
          .name = "Standby",
        },
      [META_KMS_CONNECTOR_DPMS_SUSPEND] =
        {
          .name = "Suspend",
        },
      [META_KMS_CONNECTOR_DPMS_OFF] =
        {
          .name = "Off",
        },
    },
    .underscan_enum = {
      [META_KMS_CONNECTOR_UNDERSCAN_OFF] =
        {
          .name = "off",
        },
      [META_KMS_CONNECTOR_UNDERSCAN_ON] =
        {
          .name = "on",
        },
      [META_KMS_CONNECTOR_UNDERSCAN_AUTO] =
        {
          .name = "auto",
        },
    },
    .privacy_screen_sw_enum = {
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_ENABLED] =
        {
          .name = "Enabled",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_DISABLED] =
        {
          .name = "Disabled",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_ENABLED_LOCKED] =
        {
          .name = "Enabled-locked",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_DISABLED_LOCKED] =
        {
          .name = "Disabled-locked",
        },
    },
    .privacy_screen_hw_enum = {
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_ENABLED] =
        {
          .name = "Enabled",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_DISABLED] =
        {
          .name = "Disabled",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_ENABLED_LOCKED] =
        {
          .name = "Enabled-locked",
        },
      [META_KMS_CONNECTOR_PRIVACY_SCREEN_DISABLED_LOCKED] =
        {
          .name = "Disabled-locked",
        },
    },
    .scaling_mode_enum = {
      [META_KMS_CONNECTOR_SCALING_MODE_NONE] =
        {
          .name = "None",
        },
      [META_KMS_CONNECTOR_SCALING_MODE_FULL] =
        {
          .name = "Full",
        },
      [META_KMS_CONNECTOR_SCALING_MODE_CENTER] =
        {
          .name = "Center",
        },
      [META_KMS_CONNECTOR_SCALING_MODE_FULL_ASPECT] =
        {
          .name = "Full aspect",
        },
    },
    .panel_orientation_enum = {
      [META_KMS_CONNECTOR_PANEL_ORIENTATION_NORMAL] =
        {
          .name = "Normal",
        },
      [META_KMS_CONNECTOR_PANEL_ORIENTATION_UPSIDE_DOWN] =
        {
          .name = "Upside Down",
        },
      [META_KMS_CONNECTOR_PANEL_ORIENTATION_LEFT_SIDE_UP] =
        {
          .name = "Left Side Up",
        },
      [META_KMS_CONNECTOR_PANEL_ORIENTATION_RIGHT_SIDE_UP] =
        {
          .name = "Right Side Up",
        },
    }
  };
}

static char *
make_connector_name (drmModeConnector *drm_connector)
{
  static const char * const connector_type_names[] = {
    "None",
    "VGA",
    "DVI-I",
    "DVI-D",
    "DVI-A",
    "Composite",
    "SVIDEO",
    "LVDS",
    "Component",
    "DIN",
    "DP",
    "HDMI",
    "HDMI-B",
    "TV",
    "eDP",
    "Virtual",
    "DSI",
  };

  if (drm_connector->connector_type < G_N_ELEMENTS (connector_type_names))
    return g_strdup_printf ("%s-%d",
                            connector_type_names[drm_connector->connector_type],
                            drm_connector->connector_type_id);
  else
    return g_strdup_printf ("Unknown%d-%d",
                            drm_connector->connector_type,
                            drm_connector->connector_type_id);
}

gboolean
meta_kms_connector_is_same_as (MetaKmsConnector *connector,
                               drmModeConnector *drm_connector)
{
  return (connector->id == drm_connector->connector_id &&
          connector->type == drm_connector->connector_type &&
          connector->type_id == drm_connector->connector_type_id);
}

MetaKmsConnector *
meta_kms_connector_new (MetaKmsImplDevice *impl_device,
                        drmModeConnector  *drm_connector,
                        drmModeRes        *drm_resources)
{
  MetaKmsConnector *connector;

  g_assert (drm_connector);
  connector = g_object_new (META_TYPE_KMS_CONNECTOR, NULL);
  connector->device = meta_kms_impl_device_get_device (impl_device);
  connector->id = drm_connector->connector_id;
  connector->type = drm_connector->connector_type;
  connector->type_id = drm_connector->connector_type_id;
  connector->name = make_connector_name (drm_connector);

  init_properties (connector, impl_device, drm_connector);

  meta_kms_connector_read_state (connector, impl_device,
                                 drm_connector,
                                 drm_resources);

  return connector;
}

static void
meta_kms_connector_finalize (GObject *object)
{
  MetaKmsConnector *connector = META_KMS_CONNECTOR (object);

  if (connector->fd_held)
    {
      MetaKmsImplDevice *impl_device;

      impl_device = meta_kms_device_get_impl_device (connector->device);
      meta_kms_impl_device_unhold_fd (impl_device);
    }

  g_clear_pointer (&connector->current_state, meta_kms_connector_state_free);
  g_free (connector->name);

  G_OBJECT_CLASS (meta_kms_connector_parent_class)->finalize (object);
}

static void
meta_kms_connector_init (MetaKmsConnector *connector)
{
}

static void
meta_kms_connector_class_init (MetaKmsConnectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_kms_connector_finalize;
}
