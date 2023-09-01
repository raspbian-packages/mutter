/*
 * Copyright (C) 2023 Red Hat Inc.
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

#ifndef META_X11_EVENT_SOURCE_H
#define META_X11_EVENT_SOURCE_H

#include <glib.h>
#include <X11/Xlib.h>

typedef gboolean (* MetaX11EventFunc) (XEvent   *xevent,
                                       gpointer  user_data);

GSource * meta_x11_event_source_new (Display *xdisplay);

#endif
