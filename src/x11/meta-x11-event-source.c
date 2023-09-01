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

#include "config.h"

#include "meta-x11-event-source.h"

typedef struct {
  GSource base;
  GPollFD event_poll_fd;
  Display *xdisplay;
} MetaX11EventSource;

static gboolean
meta_x11_event_source_prepare (GSource *source,
                               int     *timeout)
{
  MetaX11EventSource *event_source = (MetaX11EventSource *) source;

  *timeout = -1;

  return XPending (event_source->xdisplay);
}

static gboolean
meta_x11_event_source_check (GSource *source)
{
  MetaX11EventSource *event_source = (MetaX11EventSource *) source;

  return XPending (event_source->xdisplay);
}

static gboolean
meta_x11_event_source_dispatch (GSource     *source,
                                GSourceFunc  callback,
                                gpointer     user_data)
{
  MetaX11EventSource *event_source = (MetaX11EventSource *) source;
  MetaX11EventFunc event_func = (MetaX11EventFunc) callback;
  gboolean retval = G_SOURCE_CONTINUE;

  while (retval == G_SOURCE_CONTINUE &&
         XPending (event_source->xdisplay))
    {
      XEvent xevent;

      XNextEvent (event_source->xdisplay, &xevent);
      retval = event_func (&xevent, user_data);
    }

  return retval;
}

static GSourceFuncs meta_x11_event_source_funcs = {
  meta_x11_event_source_prepare,
  meta_x11_event_source_check,
  meta_x11_event_source_dispatch,
};

GSource *
meta_x11_event_source_new (Display *xdisplay)
{
  GSource *source;
  MetaX11EventSource *event_source;

  source = g_source_new (&meta_x11_event_source_funcs,
                         sizeof (MetaX11EventSource));
  g_source_set_name (source, "[mutter] MetaX11Display events");

  event_source = (MetaX11EventSource *) source;
  event_source->xdisplay = xdisplay;
  event_source->event_poll_fd.fd = ConnectionNumber (xdisplay);
  event_source->event_poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &event_source->event_poll_fd);

  return source;
}
