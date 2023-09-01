/*
 * Copyright (C) 2022 Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_FRAME_HEADER_H
#define META_FRAME_HEADER_H

#include <gtk/gtk.h>

#define META_TYPE_FRAME_HEADER (meta_frame_header_get_type ())
G_DECLARE_FINAL_TYPE (MetaFrameHeader, meta_frame_header,
                      META, FRAME_HEADER, GtkWidget)

GtkWidget * meta_frame_header_new (void);

#endif /* META_FRAME_HEADER_H */
