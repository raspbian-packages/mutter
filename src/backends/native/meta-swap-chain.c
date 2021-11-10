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

#include "backends/native/meta-swap-chain.h"

typedef struct
{
  GObject *front, *back;
  gboolean back_is_set;
} PlaneState;

typedef struct _MetaSwapChainPrivate MetaSwapChainPrivate;
struct _MetaSwapChainPrivate
{
  GHashTable *plane_states;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaSwapChain, meta_swap_chain, G_TYPE_OBJECT)

MetaSwapChain *
meta_swap_chain_new (void)
{
  return g_object_new (META_TYPE_SWAP_CHAIN, NULL);
}

void
meta_swap_chain_push_buffer (MetaSwapChain *swap_chain,
                             unsigned int   plane_id,
                             GObject       *buffer)
{
  MetaSwapChainPrivate *priv =
    meta_swap_chain_get_instance_private (swap_chain);
  gpointer key = GUINT_TO_POINTER (plane_id);
  PlaneState *plane_state;

  plane_state = g_hash_table_lookup (priv->plane_states, key);
  if (plane_state == NULL)
    {
      plane_state = g_new0 (PlaneState, 1);
      g_hash_table_insert (priv->plane_states, key, plane_state);
    }

  plane_state->back_is_set = TRUE;  /* note buffer may be NULL */
  g_set_object (&plane_state->back, buffer);
}

static void
swap_plane_buffers (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  PlaneState *plane_state = value;

  if (plane_state->back_is_set)
    {
      g_set_object (&plane_state->front, plane_state->back);
      g_clear_object (&plane_state->back);
      plane_state->back_is_set = FALSE;
    }
}

void
meta_swap_chain_swap_buffers (MetaSwapChain *swap_chain)
{
  MetaSwapChainPrivate *priv =
    meta_swap_chain_get_instance_private (swap_chain);

  g_hash_table_foreach (priv->plane_states, swap_plane_buffers, NULL);
}

void
meta_swap_chain_release_buffers (MetaSwapChain *swap_chain)
{
  MetaSwapChainPrivate *priv =
    meta_swap_chain_get_instance_private (swap_chain);

  g_hash_table_remove_all (priv->plane_states);
}

static void
meta_swap_chain_dispose (GObject *object)
{
  MetaSwapChain *swap_chain = META_SWAP_CHAIN (object);

  meta_swap_chain_release_buffers (swap_chain);

  G_OBJECT_CLASS (meta_swap_chain_parent_class)->dispose (object);
}

static void
meta_swap_chain_finalize (GObject *object)
{
  MetaSwapChain *swap_chain = META_SWAP_CHAIN (object);
  MetaSwapChainPrivate *priv =
    meta_swap_chain_get_instance_private (swap_chain);

  g_hash_table_unref (priv->plane_states);

  G_OBJECT_CLASS (meta_swap_chain_parent_class)->finalize (object);
}

static void
destroy_plane_state (gpointer data)
{
  PlaneState *plane_state = data;

  g_clear_object (&plane_state->front);
  g_clear_object (&plane_state->back);
  g_free (plane_state);
}

static void
meta_swap_chain_init (MetaSwapChain *swap_chain)
{
  MetaSwapChainPrivate *priv =
    meta_swap_chain_get_instance_private (swap_chain);

  priv->plane_states = g_hash_table_new_full (NULL,
                                              NULL,
                                              NULL,
                                              destroy_plane_state);
}

static void
meta_swap_chain_class_init (MetaSwapChainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_swap_chain_dispose;
  object_class->finalize = meta_swap_chain_finalize;
}
