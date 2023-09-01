/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2022  Intel Corporation.
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
 * Author:
 *   Naveen Kumar <naveen1.kumar@intel.com>
 */

#ifndef CLUTTER_COLOR_STATE_H
#define CLUTTER_COLOR_STATE_H

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_COLOR_STATE (clutter_color_state_get_type ())
CLUTTER_EXPORT
G_DECLARE_FINAL_TYPE (ClutterColorState, clutter_color_state,
                      CLUTTER, COLOR_STATE,
                      GObject)

CLUTTER_EXPORT
ClutterColorState * clutter_color_state_new (ClutterColorspace colorspace);

CLUTTER_EXPORT
ClutterColorspace clutter_color_state_get_colorspace (ClutterColorState *color_state);

G_END_DECLS

#endif /* CLUTTER_COLOR_STATE_H */
