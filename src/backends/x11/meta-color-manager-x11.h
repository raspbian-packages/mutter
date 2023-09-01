/*
 * Copyright (C) 2021 Red Hat Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_COLOR_MANAGER_X11_H
#define META_COLOR_MANAGER_X11_H

#include <glib-object.h>

#include "backends/meta-color-manager-private.h"

#define META_TYPE_COLOR_MANAGER_X11 (meta_color_manager_x11_get_type ())
G_DECLARE_FINAL_TYPE (MetaColorManagerX11, meta_color_manager_x11,
                      META, COLOR_MANAGER_X11,
                      MetaColorManager)

#endif /* META_COLOR_MANAGER_X11_H */
