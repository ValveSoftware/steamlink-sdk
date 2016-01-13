/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include "connman.h"

struct address_info {
	int index;
	uint32_t start;
	uint32_t end;

	unsigned int use_count;
	struct connman_ippool *pool;
};

struct connman_ippool {
	unsigned int refcount;

	struct address_info *info;

	char *gateway;
	char *broadcast;
	char *start_ip;
	char *end_ip;
	char *subnet_mask;

	ippool_collision_cb_t collision_cb;
	void *user_data;
};

GSList *allocated_blocks;
GHashTable *pool_hash;

static uint32_t last_block;
static uint32_t block_16_bits;
static uint32_t block_20_bits;
static uint32_t block_24_bits;
static uint32_t subnet_mask_24;

struct connman_ippool *
__connman_ippool_ref_debug(struct connman_ippool *pool,
				const char *file, int line, const char *caller)
{
	DBG("%p ref %d by %s:%d:%s()", pool, pool->refcount + 1,
		file, line, caller);

	__sync_fetch_and_add(&pool->refcount, 1);

	return pool;
}

void __connman_ippool_unref_debug(struct connman_ippool *pool,
				const char *file, int line, const char *caller)
{
	if (!pool)
		return;

	DBG("%p ref %d by %s:%d:%s()", pool, pool->refcount - 1,
		file, line, caller);

	if (__sync_fetch_and_sub(&pool->refcount, 1) != 1)
		return;

	g_hash_table_remove(pool_hash, pool);
}

static char *get_ip(uint32_t ip)
{
	struct in_addr addr;

	addr.s_addr = htonl(ip);

	return g_strdup(inet_ntoa(addr));
}

static uint32_t next_block(uint32_t block)
{
	uint32_t next;

	/*
	 * Return the next IP block within the private IP range
	 *
	 * 16-bit block 192.168.0.0 – 192.168.255.255
	 * 20-bit block  172.16.0.0 –  172.31.255.255
	 * 24-bit block    10.0.0.0 –  10.255.255.255
	 */

	next = (block & 0x0000ff00) >> 8;
	next += 1;

	if (next == 255) {
		if ((block & 0xffff0000) == block_16_bits) {
			/*
			 * Reached the end of the 16 bit block, switch
			 * to the 20-bit block.
			 */
			return block_20_bits;
		}

		if ((block & 0xffff0000) >= block_20_bits) {
			next = (block & 0x00ff0000) >> 16;
			if (next >= 16 && next < 32)
				next += 1;

			if (next == 32) {
				/*
				 * Reached the end of the 20 bit
				 * block, switch to the 24-bit block.
				 */
				return block_24_bits;
			}

			return (block & 0xff000000) |
				((next << 16) & 0x00ff0000);
		}

		if ((block & 0xff000000) == block_24_bits) {
			next = (block & 0x00ff0000) >> 16;
			if (next < 255)
				next += 1;

			if (next == 255) {
				/*
				 * Reached the end of the 24 bit
				 * block, switch to the 16-bit block.
				 */
				return block_16_bits;
			}

			return (block & 0xff000000) |
				((next << 16) & 0x00ff0000);
		}
	}

	return (block & 0xffff0000) | ((next << 8) & 0x0000ff00);
}

static uint32_t get_free_block(unsigned int size)
{
	struct address_info *info;
	uint32_t block;
	GSList *list;
	bool collision;

	/*
	 * Instead starting always from the 16 bit block, we start
	 * from the last assigned block. This is a simple optimimazion
	 * for the case where a lot of blocks have been assigned, e.g.
	 * the first half of the private IP pool is in use and a new
	 * we need to find a new block.
	 *
	 * To only thing we have to make sure is that we terminated if
	 * there is no block left.
	 */
	if (last_block == 0)
		block = block_16_bits;
	else
		block = next_block(last_block);

	do {
		collision = false;
		for (list = allocated_blocks; list; list = list->next) {
			info = list->data;

			if (info->start <= block && block <= info->end) {
				collision = true;
				break;
			}
		}

		if (!collision)
			return block;

		block = next_block(block);
	} while (block != last_block);

	return 0;
}

static struct address_info *lookup_info(int index, uint32_t start)
{
	GSList *list;

	for (list = allocated_blocks; list; list = list->next) {
		struct address_info *info = list->data;

		if (info->index == index && info->start == start)
			return info;
	}

	return NULL;
}

static bool is_private_address(uint32_t address)
{
	unsigned int a, b;

	a = (address & 0xff000000) >> 24;
	b = (address & 0x00ff0000) >> 16;

	if (a == 10 || (a == 192 && b == 168) ||
					(a == 172 && (b >= 16 && b <= 31)))
		return true;

	return false;
}

void __connman_ippool_newaddr(int index, const char *address,
				unsigned char prefixlen)
{
	struct address_info *info, *it;
	struct in_addr inp;
	uint32_t start, end, mask;
	GSList *list;

	if (inet_aton(address, &inp) == 0)
		return;

	start = ntohl(inp.s_addr);
	if (!is_private_address(start))
		return;

	if (prefixlen >= 32)
		mask = 0xffffffff;
	else
		mask = ~(0xffffffff >> prefixlen);

	start = start & mask;
	end = start | ~mask;

	info = lookup_info(index, start);
	if (info)
		goto update;

	info = g_try_new0(struct address_info, 1);
	if (!info)
		return;

	info->index = index;
	info->start = start;
	info->end = end;

	allocated_blocks = g_slist_prepend(allocated_blocks, info);

update:
	info->use_count = info->use_count + 1;

	if (info->use_count > 1 || info->pool) {
		/*
		 * We need only to check for the first IP in a block for
		 * collisions.
		 */
		return;
	}

	for (list = allocated_blocks; list; list = list->next) {
		it = list->data;

		if (it == info)
			continue;

		if (!(info->start >= it->start && info->start <= it->end))
			continue;

		if (it->pool && it->pool->collision_cb)
			it->pool->collision_cb(it->pool, it->pool->user_data);

		return;
	}
}

void __connman_ippool_deladdr(int index, const char *address,
				unsigned char prefixlen)
{
	struct address_info *info;
	struct in_addr inp;
	uint32_t start, mask;

	if (inet_aton(address, &inp) == 0)
		return;

	start = ntohl(inp.s_addr);
	if (!is_private_address(start))
		return;

	mask = ~(0xffffffff >> prefixlen);
	start = start & mask;

	info = lookup_info(index, start);
	if (!info) {
		/* In theory this should never happen */
		connman_error("Inconsistent IP pool management (start not found)");
		return;
	}

	info->use_count = info->use_count - 1;
	if (info->pool)
		return;

	if (info->use_count > 0)
		return;

	allocated_blocks = g_slist_remove(allocated_blocks, info);
	g_free(info);
}

struct connman_ippool *__connman_ippool_create(int index,
					unsigned int start,
					unsigned int range,
					ippool_collision_cb_t collision_cb,
					void *user_data)
{
	struct connman_ippool *pool;
	struct address_info *info;
	uint32_t block;

	DBG("");

	/*
	 * The range is at max 255 and we don't support overlapping
	 * blocks.
	 */
	if (start + range > 254) {
		connman_error("IP pool does not support pool size larger than 254");
		return NULL;
	}

	block = get_free_block(start + range);
	if (block == 0) {
		connman_warn("Could not find a free IP block");
		return NULL;
	}

	pool = g_try_new0(struct connman_ippool, 1);
	if (!pool)
		return NULL;

	info = g_try_new0(struct address_info, 1);
	if (!info) {
		g_free(pool);
		return NULL;
	}

	last_block = block;

	info->index = index;
	info->start = block;
	info->end = block + range;

	pool->refcount = 1;
	pool->info = info;
	pool->collision_cb = collision_cb;
	pool->user_data = user_data;

	info->pool = pool;

	if (range == 0)
		range = 1;

	pool->gateway = get_ip(info->start + 1);
	pool->broadcast = get_ip(info->start + 255);
	pool->subnet_mask = get_ip(subnet_mask_24);
	pool->start_ip = get_ip(block + start);
	pool->end_ip = get_ip(block + start + range);

	allocated_blocks = g_slist_prepend(allocated_blocks, info);
	g_hash_table_insert(pool_hash, pool, pool);

	return pool;
}

const char *__connman_ippool_get_gateway(struct connman_ippool *pool)
{
	return pool->gateway;
}

const char *__connman_ippool_get_broadcast(struct connman_ippool *pool)
{
	return pool->broadcast;
}

const char *__connman_ippool_get_start_ip(struct connman_ippool *pool)
{
	return pool->start_ip;
}

const char *__connman_ippool_get_end_ip(struct connman_ippool *pool)
{
	return pool->end_ip;
}

const char *__connman_ippool_get_subnet_mask(struct connman_ippool *pool)
{
	return pool->subnet_mask;
}

static void pool_free(gpointer data)
{
	struct connman_ippool *pool = data;

	if (pool->info) {
		allocated_blocks = g_slist_remove(allocated_blocks, pool->info);
		g_free(pool->info);
	}

	g_free(pool->gateway);
	g_free(pool->broadcast);
	g_free(pool->start_ip);
	g_free(pool->end_ip);
	g_free(pool->subnet_mask);

	g_free(pool);
}

int __connman_ippool_init(void)
{
	DBG("");

	block_16_bits = ntohl(inet_addr("192.168.0.0"));
	block_20_bits = ntohl(inet_addr("172.16.0.0"));
	block_24_bits = ntohl(inet_addr("10.0.0.0"));
	subnet_mask_24 = ntohl(inet_addr("255.255.255.0"));

	pool_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
					pool_free);

	return 0;
}

void __connman_ippool_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(pool_hash);
	pool_hash = NULL;

	g_slist_free_full(allocated_blocks, g_free);
	last_block = 0;
	allocated_blocks = NULL;
}
