/*
 * Copyright (C) 2021 Red Hat Inc.
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
 *
 */

#include "config.h"

#include <dlfcn.h>

#include "backends/native/meta-kms-connector.h"
#include "backends/native/meta-kms-crtc.h"
#include "backends/native/meta-kms-device.h"
#include "backends/native/meta-kms-mode.h"
#include "backends/native/meta-kms-update-private.h"
#include "backends/native/meta-kms.h"
#include "meta-test/meta-context-test.h"
#include "tests/meta-kms-test-utils.h"

static MetaContext *test_context;

const MetaKmsCrtcState *
meta_kms_crtc_get_current_state (MetaKmsCrtc *crtc)
{
  static MetaKmsCrtcState mock_state;
  static const MetaKmsCrtcState *
    (* real_get_current_state) (MetaKmsCrtc *crtc) = NULL;
  const MetaKmsCrtcState *state;

  if (!real_get_current_state)
    real_get_current_state = dlsym (RTLD_NEXT, __func__);

  state = real_get_current_state (crtc);
  if (!state)
    return NULL;

  mock_state = *state;
  mock_state.gamma.size = 3;

  return &mock_state;
}

static void
meta_test_kms_update_sanity (void)
{
  MetaKmsDevice *device;
  MetaKmsCrtc *crtc;
  MetaKmsUpdate *update;

  device = meta_get_test_kms_device (test_context);
  crtc = meta_get_test_kms_crtc (device);

  update = meta_kms_update_new (device);
  g_assert (meta_kms_update_get_device (update) == device);
  g_assert_false (meta_kms_update_is_sealed (update));
  g_assert_null (meta_kms_update_get_primary_plane_assignment (update, crtc));
  g_assert_null (meta_kms_update_get_plane_assignments (update));
  g_assert_null (meta_kms_update_get_mode_sets (update));
  g_assert_null (meta_kms_update_get_page_flip_listeners (update));
  g_assert_null (meta_kms_update_get_connector_updates (update));
  g_assert_null (meta_kms_update_get_crtc_color_updates (update));
  meta_kms_update_free (update);
}

static void
meta_test_kms_update_plane_assignments (void)
{
  MetaKmsDevice *device;
  MetaKmsUpdate *update;
  MetaKmsCrtc *crtc;
  MetaKmsConnector *connector;
  MetaKmsPlane *primary_plane;
  MetaKmsPlane *cursor_plane;
  MetaKmsMode *mode;
  int mode_width, mode_height;
  g_autoptr (MetaDrmBuffer) primary_buffer = NULL;
  g_autoptr (MetaDrmBuffer) cursor_buffer = NULL;
  MetaKmsPlaneAssignment *primary_plane_assignment;
  MetaKmsPlaneAssignment *cursor_plane_assignment;
  GList *plane_assignments;

  device = meta_get_test_kms_device (test_context);
  update = meta_kms_update_new (device);
  crtc = meta_get_test_kms_crtc (device);
  connector = meta_get_test_kms_connector (device);

  primary_plane = meta_kms_device_get_primary_plane_for (device, crtc);
  g_assert_nonnull (primary_plane);

  cursor_plane = meta_kms_device_get_cursor_plane_for (device, crtc);
  g_assert_nonnull (cursor_plane);

  mode = meta_kms_connector_get_preferred_mode (connector);

  mode_width = meta_kms_mode_get_width (mode);
  mode_height = meta_kms_mode_get_height (mode);
  primary_buffer = meta_create_test_mode_dumb_buffer (device, mode);

  primary_plane_assignment =
    meta_kms_update_assign_plane (update,
                                  crtc,
                                  primary_plane,
                                  primary_buffer,
                                  meta_get_mode_fixed_rect_16 (mode),
                                  meta_get_mode_rect (mode),
                                  META_KMS_ASSIGN_PLANE_FLAG_NONE);
  g_assert_nonnull (primary_plane_assignment);
  g_assert_cmpint (primary_plane_assignment->src_rect.x, ==, 0);
  g_assert_cmpint (primary_plane_assignment->src_rect.y, ==, 0);
  g_assert_cmpint (primary_plane_assignment->src_rect.width,
                   ==,
                   meta_fixed_16_from_int (mode_width));
  g_assert_cmpint (primary_plane_assignment->src_rect.height,
                   ==,
                   meta_fixed_16_from_int (mode_height));
  g_assert_cmpint (primary_plane_assignment->dst_rect.x, ==, 0);
  g_assert_cmpint (primary_plane_assignment->dst_rect.y, ==, 0);
  g_assert_cmpint (primary_plane_assignment->dst_rect.width, ==, mode_width);
  g_assert_cmpint (primary_plane_assignment->dst_rect.height, ==, mode_height);

  cursor_buffer = meta_create_test_dumb_buffer (device, 64, 64);

  cursor_plane_assignment =
    meta_kms_update_assign_plane (update,
                                  crtc,
                                  cursor_plane,
                                  cursor_buffer,
                                  META_FIXED_16_RECTANGLE_INIT_INT (0, 0, 64, 64),
                                  META_RECTANGLE_INIT (24, 48, 64, 64),
                                  META_KMS_ASSIGN_PLANE_FLAG_NONE);
  g_assert_nonnull (cursor_plane_assignment);
  g_assert_cmpint (cursor_plane_assignment->src_rect.x, ==, 0);
  g_assert_cmpint (cursor_plane_assignment->src_rect.y, ==, 0);
  g_assert_cmpint (cursor_plane_assignment->src_rect.width,
                   ==,
                   meta_fixed_16_from_int (64));
  g_assert_cmpint (cursor_plane_assignment->src_rect.height,
                   ==,
                   meta_fixed_16_from_int (64));
  g_assert_cmpint (cursor_plane_assignment->dst_rect.x, ==, 24);
  g_assert_cmpint (cursor_plane_assignment->dst_rect.y, ==, 48);
  g_assert_cmpint (cursor_plane_assignment->dst_rect.width, ==, 64);
  g_assert_cmpint (cursor_plane_assignment->dst_rect.height, ==, 64);

  meta_kms_plane_assignment_set_cursor_hotspot (cursor_plane_assignment,
                                                10, 11);

  g_assert (meta_kms_update_get_primary_plane_assignment (update, crtc) ==
            primary_plane_assignment);

  g_assert (primary_plane_assignment->crtc == crtc);
  g_assert (primary_plane_assignment->update == update);
  g_assert (primary_plane_assignment->plane == primary_plane);
  g_assert (primary_plane_assignment->buffer == primary_buffer);
  g_assert_cmpuint (primary_plane_assignment->rotation, ==, 0);
  g_assert_false (primary_plane_assignment->cursor_hotspot.is_valid);

  g_assert (meta_kms_update_get_cursor_plane_assignment (update, crtc) ==
            cursor_plane_assignment);

  g_assert (cursor_plane_assignment->crtc == crtc);
  g_assert (cursor_plane_assignment->update == update);
  g_assert (cursor_plane_assignment->plane == cursor_plane);
  g_assert (cursor_plane_assignment->buffer == cursor_buffer);
  g_assert_cmpuint (cursor_plane_assignment->rotation, ==, 0);
  g_assert_true (cursor_plane_assignment->cursor_hotspot.is_valid);
  g_assert_cmpint (cursor_plane_assignment->cursor_hotspot.x, ==, 10);
  g_assert_cmpint (cursor_plane_assignment->cursor_hotspot.y, ==, 11);

  plane_assignments = meta_kms_update_get_plane_assignments (update);
  g_assert_cmpuint (g_list_length (plane_assignments), ==, 2);

  g_assert_nonnull (g_list_find (plane_assignments, primary_plane_assignment));
  g_assert_nonnull (g_list_find (plane_assignments, cursor_plane_assignment));

  meta_kms_update_free (update);
}

static void
meta_test_kms_update_fixed16 (void)
{
  MetaFixed16Rectangle rect16;

  g_assert_cmpint (meta_fixed_16_from_int (12345), ==, 809041920);
  g_assert_cmpint (meta_fixed_16_to_int (809041920), ==, 12345);
  g_assert_cmpint (meta_fixed_16_from_int (-12345), ==, -809041920);
  g_assert_cmpint (meta_fixed_16_to_int (-809041920), ==, -12345);

  rect16 = META_FIXED_16_RECTANGLE_INIT_INT (100, 200, 300, 400);
  g_assert_cmpint (rect16.x, ==, 6553600);
  g_assert_cmpint (rect16.y, ==, 13107200);
  g_assert_cmpint (rect16.width, ==, 19660800);
  g_assert_cmpint (rect16.height, ==, 26214400);
}

static void
meta_test_kms_update_mode_sets (void)
{
  MetaKmsDevice *device;
  MetaKmsUpdate *update;
  MetaKmsCrtc *crtc;
  MetaKmsConnector *connector;
  MetaKmsMode *mode;
  GList *mode_sets;
  MetaKmsModeSet *mode_set;

  device = meta_get_test_kms_device (test_context);
  update = meta_kms_update_new (device);
  crtc = meta_get_test_kms_crtc (device);
  connector = meta_get_test_kms_connector (device);
  mode = meta_kms_connector_get_preferred_mode (connector);

  meta_kms_update_mode_set (update, crtc,
                            g_list_append (NULL, connector),
                            mode);

  mode_sets = meta_kms_update_get_mode_sets (update);
  g_assert_cmpuint (g_list_length (mode_sets), ==, 1);
  mode_set = mode_sets->data;

  g_assert (mode_set->crtc == crtc);
  g_assert_cmpuint (g_list_length (mode_set->connectors), ==, 1);
  g_assert (mode_set->connectors->data == connector);
  g_assert (mode_set->mode == mode);

  meta_kms_update_free (update);
}

typedef enum _PageFlipState
{
  INIT,
  PAGE_FLIPPED,
  DESTROYED,
} PageFlipState;

typedef struct _PageFlipData
{
  GMainLoop *loop;
  GThread *thread;

  PageFlipState state;
} PageFlipData;

static void
page_flip_feedback_flipped (MetaKmsCrtc  *kms_crtc,
                            unsigned int  sequence,
                            unsigned int  tv_sec,
                            unsigned int  tv_usec,
                            gpointer      user_data)
{
  PageFlipData *data = user_data;

  g_assert (data->thread == g_thread_self ());
  g_assert_cmpint (data->state, ==, INIT);
  data->state = PAGE_FLIPPED;
}

static void
page_flip_feedback_ready (MetaKmsCrtc *kms_crtc,
                          gpointer     user_data)
{
  g_assert_not_reached ();
}

static void
page_flip_feedback_mode_set_fallback (MetaKmsCrtc *kms_crtc,
                                      gpointer     user_data)
{
  g_assert_not_reached ();
}

static void
page_flip_feedback_discarded (MetaKmsCrtc  *kms_crtc,
                              gpointer      user_data,
                              const GError *error)
{
  g_assert_not_reached ();
}

static const MetaKmsPageFlipListenerVtable page_flip_listener_vtable = {
  .flipped = page_flip_feedback_flipped,
  .ready = page_flip_feedback_ready,
  .mode_set_fallback = page_flip_feedback_mode_set_fallback,
  .discarded = page_flip_feedback_discarded,
};

static void
page_flip_data_destroy (gpointer user_data)
{
  PageFlipData *data = user_data;

  g_assert (data->thread == g_thread_self ());
  g_assert_cmpint (data->state, ==, PAGE_FLIPPED);
  data->state = DESTROYED;

  g_main_loop_quit (data->loop);
}

static void
meta_test_kms_update_page_flip (void)
{
  MetaKmsDevice *device;
  MetaKmsUpdate *update;
  MetaKmsCrtc *crtc;
  MetaKmsConnector *connector;
  MetaKmsMode *mode;
  g_autoptr (MetaDrmBuffer) primary_buffer1 = NULL;
  g_autoptr (MetaDrmBuffer) primary_buffer2 = NULL;
  MetaKmsPlane *primary_plane;
  PageFlipData data = {};
  MetaKmsFeedback *feedback;

  device = meta_get_test_kms_device (test_context);
  crtc = meta_get_test_kms_crtc (device);
  connector = meta_get_test_kms_connector (device);
  mode = meta_kms_connector_get_preferred_mode (connector);

  update = meta_kms_update_new (device);

  meta_kms_update_mode_set (update, crtc,
                            g_list_append (NULL, connector),
                            mode);

  primary_buffer1 = meta_create_test_mode_dumb_buffer (device, mode);

  primary_plane = meta_kms_device_get_primary_plane_for (device, crtc);
  meta_kms_update_assign_plane (update,
                                crtc,
                                primary_plane,
                                primary_buffer1,
                                meta_get_mode_fixed_rect_16 (mode),
                                meta_get_mode_rect (mode),
                                META_KMS_ASSIGN_PLANE_FLAG_NONE);

  data.loop = g_main_loop_new (NULL, FALSE);
  data.thread = g_thread_self ();
  meta_kms_update_add_page_flip_listener (update, crtc,
                                          &page_flip_listener_vtable,
                                          META_KMS_PAGE_FLIP_LISTENER_FLAG_NONE,
                                          &data,
                                          page_flip_data_destroy);

  feedback =
    meta_kms_device_process_update_sync (device, update,
                                         META_KMS_UPDATE_FLAG_NONE);
  meta_kms_feedback_unref (feedback);

  g_main_loop_run (data.loop);
  g_assert_cmpint (data.state, ==, DESTROYED);

  data.state = INIT;
  update = meta_kms_update_new (device);
  primary_buffer2 = meta_create_test_mode_dumb_buffer (device, mode);
  meta_kms_update_assign_plane (update,
                                crtc,
                                primary_plane,
                                primary_buffer2,
                                meta_get_mode_fixed_rect_16 (mode),
                                meta_get_mode_rect (mode),
                                META_KMS_ASSIGN_PLANE_FLAG_NONE);
  meta_kms_update_add_page_flip_listener (update, crtc,
                                          &page_flip_listener_vtable,
                                          META_KMS_PAGE_FLIP_LISTENER_FLAG_NONE,
                                          &data,
                                          page_flip_data_destroy);

  feedback =
    meta_kms_device_process_update_sync (device, update,
                                         META_KMS_UPDATE_FLAG_NONE);
  meta_kms_feedback_unref (feedback);

  g_main_loop_run (data.loop);
  g_assert_cmpint (data.state, ==, DESTROYED);

  g_main_loop_unref (data.loop);
}

static void
meta_test_kms_update_merge (void)
{
  MetaKmsDevice *device;
  MetaKmsUpdate *update1;
  MetaKmsCrtc *crtc;
  MetaKmsConnector *connector;
  MetaKmsPlane *primary_plane;
  MetaKmsPlane *cursor_plane;
  MetaKmsMode *mode;
  int mode_width, mode_height;
  g_autoptr (MetaDrmBuffer) primary_buffer1 = NULL;
  g_autoptr (MetaDrmBuffer) cursor_buffer1 = NULL;
  MetaKmsPlaneAssignment *cursor_plane_assignment;
  MetaKmsUpdate *update2;
  g_autoptr (MetaGammaLut) lut = NULL;
  g_autoptr (MetaDrmBuffer) cursor_buffer2 = NULL;
  GList *mode_sets;
  MetaKmsModeSet *mode_set;
  GList *plane_assignments;
  MetaKmsPlaneAssignment *plane_assignment;
  GList *crtc_color_updates;
  MetaKmsCrtcColorUpdate *crtc_color_update;
  MetaGammaLut *crtc_gamma;
  GList *connector_updates;
  MetaKmsConnectorUpdate *connector_update;

  device = meta_get_test_kms_device (test_context);
  crtc = meta_get_test_kms_crtc (device);
  connector = meta_get_test_kms_connector (device);
  primary_plane = meta_kms_device_get_primary_plane_for (device, crtc);
  cursor_plane = meta_kms_device_get_cursor_plane_for (device, crtc);

  mode = meta_kms_connector_get_preferred_mode (connector);

  /*
   * Create an update1 with buffer 1 on the primary plane, and cursor buffer 1
   * on the cursor plane at at (24, 48)
   */

  update1 = meta_kms_update_new (device);

  mode_width = meta_kms_mode_get_width (mode);
  mode_height = meta_kms_mode_get_height (mode);
  primary_buffer1 = meta_create_test_mode_dumb_buffer (device, mode);

  meta_kms_update_assign_plane (update1,
                                crtc,
                                primary_plane,
                                primary_buffer1,
                                meta_get_mode_fixed_rect_16 (mode),
                                meta_get_mode_rect (mode),
                                META_KMS_ASSIGN_PLANE_FLAG_NONE);

  cursor_buffer1 = meta_create_test_dumb_buffer (device, 64, 64);

  cursor_plane_assignment =
    meta_kms_update_assign_plane (update1,
                                  crtc,
                                  cursor_plane,
                                  cursor_buffer1,
                                  META_FIXED_16_RECTANGLE_INIT_INT (0, 0,
                                                                    64, 64),
                                  META_RECTANGLE_INIT (24, 48, 64, 64),
                                  META_KMS_ASSIGN_PLANE_FLAG_NONE);
  meta_kms_plane_assignment_set_cursor_hotspot (cursor_plane_assignment,
                                                10, 11);

  meta_kms_update_set_underscanning (update1,
                                     connector,
                                     123, 456);
  meta_kms_update_set_privacy_screen (update1, connector, TRUE);

  /*
   * Create an update2 with a mode set and a cursor buffer 2
   * on the cursor plane at at (32, 56), and a new CRTC gamma.
   */

  update2 = meta_kms_update_new (device);

  meta_kms_update_mode_set (update2,
                            crtc,
                            g_list_append (NULL, connector),
                            mode);

  lut = meta_gamma_lut_new (3,
                            (uint16_t[]) { 1, 2, 3 },
                            (uint16_t[]) { 4, 5, 6 },
                            (uint16_t[]) { 7, 8, 9 });
  meta_kms_update_set_crtc_gamma (update2, crtc, lut);

  cursor_buffer2 = meta_create_test_dumb_buffer (device, 64, 64);
  cursor_plane_assignment =
    meta_kms_update_assign_plane (update2,
                                  crtc,
                                  cursor_plane,
                                  cursor_buffer2,
                                  META_FIXED_16_RECTANGLE_INIT_INT (0, 0,
                                                                    64, 64),
                                  META_RECTANGLE_INIT (32, 56, 64, 64),
                                  META_KMS_ASSIGN_PLANE_FLAG_NONE);
  meta_kms_plane_assignment_set_cursor_hotspot (cursor_plane_assignment,
                                                9, 7);

  meta_kms_update_set_privacy_screen (update2, connector, FALSE);
  meta_kms_update_set_max_bpc (update2, connector, 8);

  /*
   * Merge and check result.
   */

  meta_kms_update_merge_from (update1, update2);
  meta_kms_update_free (update2);

  mode_sets = meta_kms_update_get_mode_sets (update1);
  g_assert_cmpuint (g_list_length (mode_sets), ==, 1);
  mode_set = mode_sets->data;
  g_assert (mode_set->crtc == crtc);
  g_assert (mode_set->mode == mode);
  g_assert_cmpuint (g_list_length (mode_set->connectors), ==, 1);
  g_assert (mode_set->connectors->data == connector);

  plane_assignments = meta_kms_update_get_plane_assignments (update1);
  g_assert_cmpuint (g_list_length (plane_assignments), ==, 2);
  plane_assignment = meta_kms_update_get_primary_plane_assignment (update1,
                                                                   crtc);
  g_assert_nonnull (plane_assignment);
  g_assert (plane_assignment->update == update1);
  g_assert (plane_assignment->crtc == crtc);
  g_assert (plane_assignment->plane == primary_plane);
  g_assert (plane_assignment->buffer == primary_buffer1);
  g_assert_false (plane_assignment->cursor_hotspot.is_valid);
  g_assert_cmpint (plane_assignment->src_rect.x, ==, 0);
  g_assert_cmpint (plane_assignment->src_rect.y, ==, 0);
  g_assert_cmpint (plane_assignment->src_rect.width,
                   ==,
                   meta_fixed_16_from_int (mode_width));
  g_assert_cmpint (plane_assignment->src_rect.height,
                   ==,
                   meta_fixed_16_from_int (mode_height));
  g_assert_cmpint (plane_assignment->dst_rect.x, ==, 0);
  g_assert_cmpint (plane_assignment->dst_rect.y, ==, 0);
  g_assert_cmpint (plane_assignment->dst_rect.width, ==, mode_width);
  g_assert_cmpint (plane_assignment->dst_rect.height, ==, mode_height);

  plane_assignment = meta_kms_update_get_cursor_plane_assignment (update1,
                                                                  crtc);
  g_assert_nonnull (plane_assignment);
  g_assert (plane_assignment->update == update1);
  g_assert (plane_assignment->crtc == crtc);
  g_assert (plane_assignment->plane == cursor_plane);
  g_assert (plane_assignment->buffer == META_DRM_BUFFER (cursor_buffer2));
  g_assert_true (plane_assignment->cursor_hotspot.is_valid);
  g_assert_cmpint (plane_assignment->cursor_hotspot.x, ==, 9);
  g_assert_cmpint (plane_assignment->cursor_hotspot.y, ==, 7);
  g_assert_cmpint (plane_assignment->src_rect.x, ==, 0);
  g_assert_cmpint (plane_assignment->src_rect.y, ==, 0);
  g_assert_cmpint (plane_assignment->src_rect.width,
                   ==,
                   meta_fixed_16_from_int (64));
  g_assert_cmpint (plane_assignment->src_rect.height,
                   ==,
                   meta_fixed_16_from_int (64));
  g_assert_cmpint (plane_assignment->dst_rect.x, ==, 32);
  g_assert_cmpint (plane_assignment->dst_rect.y, ==, 56);
  g_assert_cmpint (plane_assignment->dst_rect.width, ==, 64);
  g_assert_cmpint (plane_assignment->dst_rect.height, ==, 64);

  crtc_color_updates = meta_kms_update_get_crtc_color_updates (update1);
  g_assert_cmpuint (g_list_length (crtc_color_updates), ==, 1);
  crtc_color_update = crtc_color_updates->data;
  crtc_gamma = crtc_color_update->gamma.state;
  g_assert_nonnull (crtc_gamma);

  g_assert_nonnull (crtc_gamma);
  g_assert_cmpint (crtc_gamma->size, ==, 3);
  g_assert_cmpuint (crtc_gamma->red[0], ==, 1);
  g_assert_cmpuint (crtc_gamma->red[1], ==, 2);
  g_assert_cmpuint (crtc_gamma->red[2], ==, 3);
  g_assert_cmpuint (crtc_gamma->green[0], ==, 4);
  g_assert_cmpuint (crtc_gamma->green[1], ==, 5);
  g_assert_cmpuint (crtc_gamma->green[2], ==, 6);
  g_assert_cmpuint (crtc_gamma->blue[0], ==, 7);
  g_assert_cmpuint (crtc_gamma->blue[1], ==, 8);
  g_assert_cmpuint (crtc_gamma->blue[2], ==, 9);

  connector_updates = meta_kms_update_get_connector_updates (update1);
  g_assert_cmpuint (g_list_length (connector_updates), ==, 1);
  connector_update = connector_updates->data;
  g_assert_nonnull (connector_update);

  g_assert_true (connector_update->underscanning.has_update);
  g_assert_true (connector_update->underscanning.is_active);
  g_assert_cmpuint (connector_update->underscanning.hborder, ==, 123);
  g_assert_cmpuint (connector_update->underscanning.vborder, ==, 456);

  g_assert_true (connector_update->privacy_screen.has_update);
  g_assert_false (connector_update->privacy_screen.is_enabled);

  g_assert_true (connector_update->max_bpc.has_update);
  g_assert_cmpuint (connector_update->max_bpc.value, ==, 8);

  meta_kms_update_free (update1);
}

static void
init_tests (void)
{
  g_test_add_func ("/backends/native/kms/update/sanity",
                   meta_test_kms_update_sanity);
  g_test_add_func ("/backends/native/kms/update/fixed16",
                   meta_test_kms_update_fixed16);
  g_test_add_func ("/backends/native/kms/update/plane-assignments",
                   meta_test_kms_update_plane_assignments);
  g_test_add_func ("/backends/native/kms/update/mode-sets",
                   meta_test_kms_update_mode_sets);
  g_test_add_func ("/backends/native/kms/update/page-flip",
                   meta_test_kms_update_page_flip);
  g_test_add_func ("/backends/native/kms/update/merge",
                   meta_test_kms_update_merge);
}

int
main (int    argc,
      char **argv)
{
  g_autoptr (MetaContext) context = NULL;
  g_autoptr (GError) error = NULL;

  context = test_context =
    meta_create_test_context (META_CONTEXT_TEST_TYPE_VKMS,
                              META_CONTEXT_TEST_FLAG_NO_X11);
  g_assert (meta_context_configure (context, &argc, &argv, NULL));

  init_tests ();

  return meta_context_test_run_tests (META_CONTEXT_TEST (context),
                                      META_TEST_RUN_FLAG_CAN_SKIP);
}
