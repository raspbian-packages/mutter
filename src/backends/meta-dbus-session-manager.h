/*
 * Copyright (C) 2015-2017, 2022 Red Hat Inc.
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

#ifndef META_DBUS_SERVICE_MANAGER_H
#define META_DBUS_SERVICE_MANAGER_H

#include <gio/gio.h>
#include <glib-object.h>

#include "backends/meta-backend-types.h"

#define META_TYPE_DBUS_SESSION_MANAGER (meta_dbus_session_manager_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaDbusSessionManager,
                          meta_dbus_session_manager,
                          META, DBUS_SESSION_MANAGER,
                          GObject)

struct _MetaDbusSessionManagerClass
{
  GObjectClass parent_class;
};

MetaDbusSession * meta_dbus_session_manager_create_session (MetaDbusSessionManager  *session_manager,
                                                            GDBusMethodInvocation   *invocation,
                                                            GError                 **error,
                                                            ...);

MetaDbusSession * meta_dbus_session_manager_get_session (MetaDbusSessionManager *session_manager,
                                                         const char             *session_id);

void meta_dbus_session_manager_inhibit (MetaDbusSessionManager *session_manager);

void meta_dbus_session_manager_uninhibit (MetaDbusSessionManager *session_manager);

MetaBackend * meta_dbus_session_manager_get_backend (MetaDbusSessionManager *session_manager);

GDBusConnection * meta_dbus_session_manager_get_connection (MetaDbusSessionManager *session_manager);

GDBusInterfaceSkeleton * meta_dbus_session_manager_get_interface_skeleton (MetaDbusSessionManager *session_manager);

#endif /* META_DBUS_SERVICE_MANAGER_H */
