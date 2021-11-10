/*
 * Copyright (C) 2022 Canonical Ltd.
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
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef META_SWAP_CHAIN_H
#define META_SWAP_CHAIN_H

#include <glib-object.h>

#define META_TYPE_SWAP_CHAIN (meta_swap_chain_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaSwapChain,
                          meta_swap_chain,
                          META, SWAP_CHAIN,
                          GObject)

struct _MetaSwapChainClass
{
  GObjectClass parent_class;
};

MetaSwapChain * meta_swap_chain_new (void);

void meta_swap_chain_push_buffer (MetaSwapChain *swap_chain,
                                  unsigned int   plane_id,
                                  GObject       *buffer);

void meta_swap_chain_swap_buffers (MetaSwapChain *swap_chain);

void meta_swap_chain_release_buffers (MetaSwapChain *swap_chain);

#endif /* META_SWAP_CHAIN_H */
