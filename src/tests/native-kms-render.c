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

#include <xf86drmMode.h>

#include "backends/native/meta-backend-native-private.h"
#include "backends/native/meta-crtc-kms.h"
#include "backends/native/meta-device-pool.h"
#include "backends/native/meta-drm-buffer.h"
#include "backends/native/meta-onscreen-native.h"
#include "backends/native/meta-kms.h"
#include "backends/native/meta-kms-device.h"
#include "core/display-private.h"
#include "meta/meta-backend.h"
#include "meta-test/meta-context-test.h"
#include "tests/meta-wayland-test-driver.h"
#include "tests/meta-wayland-test-utils.h"

#define N_CLIENT_SCANOUTS 10

typedef struct
{
  int number_of_frames_left;
  GMainLoop *loop;

  struct {
    int n_paints;
    int n_presentations;
    GList *fb_ids;
  } scanout;
} KmsRenderingTest;

static MetaContext *test_context;

static void
on_after_update (ClutterStage     *stage,
                 ClutterStageView *stage_view,
                 KmsRenderingTest *test)
{
  test->number_of_frames_left--;
  if (test->number_of_frames_left == 0)
    g_main_loop_quit (test->loop);
  else
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

static void
meta_test_kms_render_basic (void)
{
  MetaBackend *backend = meta_context_get_backend (test_context);
  ClutterActor *stage = meta_backend_get_stage (backend);
  KmsRenderingTest test;
  gulong handler_id;

  test = (KmsRenderingTest) {
    .number_of_frames_left = 10,
    .loop = g_main_loop_new (NULL, FALSE),
  };
  handler_id = g_signal_connect (stage, "after-update",
                                 G_CALLBACK (on_after_update), &test);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
  g_main_loop_run (test.loop);
  g_main_loop_unref (test.loop);

  g_assert_cmpint (test.number_of_frames_left, ==, 0);

  g_signal_handler_disconnect (stage, handler_id);
}

static void
on_scanout_before_update (ClutterStage     *stage,
                          ClutterStageView *stage_view,
                          KmsRenderingTest *test)
{
  test->scanout.n_paints = 0;
}

static void
on_scanout_before_paint (ClutterStage     *stage,
                         ClutterStageView *stage_view,
                         KmsRenderingTest *test)
{
  CoglScanout *scanout;
  MetaDrmBuffer *buffer;
  uint32_t fb_id;

  scanout = clutter_stage_view_peek_scanout (stage_view);
  if (!scanout)
    return;

  g_assert_true (META_IS_DRM_BUFFER (scanout));
  buffer = META_DRM_BUFFER (scanout);

  fb_id = meta_drm_buffer_get_fb_id (buffer);
  g_assert_cmpuint (fb_id, >, 0);
  test->scanout.fb_ids = g_list_append (test->scanout.fb_ids,
                                        GUINT_TO_POINTER (fb_id));

  /* Triple buffering, but no higher */
  g_assert_cmpuint (g_list_length (test->scanout.fb_ids), <=, 2);
}

static void
on_scanout_paint_view (ClutterStage     *stage,
                       ClutterStageView *stage_view,
                       KmsRenderingTest *test)
{
  test->scanout.n_paints++;
}

static void
on_scanout_presented (ClutterStage     *stage,
                      ClutterStageView *stage_view,
                      ClutterFrameInfo *frame_info,
                      KmsRenderingTest *test)
{
  MetaBackend *backend = meta_context_get_backend (test_context);
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaDevicePool *device_pool;
  CoglFramebuffer *fb;
  MetaCrtc *crtc;
  MetaKmsCrtc *kms_crtc;
  MetaKmsDevice *kms_device;
  MetaDeviceFile *device_file;
  GError *error = NULL;
  drmModeCrtc *drm_crtc;
  uint32_t first_fb_id_expected;

  if (test->scanout.fb_ids == NULL)
    return;

  test->scanout.n_presentations++;

  device_pool = meta_backend_native_get_device_pool (backend_native);

  fb = clutter_stage_view_get_onscreen (stage_view);
  crtc = meta_onscreen_native_get_crtc (META_ONSCREEN_NATIVE (fb));
  kms_crtc = meta_crtc_kms_get_kms_crtc (META_CRTC_KMS (crtc));
  kms_device = meta_kms_crtc_get_device (kms_crtc);

  device_file = meta_device_pool_open (device_pool,
                                       meta_kms_device_get_path (kms_device),
                                       META_DEVICE_FILE_FLAG_TAKE_CONTROL,
                                       &error);
  if (!device_file)
    g_error ("Failed to open KMS device: %s", error->message);

  drm_crtc = drmModeGetCrtc (meta_device_file_get_fd (device_file),
                             meta_kms_crtc_get_id (kms_crtc));
  g_assert_nonnull (drm_crtc);
  first_fb_id_expected = GPOINTER_TO_UINT (test->scanout.fb_ids->data);
  test->scanout.fb_ids = g_list_delete_link (test->scanout.fb_ids,
                                             test->scanout.fb_ids);

  /* The buffer ID won't match on the first frame because switching from
   * triple buffered compositing to double buffered direct scanout takes
   * an extra frame to drain the queue. Thereafter we are in direct scanout
   * mode and expect the buffer IDs to match.
   */
  if (test->scanout.n_presentations > 1)
    g_assert_cmpuint (drm_crtc->buffer_id, ==, first_fb_id_expected);

  drmModeFreeCrtc (drm_crtc);

  meta_device_file_release (device_file);

  if (test->scanout.n_presentations >= N_CLIENT_SCANOUTS)
    g_main_loop_quit (test->loop);
  else
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

static void
meta_test_kms_render_client_scanout (void)
{
  MetaBackend *backend = meta_context_get_backend (test_context);
  MetaWaylandCompositor *wayland_compositor =
    meta_context_get_wayland_compositor (test_context);
  ClutterActor *stage = meta_backend_get_stage (backend);
  MetaKms *kms = meta_backend_native_get_kms (META_BACKEND_NATIVE (backend));
  MetaKmsDevice *kms_device = meta_kms_get_devices (kms)->data;
  KmsRenderingTest test;
  MetaWaylandTestClient *wayland_test_client;
  g_autoptr (MetaWaylandTestDriver) test_driver = NULL;
  gulong before_update_handler_id;
  gulong before_paint_handler_id;
  gulong paint_view_handler_id;
  gulong presented_handler_id;

  test_driver = meta_wayland_test_driver_new (wayland_compositor);
  meta_wayland_test_driver_set_property (test_driver,
                                         "gpu-path",
                                         meta_kms_device_get_path (kms_device));

  wayland_test_client =
    meta_wayland_test_client_new ("dma-buf-scanout");
  g_assert_nonnull (wayland_test_client);

  test = (KmsRenderingTest) {
    .loop = g_main_loop_new (NULL, FALSE),
    .scanout = {0},
  };

  paint_view_handler_id =
    g_signal_connect (stage, "paint-view",
                      G_CALLBACK (on_scanout_paint_view), &test);
  before_update_handler_id =
    g_signal_connect (stage, "before-update",
                      G_CALLBACK (on_scanout_before_update), &test);
  before_paint_handler_id =
    g_signal_connect (stage, "before-paint",
                      G_CALLBACK (on_scanout_before_paint), &test);
  presented_handler_id =
    g_signal_connect (stage, "presented",
                      G_CALLBACK (on_scanout_presented), &test);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
  g_main_loop_run (test.loop);
  g_main_loop_unref (test.loop);

  g_assert_cmpint (test.scanout.n_presentations, ==, N_CLIENT_SCANOUTS);

  g_signal_handler_disconnect (stage, before_update_handler_id);
  g_signal_handler_disconnect (stage, before_paint_handler_id);
  g_signal_handler_disconnect (stage, paint_view_handler_id);
  g_signal_handler_disconnect (stage, presented_handler_id);

  meta_wayland_test_driver_emit_sync_event (test_driver, 0);
  meta_wayland_test_client_finish (wayland_test_client);
}

static void
init_tests (void)
{
  g_test_add_func ("/backends/native/kms/render/basic",
                   meta_test_kms_render_basic);
  g_test_add_func ("/backends/native/kms/render/client-scanout",
                   meta_test_kms_render_client_scanout);
}

int
main (int    argc,
      char **argv)
{
  g_autoptr (MetaContext) context = NULL;
  g_autoptr (GError) error = NULL;

  context = meta_create_test_context (META_CONTEXT_TEST_TYPE_VKMS,
                                      META_CONTEXT_TEST_FLAG_NO_X11);
  g_assert (meta_context_configure (context, &argc, &argv, NULL));

  test_context = context;

  init_tests ();

  return meta_context_test_run_tests (META_CONTEXT_TEST (context),
                                      META_TEST_RUN_FLAG_CAN_SKIP);
}
