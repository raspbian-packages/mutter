/*
 * Copyright (C) 2022 Intel Corporation.
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cogl/cogl.h>

#include "tests/cogl-test-utils.h"

static int
get_bits (uint32_t in,
          int      end,
          int      begin)
{
  int mask = (1 << (end - begin + 1)) - 1;

  return (in >> begin) & mask;
}

static int
rgb10_to_rgb8 (int rgb10)
{
  float r;

  r = rgb10 / (float) (1 << 10);
  return (int) (r * (float) (1 << 8));
}

static int
rgb8_to_rgb10 (int rgb8)
{
  float r;

  r = rgb8 / (float) (1 << 8);
  return (int) (r * (float) (1 << 10));
}

static void
test_offscreen_texture_formats_store_rgb10 (void)
{
  const int rgb10_red = 514;
  const int rgb10_green = 258;
  const int rgb10_blue = 18;
  float red;
  float green;
  float blue;
  GError *error = NULL;
  CoglPixelFormat formats[] = {
    COGL_PIXEL_FORMAT_ABGR_2101010,
    COGL_PIXEL_FORMAT_ARGB_2101010,
    COGL_PIXEL_FORMAT_XRGB_2101010,
    COGL_PIXEL_FORMAT_RGBA_1010102,
    COGL_PIXEL_FORMAT_BGRA_1010102,
    COGL_PIXEL_FORMAT_XBGR_2101010,
  };
  int i;

  /* The extra fraction is there to avoid rounding inconsistencies in OpenGL
   * implementations. */
  red = (rgb10_red / (float) (1 << 10)) + 0.00001;
  green = (rgb10_green / (float) (1 << 10)) + 0.00001;
  blue = (rgb10_blue / (float) (1 << 10)) + 0.00001;

  /* Make sure that that the color value can't be represented using rgb8. */
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_red)), !=, rgb10_red);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_green)), !=, rgb10_green);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_blue)), !=, rgb10_blue);

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      CoglTexture2D *tex;
      CoglOffscreen *offscreen;
      uint32_t rgb8_readback[4];
      uint8_t *rgb8_buf;
      int j;

      /* Allocate 2x2 to ensure we avoid any fast paths. */
      tex = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[i]);

      offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex));
      cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen), &error);
      g_assert_no_error (error);

      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR,
                                red, green, blue, 1.0);

      for (j = 0; j < G_N_ELEMENTS (formats); j++)
        {
          uint32_t rgb10_readback[4];
          int channels[3];
          int alpha;

          cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen), 0, 0, 2, 2,
                                        formats[j],
                                        (uint8_t *) &rgb10_readback);

          switch (formats[j])
            {
            case COGL_PIXEL_FORMAT_RGBA_1010102:
            case COGL_PIXEL_FORMAT_BGRA_1010102:
              channels[0] = get_bits (rgb10_readback[0], 31, 22);
              channels[1] = get_bits (rgb10_readback[0], 21, 12);
              channels[2] = get_bits (rgb10_readback[0], 11, 2);
              alpha = get_bits (rgb10_readback[0], 1, 0);
              break;
            case COGL_PIXEL_FORMAT_XRGB_2101010:
            case COGL_PIXEL_FORMAT_ARGB_2101010:
            case COGL_PIXEL_FORMAT_XBGR_2101010:
            case COGL_PIXEL_FORMAT_ABGR_2101010:
              alpha = get_bits (rgb10_readback[0], 31, 30);
              channels[0] = get_bits (rgb10_readback[0], 29, 20);
              channels[1] = get_bits (rgb10_readback[0], 19, 10);
              channels[2] = get_bits (rgb10_readback[0], 9, 0);
              break;
            default:
              g_assert_not_reached ();
            }

          g_assert_cmpint (alpha, ==, 0x3);

          switch (formats[j])
            {
            case COGL_PIXEL_FORMAT_RGBA_1010102:
            case COGL_PIXEL_FORMAT_XRGB_2101010:
            case COGL_PIXEL_FORMAT_ARGB_2101010:
              g_assert_cmpint (channels[0], ==, rgb10_red);
              g_assert_cmpint (channels[1], ==, rgb10_green);
              g_assert_cmpint (channels[2], ==, rgb10_blue);
              break;
            case COGL_PIXEL_FORMAT_BGRA_1010102:
            case COGL_PIXEL_FORMAT_XBGR_2101010:
            case COGL_PIXEL_FORMAT_ABGR_2101010:
              g_assert_cmpint (channels[0], ==, rgb10_blue);
              g_assert_cmpint (channels[1], ==, rgb10_green);
              g_assert_cmpint (channels[2], ==, rgb10_red);
              break;
            default:
              g_assert_not_reached ();
            }
        }

      cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen), 0, 0, 2, 2,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    (uint8_t *) &rgb8_readback);
      rgb8_buf = (uint8_t *) &rgb8_readback[0];

      g_assert_cmpint (rgb8_buf[0], ==, rgb10_to_rgb8 (rgb10_red));
      g_assert_cmpint (rgb8_buf[1], ==, rgb10_to_rgb8 (rgb10_green));
      g_assert_cmpint (rgb8_buf[2], ==, rgb10_to_rgb8 (rgb10_blue));
      g_assert_cmpint (rgb8_buf[3], ==, 0xff);

      g_object_unref (offscreen);
      cogl_object_unref (tex);
    }
}

static void
test_offscreen_texture_formats_paint_rgb10 (void)
{
  const int rgb10_red = 514;
  const int rgb10_green = 258;
  const int rgb10_blue = 18;
  float red;
  float green;
  float blue;
  CoglTexture2D *tex_src;
  CoglOffscreen *offscreen_src;
  CoglTexture2D *tex_dst;
  CoglOffscreen *offscreen_dst;
  CoglPipeline *pipeline;
  uint32_t rgb10_readback[4];
  GError *error = NULL;

  /* The extra fraction is there to avoid rounding inconsistencies in OpenGL
   * implementations. */
  red = (rgb10_red / (float) (1 << 10)) + 0.00001;
  green = (rgb10_green / (float) (1 << 10)) + 0.00001;
  blue = (rgb10_blue / (float) (1 << 10)) + 0.00001;

  /* Make sure that that the color value can't be represented using rgb8. */
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_red)), !=, rgb10_red);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_green)), !=, rgb10_green);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_blue)), !=, rgb10_blue);

  tex_src = cogl_texture_2d_new_with_format (test_ctx, 2, 2,
                                             COGL_PIXEL_FORMAT_RGBA_1010102);
  offscreen_src = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_src));
  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_src), &error);
  g_assert_no_error (error);

  tex_dst = cogl_texture_2d_new_with_format (test_ctx, 2, 2,
                                             COGL_PIXEL_FORMAT_ABGR_2101010);
  offscreen_dst = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_dst));
  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_dst), &error);
  g_assert_no_error (error);

  cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen_src),
                            COGL_BUFFER_BIT_COLOR,
                            red, green, blue, 1.0);

  pipeline = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_layer_texture (pipeline, 0, tex_src);
  cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen_dst),
                                   pipeline,
                                   -1.0, -1.0, 1.0, 1.0);
  cogl_object_unref (pipeline);

  cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen_dst), 0, 0, 2, 2,
                                COGL_PIXEL_FORMAT_ABGR_2101010,
                                (uint8_t *) &rgb10_readback);
  g_assert_cmpint (get_bits (rgb10_readback[0], 31, 30), ==, 0x3);
  g_assert_cmpint (get_bits (rgb10_readback[0], 29, 20), ==, rgb10_blue);
  g_assert_cmpint (get_bits (rgb10_readback[0], 19, 10), ==, rgb10_green);
  g_assert_cmpint (get_bits (rgb10_readback[0], 9, 0), ==, rgb10_red);
}

COGL_TEST_SUITE (
  g_test_add_func ("/offscreen/texture-formats/store-rgb10",
                   test_offscreen_texture_formats_store_rgb10);
  g_test_add_func ("/offscreen/texture-formats/paint-rgb10",
                   test_offscreen_texture_formats_paint_rgb10);
)
