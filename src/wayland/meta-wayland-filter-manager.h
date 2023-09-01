/*
 * Copyright 2023 Red Hat
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

#ifndef META_WAYLAND_FILTER_MANAGER_H
#define META_WAYLAND_FILTER_MANAGER_H

#include <glib.h>
#include <wayland-server-core.h>

#include "core/util-private.h"
#include "wayland/meta-wayland-types.h"

typedef enum _MetaWaylandAccess
{
  META_WAYLAND_ACCESS_ALLOWED,
  META_WAYLAND_ACCESS_DENIED,
} MetaWaylandAccess;

typedef struct _MetaWaylandFilterManager MetaWaylandFilterManager;

typedef MetaWaylandAccess (* MetaWaylandFilterFunc) (const struct wl_client *client,
                                                     const struct wl_global *global,
                                                     gpointer                user_data);

MetaWaylandFilterManager * meta_wayland_filter_manager_new (MetaWaylandCompositor *compositor);

void meta_wayland_filter_manager_free (MetaWaylandFilterManager *filter_manager);

META_EXPORT_TEST
void meta_wayland_filter_manager_add_global (MetaWaylandFilterManager *filter_manager,
                                             struct wl_global         *global,
                                             MetaWaylandFilterFunc     filter_func,
                                             gpointer                  user_data);


META_EXPORT_TEST
void meta_wayland_filter_manager_remove_global (MetaWaylandFilterManager *filter_manager,
                                                struct wl_global         *global);

#endif /* META_WAYLAND_FILTER_MANAGER_H */
