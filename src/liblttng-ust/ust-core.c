/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (C) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#define _LGPL_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "context-internal.h"
#include "ust-events-internal.h"
#include <usterr-signal-safe.h>
#include "lttng-tracer-core.h"
#include "lttng-rb-clients.h"
#include "lttng-counter-client.h"
#include "jhash.h"

static CDS_LIST_HEAD(lttng_transport_list);
static CDS_LIST_HEAD(lttng_counter_transport_list);

struct lttng_transport *lttng_ust_transport_find(const char *name)
{
	struct lttng_transport *transport;

	cds_list_for_each_entry(transport, &lttng_transport_list, node) {
		if (!strcmp(transport->name, name))
			return transport;
	}
	return NULL;
}

struct lttng_counter_transport *lttng_counter_transport_find(const char *name)
{
	struct lttng_counter_transport *transport;

	cds_list_for_each_entry(transport, &lttng_counter_transport_list, node) {
		if (!strcmp(transport->name, name))
			return transport;
	}
	return NULL;
}

/**
 * lttng_transport_register - LTT transport registration
 * @transport: transport structure
 *
 * Registers a transport which can be used as output to extract the data out of
 * LTTng. Called with ust_lock held.
 */
void lttng_transport_register(struct lttng_transport *transport)
{
	cds_list_add_tail(&transport->node, &lttng_transport_list);
}

/**
 * lttng_transport_unregister - LTT transport unregistration
 * @transport: transport structure
 * Called with ust_lock held.
 */
void lttng_transport_unregister(struct lttng_transport *transport)
{
	cds_list_del(&transport->node);
}

/**
 * lttng_counter_transport_register - LTTng counter transport registration
 * @transport: transport structure
 *
 * Registers a counter transport which can be used as output to extract
 * the data out of LTTng. Called with ust_lock held.
 */
void lttng_counter_transport_register(struct lttng_counter_transport *transport)
{
	cds_list_add_tail(&transport->node, &lttng_counter_transport_list);
}

/**
 * lttng_counter_transport_unregister - LTTng counter transport unregistration
 * @transport: transport structure
 * Called with ust_lock held.
 */
void lttng_counter_transport_unregister(struct lttng_counter_transport *transport)
{
	cds_list_del(&transport->node);
}

/*
 * Needed by comm layer.
 */
struct lttng_enum *lttng_ust_enum_get_from_desc(struct lttng_ust_session *session,
		const struct lttng_ust_enum_desc *enum_desc)
{
	struct lttng_enum *_enum;
	struct cds_hlist_head *head;
	struct cds_hlist_node *node;
	size_t name_len = strlen(enum_desc->name);
	uint32_t hash;

	hash = jhash(enum_desc->name, name_len, 0);
	head = &session->priv->enums_ht.table[hash & (LTTNG_UST_ENUM_HT_SIZE - 1)];
	cds_hlist_for_each_entry(_enum, node, head, hlist) {
		assert(_enum->desc);
		if (_enum->desc == enum_desc)
			return _enum;
	}
	return NULL;
}

size_t lttng_ust_dummy_get_size(void *priv __attribute__((unused)),
		size_t offset)
{
	size_t size = 0;

	size += lttng_ust_lib_ring_buffer_align(offset, lttng_ust_rb_alignof(char));
	size += sizeof(char);		/* tag */
	return size;
}

void lttng_ust_dummy_record(void *priv __attribute__((unused)),
		 struct lttng_ust_lib_ring_buffer_ctx *ctx,
		 struct lttng_ust_channel_buffer *chan)
{
	char sel_char = (char) LTTNG_UST_DYNAMIC_TYPE_NONE;

	chan->ops->event_write(ctx, &sel_char, sizeof(sel_char), lttng_ust_rb_alignof(sel_char));
}

void lttng_ust_dummy_get_value(void *priv __attribute__((unused)),
		struct lttng_ust_ctx_value *value)
{
	value->sel = LTTNG_UST_DYNAMIC_TYPE_NONE;
}

int lttng_context_is_app(const char *name)
{
	if (strncmp(name, "$app.", strlen("$app.")) != 0) {
		return 0;
	}
	return 1;
}

struct lttng_ust_channel_buffer *lttng_ust_alloc_channel_buffer(void)
{
	struct lttng_ust_channel_buffer *lttng_chan_buf;
	struct lttng_ust_channel_common *lttng_chan_common;
	struct lttng_ust_channel_buffer_private *lttng_chan_buf_priv;

	lttng_chan_buf = zmalloc(sizeof(struct lttng_ust_channel_buffer));
	if (!lttng_chan_buf)
		goto lttng_chan_buf_error;
	lttng_chan_buf->struct_size = sizeof(struct lttng_ust_channel_buffer);
	lttng_chan_common = zmalloc(sizeof(struct lttng_ust_channel_common));
	if (!lttng_chan_common)
		goto lttng_chan_common_error;
	lttng_chan_common->struct_size = sizeof(struct lttng_ust_channel_common);
	lttng_chan_buf_priv = zmalloc(sizeof(struct lttng_ust_channel_buffer_private));
	if (!lttng_chan_buf_priv)
		goto lttng_chan_buf_priv_error;
	lttng_chan_buf->parent = lttng_chan_common;
	lttng_chan_common->type = LTTNG_UST_CHANNEL_TYPE_BUFFER;
	lttng_chan_common->child = lttng_chan_buf;
	lttng_chan_buf->priv = lttng_chan_buf_priv;
	lttng_chan_common->priv = &lttng_chan_buf_priv->parent;
	lttng_chan_buf_priv->pub = lttng_chan_buf;
	lttng_chan_buf_priv->parent.pub = lttng_chan_common;

	return lttng_chan_buf;

lttng_chan_buf_priv_error:
	free(lttng_chan_common);
lttng_chan_common_error:
	free(lttng_chan_buf);
lttng_chan_buf_error:
	return NULL;
}

void lttng_ust_free_channel_common(struct lttng_ust_channel_common *chan)
{
	switch (chan->type) {
	case LTTNG_UST_CHANNEL_TYPE_BUFFER:
	{
		struct lttng_ust_channel_buffer *chan_buf;

		chan_buf = (struct lttng_ust_channel_buffer *)chan->child;
		free(chan_buf->parent);
		free(chan_buf->priv);
		free(chan_buf);
		break;
	}
	default:
		abort();
	}
}

void lttng_ust_ring_buffer_clients_init(void)
{
	lttng_ring_buffer_metadata_client_init();
	lttng_ring_buffer_client_overwrite_init();
	lttng_ring_buffer_client_overwrite_rt_init();
	lttng_ring_buffer_client_discard_init();
	lttng_ring_buffer_client_discard_rt_init();
}

void lttng_ust_ring_buffer_clients_exit(void)
{
	lttng_ring_buffer_client_discard_rt_exit();
	lttng_ring_buffer_client_discard_exit();
	lttng_ring_buffer_client_overwrite_rt_exit();
	lttng_ring_buffer_client_overwrite_exit();
	lttng_ring_buffer_metadata_client_exit();
}

void lttng_ust_counter_clients_init(void)
{
	lttng_counter_client_percpu_64_modular_init();
	lttng_counter_client_percpu_32_modular_init();
}

void lttng_ust_counter_clients_exit(void)
{
	lttng_counter_client_percpu_32_modular_exit();
	lttng_counter_client_percpu_64_modular_exit();
}