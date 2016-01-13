/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <xtables.h>

#include <linux/netfilter_ipv4/ip_tables.h>

#include "connman.h"
#include "src/shared/util.h"

/*
 * Some comments on how the iptables API works (some of them from the
 * source code from iptables and the kernel):
 *
 * - valid_hooks: bit indicates valid IDs for hook_entry
 * - hook_entry[ID] offset to the chain start
 * - overflows should be end of entry chains, and uncodintional policy nodes.
 * - policy entry: last entry in a chain
 * - user chain: end of last builtin + policy entry
 * - final entry must be error node
 * - Underflows must be unconditional and use the STANDARD target with
 *   ACCEPT/DROP
 * - IPT_SO_GET_INFO and IPT_SO_GET_ENTRIES are used to read a table
 * - IPT_SO_GET_INFO: struct ipt_getinfo (note the lack of table content)
 * - IPT_SO_GET_ENTRIES: struct ipt_get_entries (contains only parts of the
 *   table header/meta info. The table is appended after the header. The entries
 *   are of the type struct ipt_entry.
 * - After the ipt_entry the matches are appended. After the matches
 *   the target is appended.
 * - ipt_entry->target_offset =  Size of ipt_entry + matches
 * - ipt_entry->next_offset =  Size of ipt_entry + matches + target
 * - IPT_SO_SET_REPLACE is used to write a table (contains the complete
 * - hook_entry and overflow mark the begining and the end of a chain, e.g
 *     entry hook: pre/in/fwd/out/post -1/0/352/504/-1
 *     underflow:  pre/in/fwd/out/post -1/200/352/904/-1
 *   means that INPUT starts at offset 0 and ends at 200 (the start offset to
 *   the last element). FORWARD has one entry starting/ending at 352. The entry
 *   has a size of 152. 352 + 152 = 504 which is the start of the OUTPUT chain
 *   which then ends at 904. PREROUTING and POSTROUTING are invalid hooks in
 *   the filter table.
 * - 'iptables -t filter -A INPUT -m mark --mark 999 -j LOG'
 *   writing that table looks like this:
 *
 *   filter valid_hooks 0x0000000e  num_entries 5  size 856
 *   entry hook: pre/in/fwd/out/post -1/0/376/528/-1
 *   underflow:  pre/in/fwd/out/post -1/224/376/528/-1
 *   entry 0x699d30  offset 0  size 224
 *     RULE  match 0x699da0  target 0x699dd0
 *             match  mark match 0x3e7
 *             target  LOG flags 0 level 4
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x699e10  offset 224  size 152
 *     RULE  match 0x699e80  target 0x699e80
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x699ea8  offset 376  size 152
 *     RULE  match 0x699f18  target 0x699f18
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x699f40  offset 528  size 152
 *     RULE  match 0x699fb0  target 0x699fb0
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x699fd8  offset 680  size 176
 *     USER CHAIN (ERROR)  match 0x69a048  target 0x69a048
 *
 *   Reading the filter table looks like this:
 *
 *   filter valid_hooks 0x0000000e  num_entries 5  size 856
 *   entry hook: pre/in/fwd/out/post -1/0/376/528/-1
 *   underflow:  pre/in/fwd/out/post -1/224/376/528/-1
 *   entry 0x25fec28  offset 0  size 224
 *     CHAIN (INPUT)  match 0x25fec98  target 0x25fecc8
 *             match  mark match 0x3e7
 *             target  LOG flags 0 level 4
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x25fed08  offset 224  size 152
 *     RULE  match 0x25fed78  target 0x25fed78
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x25feda0  offset 376  size 152
 *     CHAIN (FORWARD)  match 0x25fee10  target 0x25fee10
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x25fee38  offset 528  size 152
 *     CHAIN (OUTPUT)  match 0x25feea8  target 0x25feea8
 *             target ACCEPT
 *             src 0.0.0.0/0.0.0.0
 *             dst 0.0.0.0/0.0.0.0
 *   entry 0x25feed0  offset 680  size 176
 *     End of CHAIN
 */

static const char *hooknames[] = {
	[NF_IP_PRE_ROUTING]	= "PREROUTING",
	[NF_IP_LOCAL_IN]	= "INPUT",
	[NF_IP_FORWARD]		= "FORWARD",
	[NF_IP_LOCAL_OUT]	= "OUTPUT",
	[NF_IP_POST_ROUTING]	= "POSTROUTING",
};

#define LABEL_ACCEPT  "ACCEPT"
#define LABEL_DROP    "DROP"
#define LABEL_QUEUE   "QUEUE"
#define LABEL_RETURN  "RETURN"

#define XT_OPTION_OFFSET_SCALE 256

#define MIN_ALIGN (__alignof__(struct ipt_entry))

#define ALIGN(s) (((s) + ((MIN_ALIGN)-1)) & ~((MIN_ALIGN)-1))

struct error_target {
	struct xt_entry_target t;
	char error[IPT_TABLE_MAXNAMELEN];
};

struct connman_iptables_entry {
	int offset;
	int builtin;

	struct ipt_entry *entry;
};

struct connman_iptables {
	char *name;
	int ipt_sock;

	struct ipt_getinfo *info;
	struct ipt_get_entries *blob_entries;

	unsigned int num_entries;
	unsigned int old_entries;
	unsigned int size;

	unsigned int underflow[NF_INET_NUMHOOKS];
	unsigned int hook_entry[NF_INET_NUMHOOKS];

	GList *entries;
};

static GHashTable *table_hash = NULL;
static bool debug_enabled = false;

typedef int (*iterate_entries_cb_t)(struct ipt_entry *entry, int builtin,
					unsigned int hook, size_t size,
					unsigned int offset, void *user_data);

static unsigned int next_hook_entry_index(unsigned int *valid_hooks)
{
	unsigned int h;

	if (*valid_hooks == 0)
		return NF_INET_NUMHOOKS;

	h = __builtin_ffs(*valid_hooks) - 1;
	*valid_hooks ^= (1 << h);

	return h;
}

static int iterate_entries(struct ipt_entry *entries,
				unsigned int valid_hooks,
				unsigned int *hook_entry,
				unsigned int *underflow,
				size_t size, iterate_entries_cb_t cb,
				void *user_data)
{
	unsigned int offset, h, hook;
	int builtin, err;
	struct ipt_entry *entry;

	h = next_hook_entry_index(&valid_hooks);
	hook = h;

	for (offset = 0, entry = entries; offset < size;
			offset += entry->next_offset) {
		builtin = -1;
		entry = (void *)entries + offset;

		/*
		 * Updating builtin, hook and h is very tricky.
		 * The rules are:
		 * - builtin is only set to the current hook number
		 *   if the current entry is the hook entry (aka chain
		 *   head). And only for builtin chains, never for
		 *   the user chains.
		 * - hook is the current hook number. If we
		 *   look at user chains it needs to be NF_INET_NETNUMHOOKS.
		 * - h is the next hook entry. Thous we need to be carefully
		 *   not to access the table when h is NF_INET_NETNUMHOOKS.
		 */
		if (h < NF_INET_NUMHOOKS && hook_entry[h] == offset) {
			builtin = h;
			hook = h;
		}

		if (h == NF_INET_NUMHOOKS)
			hook = h;

		if (h < NF_INET_NUMHOOKS && underflow[h] <= offset)
			h = next_hook_entry_index(&valid_hooks);

		err = cb(entry, builtin, hook, size, offset, user_data);
		if (err < 0)
			return err;
	}

	return 0;
}

static int print_entry(struct ipt_entry *entry, int builtin, unsigned int hook,
					size_t size, unsigned int offset,
					void *user_data)
{
	iterate_entries_cb_t cb = user_data;

	DBG("entry %p  hook %d  offset %d  size %d", entry, hook,
			offset, entry->next_offset);

	return cb(entry, builtin, hook, size, offset, NULL);
}

static int target_to_verdict(const char *target_name)
{
	if (!g_strcmp0(target_name, LABEL_ACCEPT))
		return -NF_ACCEPT - 1;

	if (!g_strcmp0(target_name, LABEL_DROP))
		return -NF_DROP - 1;

	if (!g_strcmp0(target_name, LABEL_QUEUE))
		return -NF_QUEUE - 1;

	if (!g_strcmp0(target_name, LABEL_RETURN))
		return XT_RETURN;

	return 0;
}

static bool is_builtin_target(const char *target_name)
{
	if (!g_strcmp0(target_name, LABEL_ACCEPT) ||
		!g_strcmp0(target_name, LABEL_DROP) ||
		!g_strcmp0(target_name, LABEL_QUEUE) ||
		!g_strcmp0(target_name, LABEL_RETURN))
		return true;

	return false;
}

static bool is_jump(struct connman_iptables_entry *e)
{
	struct xt_entry_target *target;

	target = ipt_get_target(e->entry);

	if (!g_strcmp0(target->u.user.name, IPT_STANDARD_TARGET)) {
		struct xt_standard_target *t;

		t = (struct xt_standard_target *)target;

		switch (t->verdict) {
		case XT_RETURN:
		case -NF_ACCEPT - 1:
		case -NF_DROP - 1:
		case -NF_QUEUE - 1:
		case -NF_STOP - 1:
			return false;

		default:
			return true;
		}
	}

	return false;
}

static bool is_fallthrough(struct connman_iptables_entry *e)
{
	struct xt_entry_target *target;

	target = ipt_get_target(e->entry);
	if (!g_strcmp0(target->u.user.name, IPT_STANDARD_TARGET)) {
		struct xt_standard_target *t;

		t = (struct xt_standard_target *)target;
		if (t->verdict == 0)
			return true;
	}
	return false;
}

static bool is_chain(struct connman_iptables *table,
				struct connman_iptables_entry *e)
{
	struct ipt_entry *entry;
	struct xt_entry_target *target;

	entry = e->entry;
	if (e->builtin >= 0)
		return true;

	target = ipt_get_target(entry);
	if (!g_strcmp0(target->u.user.name, IPT_ERROR_TARGET))
		return true;

	return false;
}

static GList *find_chain_head(struct connman_iptables *table,
				const char *chain_name)
{
	GList *list;
	struct connman_iptables_entry *head;
	struct ipt_entry *entry;
	struct xt_entry_target *target;
	int builtin;

	for (list = table->entries; list; list = list->next) {
		head = list->data;
		entry = head->entry;

		/* Buit-in chain */
		builtin = head->builtin;
		if (builtin >= 0 && !g_strcmp0(hooknames[builtin], chain_name))
			break;

		/* User defined chain */
		target = ipt_get_target(entry);
		if (!g_strcmp0(target->u.user.name, IPT_ERROR_TARGET) &&
		    !g_strcmp0((char *)target->data, chain_name))
			break;
	}

	return list;
}

static GList *find_chain_tail(struct connman_iptables *table,
				const char *chain_name)
{
	struct connman_iptables_entry *tail;
	GList *chain_head, *list;

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return NULL;

	/* Then we look for the next chain */
	for (list = chain_head->next; list; list = list->next) {
		tail = list->data;

		if (is_chain(table, tail))
			return list;
	}

	/* Nothing found, we return the table end */
	return g_list_last(table->entries);
}


static void update_offsets(struct connman_iptables *table)
{
	GList *list, *prev;
	struct connman_iptables_entry *entry, *prev_entry;

	for (list = table->entries; list; list = list->next) {
		entry = list->data;

		if (list == table->entries) {
			entry->offset = 0;

			continue;
		}

		prev = list->prev;
		prev_entry = prev->data;

		entry->offset = prev_entry->offset +
					prev_entry->entry->next_offset;
	}
}

static void update_targets_reference(struct connman_iptables *table,
				struct connman_iptables_entry *entry_before,
				struct connman_iptables_entry *modified_entry,
				bool is_removing)
{
	struct connman_iptables_entry *tmp;
	struct xt_standard_target *t;
	GList *list;
	int offset;

	offset = modified_entry->entry->next_offset;

	for (list = table->entries; list; list = list->next) {
		tmp = list->data;

		if (!is_jump(tmp))
			continue;

		t = (struct xt_standard_target *)ipt_get_target(tmp->entry);

		if (is_removing) {
			if (t->verdict >= entry_before->offset)
				t->verdict -= offset;
		} else {
			if (t->verdict > entry_before->offset)
				t->verdict += offset;
		}
	}

	if (is_fallthrough(modified_entry)) {
		t = (struct xt_standard_target *)
			ipt_get_target(modified_entry->entry);

		t->verdict = entry_before->offset +
			modified_entry->entry->target_offset +
			ALIGN(sizeof(struct xt_standard_target));
		t->target.u.target_size =
			ALIGN(sizeof(struct xt_standard_target));
	}
}

static int iptables_add_entry(struct connman_iptables *table,
				struct ipt_entry *entry, GList *before,
					int builtin)
{
	struct connman_iptables_entry *e, *entry_before;

	if (!table)
		return -1;

	e = g_try_malloc0(sizeof(struct connman_iptables_entry));
	if (!e)
		return -1;

	e->entry = entry;
	e->builtin = builtin;

	table->entries = g_list_insert_before(table->entries, before, e);
	table->num_entries++;
	table->size += entry->next_offset;

	if (!before) {
		e->offset = table->size - entry->next_offset;

		return 0;
	}

	entry_before = before->data;

	/*
	 * We've just appended/insterted a new entry. All references
	 * should be bumped accordingly.
	 */
	update_targets_reference(table, entry_before, e, false);

	update_offsets(table);

	return 0;
}

static int remove_table_entry(struct connman_iptables *table,
				struct connman_iptables_entry *entry)
{
	int removed = 0;

	table->num_entries--;
	table->size -= entry->entry->next_offset;
	removed = entry->entry->next_offset;

	table->entries = g_list_remove(table->entries, entry);

	g_free(entry->entry);
	g_free(entry);

	return removed;
}

static void delete_update_hooks(struct connman_iptables *table,
				int builtin, GList *chain_head,
				int removed)
{
	struct connman_iptables_entry *e;
	GList *list;

	e = chain_head->data;
	e->builtin = builtin;

	table->underflow[builtin] -= removed;

	for (list = chain_head->next; list; list = list->next) {
		e = list->data;

		if (e->builtin < 0)
			continue;

		table->hook_entry[e->builtin] -= removed;
		table->underflow[e->builtin] -= removed;
	}
}

static int iptables_flush_chain(struct connman_iptables *table,
						const char *name)
{
	GList *chain_head, *chain_tail, *list, *next;
	struct connman_iptables_entry *entry;
	int builtin, removed = 0;

	DBG("table %s chain %s", table->name, name);

	chain_head = find_chain_head(table, name);
	if (!chain_head)
		return -EINVAL;

	chain_tail = find_chain_tail(table, name);
	if (!chain_tail)
		return -EINVAL;

	entry = chain_head->data;
	builtin = entry->builtin;

	if (builtin >= 0)
		list = chain_head;
	else
		list = chain_head->next;

	if (list == chain_tail->prev)
		return 0;

	while (list != chain_tail->prev) {
		entry = list->data;
		next = g_list_next(list);

		removed += remove_table_entry(table, entry);

		list = next;
	}

	if (builtin >= 0)
		delete_update_hooks(table, builtin, chain_tail->prev, removed);

	update_offsets(table);

	return 0;
}

static int iptables_add_chain(struct connman_iptables *table,
				const char *name)
{
	GList *last;
	struct ipt_entry *entry_head;
	struct ipt_entry *entry_return;
	struct error_target *error;
	struct ipt_standard_target *standard;
	u_int16_t entry_head_size, entry_return_size;

	DBG("table %s chain %s", table->name, name);

	last = g_list_last(table->entries);

	/*
	 * An empty chain is composed of:
	 * - A head entry, with no match and an error target.
	 *   The error target data is the chain name.
	 * - A tail entry, with no match and a standard target.
	 *   The standard target verdict is XT_RETURN (return to the
	 *   caller).
	 */

	/* head entry */
	entry_head_size = ALIGN(sizeof(struct ipt_entry)) +
			ALIGN(sizeof(struct error_target));
	entry_head = g_try_malloc0(entry_head_size);
	if (!entry_head)
		goto err_head;

	entry_head->target_offset = ALIGN(sizeof(struct ipt_entry));
	entry_head->next_offset = entry_head_size;

	error = (struct error_target *) entry_head->elems;
	g_stpcpy(error->t.u.user.name, IPT_ERROR_TARGET);
	error->t.u.user.target_size = ALIGN(sizeof(struct error_target));
	g_stpcpy(error->error, name);

	if (iptables_add_entry(table, entry_head, last, -1) < 0)
		goto err_head;

	/* tail entry */
	entry_return_size = ALIGN(sizeof(struct ipt_entry))+
			ALIGN(sizeof(struct ipt_standard_target));
	entry_return = g_try_malloc0(entry_return_size);
	if (!entry_return)
		goto err;

	entry_return->target_offset = ALIGN(sizeof(struct ipt_entry));
	entry_return->next_offset = entry_return_size;

	standard = (struct ipt_standard_target *) entry_return->elems;
	standard->target.u.user.target_size =
				ALIGN(sizeof(struct ipt_standard_target));
	standard->verdict = XT_RETURN;

	if (iptables_add_entry(table, entry_return, last, -1) < 0)
		goto err;

	return 0;

err:
	g_free(entry_return);
err_head:
	g_free(entry_head);

	return -ENOMEM;
}

static int iptables_delete_chain(struct connman_iptables *table,
					const char *name)
{
	struct connman_iptables_entry *entry;
	GList *chain_head, *chain_tail;

	DBG("table %s chain %s", table->name, name);

	chain_head = find_chain_head(table, name);
	if (!chain_head)
		return -EINVAL;

	entry = chain_head->data;

	/* We cannot remove builtin chain */
	if (entry->builtin >= 0)
		return -EINVAL;

	chain_tail = find_chain_tail(table, name);
	if (!chain_tail)
		return -EINVAL;

	/* Chain must be flushed */
	if (chain_head->next != chain_tail->prev)
		return -EINVAL;

	remove_table_entry(table, entry);

	entry = chain_tail->prev->data;
	remove_table_entry(table, entry);

	update_offsets(table);

	return 0;
}

static struct ipt_entry *new_rule(struct ipt_ip *ip,
		const char *target_name, struct xtables_target *xt_t,
		struct xtables_rule_match *xt_rm)
{
	struct xtables_rule_match *tmp_xt_rm;
	struct ipt_entry *new_entry;
	size_t match_size, target_size;

	match_size = 0;
	for (tmp_xt_rm = xt_rm; tmp_xt_rm; tmp_xt_rm = tmp_xt_rm->next)
		match_size += tmp_xt_rm->match->m->u.match_size;

	if (xt_t)
		target_size = xt_t->t->u.target_size;
	else
		target_size = ALIGN(sizeof(struct xt_standard_target));

	new_entry = g_try_malloc0(ALIGN(sizeof(struct ipt_entry)) +
				target_size + match_size);
	if (!new_entry)
		return NULL;

	memcpy(&new_entry->ip, ip, sizeof(struct ipt_ip));

	new_entry->target_offset = ALIGN(sizeof(struct ipt_entry)) +
							match_size;
	new_entry->next_offset = ALIGN(sizeof(struct ipt_entry)) +
					target_size + match_size;

	match_size = 0;
	for (tmp_xt_rm = xt_rm; tmp_xt_rm;
				tmp_xt_rm = tmp_xt_rm->next) {
		memcpy(new_entry->elems + match_size, tmp_xt_rm->match->m,
					tmp_xt_rm->match->m->u.match_size);
		match_size += tmp_xt_rm->match->m->u.match_size;
	}

	if (xt_t) {
		struct xt_entry_target *entry_target;

		entry_target = ipt_get_target(new_entry);
		memcpy(entry_target, xt_t->t, target_size);
	}

	return new_entry;
}

static void update_hooks(struct connman_iptables *table, GList *chain_head,
				struct ipt_entry *entry)
{
	GList *list;
	struct connman_iptables_entry *head, *e;
	int builtin;

	if (!chain_head)
		return;

	head = chain_head->data;

	builtin = head->builtin;
	if (builtin < 0)
		return;

	table->underflow[builtin] += entry->next_offset;

	for (list = chain_head->next; list; list = list->next) {
		e = list->data;

		builtin = e->builtin;
		if (builtin < 0)
			continue;

		table->hook_entry[builtin] += entry->next_offset;
		table->underflow[builtin] += entry->next_offset;
	}
}

static struct ipt_entry *prepare_rule_inclusion(struct connman_iptables *table,
				struct ipt_ip *ip, const char *chain_name,
				const char *target_name,
				struct xtables_target *xt_t,
				int *builtin, struct xtables_rule_match *xt_rm,
				bool insert)
{
	GList *chain_tail, *chain_head;
	struct ipt_entry *new_entry;
	struct connman_iptables_entry *head;

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return NULL;

	chain_tail = find_chain_tail(table, chain_name);
	if (!chain_tail)
		return NULL;

	new_entry = new_rule(ip, target_name, xt_t, xt_rm);
	if (!new_entry)
		return NULL;

	update_hooks(table, chain_head, new_entry);

	/*
	 * If the chain is builtin, and does not have any rule,
	 * then the one that we're inserting is becoming the head
	 * and thus needs the builtin flag.
	 */
	head = chain_head->data;
	if (head->builtin < 0)
		*builtin = -1;
	else if (insert || chain_head == chain_tail->prev) {
		*builtin = head->builtin;
		head->builtin = -1;
	}

	return new_entry;
}

static int iptables_append_rule(struct connman_iptables *table,
				struct ipt_ip *ip, const char *chain_name,
				const char *target_name,
				struct xtables_target *xt_t,
				struct xtables_rule_match *xt_rm)
{
	struct ipt_entry *new_entry;
	int builtin = -1, ret;
	GList *chain_tail;

	DBG("table %s chain %s", table->name, chain_name);

	chain_tail = find_chain_tail(table, chain_name);
	if (!chain_tail)
		return -EINVAL;

	new_entry = prepare_rule_inclusion(table, ip, chain_name,
				target_name, xt_t, &builtin, xt_rm, false);
	if (!new_entry)
		return -EINVAL;

	ret = iptables_add_entry(table, new_entry, chain_tail->prev, builtin);
	if (ret < 0)
		g_free(new_entry);

	return ret;
}

static int iptables_insert_rule(struct connman_iptables *table,
				struct ipt_ip *ip, const char *chain_name,
				const char *target_name,
				struct xtables_target *xt_t,
				struct xtables_rule_match *xt_rm)
{
	struct ipt_entry *new_entry;
	int builtin = -1, ret;
	GList *chain_head;

	DBG("table %s chain %s", table->name, chain_name);

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return -EINVAL;

	new_entry = prepare_rule_inclusion(table, ip, chain_name,
				target_name, xt_t, &builtin, xt_rm, true);
	if (!new_entry)
		return -EINVAL;

	if (builtin == -1)
		chain_head = chain_head->next;

	ret = iptables_add_entry(table, new_entry, chain_head, builtin);
	if (ret < 0)
		g_free(new_entry);

	return ret;
}

static bool is_same_ipt_entry(struct ipt_entry *i_e1,
					struct ipt_entry *i_e2)
{
	if (memcmp(&i_e1->ip, &i_e2->ip, sizeof(struct ipt_ip)) != 0)
		return false;

	if (i_e1->target_offset != i_e2->target_offset)
		return false;

	if (i_e1->next_offset != i_e2->next_offset)
		return false;

	return true;
}

static bool is_same_target(struct xt_entry_target *xt_e_t1,
					struct xt_entry_target *xt_e_t2)
{
	unsigned int i;

	if (!xt_e_t1 || !xt_e_t2)
		return false;

	if (g_strcmp0(xt_e_t1->u.user.name, "") == 0 &&
			g_strcmp0(xt_e_t2->u.user.name, "") == 0) {
		/* fallthrough */
		return true;
	} else if (g_strcmp0(xt_e_t1->u.user.name, IPT_STANDARD_TARGET) == 0) {
		struct xt_standard_target *xt_s_t1;
		struct xt_standard_target *xt_s_t2;

		xt_s_t1 = (struct xt_standard_target *) xt_e_t1;
		xt_s_t2 = (struct xt_standard_target *) xt_e_t2;

		if (xt_s_t1->verdict != xt_s_t2->verdict)
			return false;
	} else {
		if (xt_e_t1->u.target_size != xt_e_t2->u.target_size)
			return false;

		if (g_strcmp0(xt_e_t1->u.user.name, xt_e_t2->u.user.name) != 0)
			return false;

		for (i = 0; i < xt_e_t1->u.target_size -
				sizeof(struct xt_standard_target); i++) {
			if ((xt_e_t1->data[i] ^ xt_e_t2->data[i]) != 0)
				return false;
		}
	}

	return true;
}

static bool is_same_match(struct xt_entry_match *xt_e_m1,
				struct xt_entry_match *xt_e_m2)
{
	unsigned int i;

	if (!xt_e_m1 || !xt_e_m2)
		return false;

	if (xt_e_m1->u.match_size != xt_e_m2->u.match_size)
		return false;

	if (xt_e_m1->u.user.revision != xt_e_m2->u.user.revision)
		return false;

	if (g_strcmp0(xt_e_m1->u.user.name, xt_e_m2->u.user.name) != 0)
		return false;

	for (i = 0; i < xt_e_m1->u.match_size - sizeof(struct xt_entry_match);
			i++) {
		if ((xt_e_m1->data[i] ^ xt_e_m2->data[i]) != 0)
			return false;
	}

	return true;
}

static GList *find_existing_rule(struct connman_iptables *table,
				struct ipt_ip *ip, const char *chain_name,
				const char *target_name,
				struct xtables_target *xt_t,
				GList *matches,
				struct xtables_rule_match *xt_rm)
{
	GList *chain_tail, *chain_head, *list;
	struct xt_entry_target *xt_e_t = NULL;
	struct xt_entry_match *xt_e_m = NULL;
	struct connman_iptables_entry *entry;
	struct ipt_entry *entry_test;
	int builtin;

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return NULL;

	chain_tail = find_chain_tail(table, chain_name);
	if (!chain_tail)
		return NULL;

	if (!xt_t && !matches)
		return NULL;

	entry_test = new_rule(ip, target_name, xt_t, xt_rm);
	if (!entry_test)
		return NULL;

	if (xt_t)
		xt_e_t = ipt_get_target(entry_test);
	if (matches)
		xt_e_m = (struct xt_entry_match *)entry_test->elems;

	entry = chain_head->data;
	builtin = entry->builtin;

	if (builtin >= 0)
		list = chain_head;
	else
		list = chain_head->next;

	for (; list != chain_tail->prev; list = list->next) {
		struct connman_iptables_entry *tmp;
		struct ipt_entry *tmp_e;

		tmp = list->data;
		tmp_e = tmp->entry;

		if (!is_same_ipt_entry(entry_test, tmp_e))
			continue;

		if (xt_t) {
			struct xt_entry_target *tmp_xt_e_t;

			tmp_xt_e_t = ipt_get_target(tmp_e);

			if (!is_same_target(tmp_xt_e_t, xt_e_t))
				continue;
		}

		if (matches) {
			struct xt_entry_match *tmp_xt_e_m;

			tmp_xt_e_m = (struct xt_entry_match *)tmp_e->elems;

			if (!is_same_match(tmp_xt_e_m, xt_e_m))
				continue;
		}

		break;
	}

	g_free(entry_test);

	if (list != chain_tail->prev)
		return list;

	return NULL;
}

static int iptables_delete_rule(struct connman_iptables *table,
				struct ipt_ip *ip, const char *chain_name,
				const char *target_name,
				struct xtables_target *xt_t,
				GList *matches,
				struct xtables_rule_match *xt_rm)
{
	struct connman_iptables_entry *entry;
	GList *chain_head, *chain_tail, *list;
	int builtin, removed;


	DBG("table %s chain %s", table->name, chain_name);

	removed = 0;

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return -EINVAL;

	chain_tail = find_chain_tail(table, chain_name);
	if (!chain_tail)
		return -EINVAL;

	list = find_existing_rule(table, ip, chain_name, target_name,
							xt_t, matches, xt_rm);
	if (!list)
		return -EINVAL;

	entry = chain_head->data;
	builtin = entry->builtin;

	if (builtin >= 0 && list == chain_head) {
		/*
		 * We are about to remove the first rule in the
		 * chain. In this case we need to store the builtin
		 * value to the new chain_head.
		 *
		 * Note, for builtin chains, chain_head->next is
		 * always valid. A builtin chain has always a policy
		 * rule at the end.
		 */
		chain_head = chain_head->next;

		entry = chain_head->data;
		entry->builtin = builtin;
	}

	entry = list->data;
	if (!entry)
		return -EINVAL;

	/* We have deleted a rule,
	 * all references should be bumped accordingly */
	if (list->next)
		update_targets_reference(table, list->next->data,
						list->data, true);

	removed += remove_table_entry(table, entry);

	if (builtin >= 0)
		delete_update_hooks(table, builtin, chain_head, removed);

	update_offsets(table);

	return 0;
}

static int iptables_change_policy(struct connman_iptables *table,
				const char *chain_name, const char *policy)
{
	GList *chain_head, *chain_tail;
	struct connman_iptables_entry *entry;
	struct xt_entry_target *target;
	struct xt_standard_target *t;
	int verdict;

	DBG("table %s chain %s policy %s", table->name, chain_name, policy);

	verdict = target_to_verdict(policy);
	switch (verdict) {
	case -NF_ACCEPT - 1:
	case -NF_DROP - 1:
		break;
	default:
		return -EINVAL;
	}

	chain_head = find_chain_head(table, chain_name);
	if (!chain_head)
		return -EINVAL;

	entry = chain_head->data;
	if (entry->builtin < 0)
		return -EINVAL;

	chain_tail = find_chain_tail(table, chain_name);
	if (!chain_tail)
		return -EINVAL;

	entry = chain_tail->prev->data;
	target = ipt_get_target(entry->entry);

	t = (struct xt_standard_target *)target;
	t->verdict = verdict;

	return 0;
}

static struct ipt_replace *iptables_blob(struct connman_iptables *table)
{
	struct ipt_replace *r;
	GList *list;
	struct connman_iptables_entry *e;
	unsigned char *entry_index;

	r = g_try_malloc0(sizeof(struct ipt_replace) + table->size);
	if (!r)
		return NULL;

	memset(r, 0, sizeof(*r) + table->size);

	r->counters = g_try_malloc0(sizeof(struct xt_counters)
				* table->old_entries);
	if (!r->counters) {
		g_free(r);
		return NULL;
	}

	g_stpcpy(r->name, table->info->name);
	r->num_entries = table->num_entries;
	r->size = table->size;

	r->num_counters = table->old_entries;
	r->valid_hooks  = table->info->valid_hooks;

	memcpy(r->hook_entry, table->hook_entry, sizeof(table->hook_entry));
	memcpy(r->underflow, table->underflow, sizeof(table->underflow));

	entry_index = (unsigned char *)r->entries;
	for (list = table->entries; list; list = list->next) {
		e = list->data;

		memcpy(entry_index, e->entry, e->entry->next_offset);
		entry_index += e->entry->next_offset;
	}

	return r;
}

static void dump_ip(struct ipt_entry *entry)
{
	struct ipt_ip *ip = &entry->ip;
	char ip_string[INET6_ADDRSTRLEN];
	char ip_mask[INET6_ADDRSTRLEN];

	if (strlen(ip->iniface))
		DBG("\tin %s", ip->iniface);

	if (strlen(ip->outiface))
		DBG("\tout %s", ip->outiface);

	if (inet_ntop(AF_INET, &ip->src, ip_string, INET6_ADDRSTRLEN) &&
			inet_ntop(AF_INET, &ip->smsk, ip_mask,
				INET6_ADDRSTRLEN))
		DBG("\tsrc %s/%s", ip_string, ip_mask);

	if (inet_ntop(AF_INET, &ip->dst, ip_string, INET6_ADDRSTRLEN) &&
			inet_ntop(AF_INET, &ip->dmsk, ip_mask,
				INET6_ADDRSTRLEN))
		DBG("\tdst %s/%s", ip_string, ip_mask);
}

static void dump_target(struct ipt_entry *entry)

{
	struct xtables_target *xt_t;
	struct xt_entry_target *target;

	target = ipt_get_target(entry);

	if (!g_strcmp0(target->u.user.name, IPT_STANDARD_TARGET)) {
		struct xt_standard_target *t;

		t = (struct xt_standard_target *)target;

		switch (t->verdict) {
		case XT_RETURN:
			DBG("\ttarget RETURN");
			break;

		case -NF_ACCEPT - 1:
			DBG("\ttarget ACCEPT");
			break;

		case -NF_DROP - 1:
			DBG("\ttarget DROP");
			break;

		case -NF_QUEUE - 1:
			DBG("\ttarget QUEUE");
			break;

		case -NF_STOP - 1:
			DBG("\ttarget STOP");
			break;

		default:
			DBG("\tJUMP %u", t->verdict);
			break;
		}

		xt_t = xtables_find_target(IPT_STANDARD_TARGET,
						XTF_LOAD_MUST_SUCCEED);

		if (xt_t->print)
			xt_t->print(NULL, target, 1);
	} else {
		xt_t = xtables_find_target(target->u.user.name, XTF_TRY_LOAD);
		if (!xt_t) {
			DBG("\ttarget %s", target->u.user.name);
			return;
		}

		if (xt_t->print) {
			DBG("\ttarget ");
			xt_t->print(NULL, target, 1);
		}
	}

	if (xt_t == xt_t->next)
		free(xt_t);
}

static void dump_match(struct ipt_entry *entry)
{
	struct xtables_match *xt_m;
	struct xt_entry_match *match;

	if (entry->elems == (unsigned char *)entry + entry->target_offset)
		return;

	match = (struct xt_entry_match *) entry->elems;

	if (!strlen(match->u.user.name))
		return;

	xt_m = xtables_find_match(match->u.user.name, XTF_TRY_LOAD, NULL);
	if (!xt_m)
		goto out;

	if (xt_m->print) {
		DBG("\tmatch ");
		xt_m->print(NULL, match, 1);

		return;
	}
	if (xt_m == xt_m->next)
		free(xt_m);

out:
	DBG("\tmatch %s", match->u.user.name);

}

static int dump_entry(struct ipt_entry *entry, int builtin,
			unsigned int hook, size_t size, unsigned int offset,
			void *user_data)
{
	struct xt_entry_target *target;

	target = ipt_get_target(entry);

	if (offset + entry->next_offset == size) {
		DBG("\tEnd of CHAIN");
		return 0;
	}

	if (!g_strcmp0(target->u.user.name, IPT_ERROR_TARGET)) {
		DBG("\tUSER CHAIN (%s) match %p  target %p",
			target->data, entry->elems,
			(char *)entry + entry->target_offset);

		return 0;
	} else if (builtin >= 0) {
		DBG("\tCHAIN (%s) match %p  target %p",
			hooknames[builtin], entry->elems,
			(char *)entry + entry->target_offset);
	} else {
		DBG("\tRULE  match %p  target %p",
			entry->elems,
			(char *)entry + entry->target_offset);
	}

	dump_match(entry);
	dump_target(entry);
	dump_ip(entry);

	return 0;
}

static void dump_table(struct connman_iptables *table)
{
	DBG("%s valid_hooks=0x%08x, num_entries=%u, size=%u",
			table->info->name,
			table->info->valid_hooks, table->info->num_entries,
				table->info->size);

	DBG("entry hook: pre/in/fwd/out/post %d/%d/%d/%d/%d",
		table->info->hook_entry[NF_IP_PRE_ROUTING],
		table->info->hook_entry[NF_IP_LOCAL_IN],
		table->info->hook_entry[NF_IP_FORWARD],
		table->info->hook_entry[NF_IP_LOCAL_OUT],
		table->info->hook_entry[NF_IP_POST_ROUTING]);
	DBG("underflow:  pre/in/fwd/out/post %d/%d/%d/%d/%d",
		table->info->underflow[NF_IP_PRE_ROUTING],
		table->info->underflow[NF_IP_LOCAL_IN],
		table->info->underflow[NF_IP_FORWARD],
		table->info->underflow[NF_IP_LOCAL_OUT],
		table->info->underflow[NF_IP_POST_ROUTING]);

	iterate_entries(table->blob_entries->entrytable,
			table->info->valid_hooks,
			table->info->hook_entry,
			table->info->underflow,
			table->blob_entries->size,
			print_entry, dump_entry);
}

static void dump_ipt_replace(struct ipt_replace *repl)
{
	DBG("%s valid_hooks 0x%08x  num_entries %u  size %u",
			repl->name, repl->valid_hooks, repl->num_entries,
			repl->size);

	DBG("entry hook: pre/in/fwd/out/post %d/%d/%d/%d/%d",
		repl->hook_entry[NF_IP_PRE_ROUTING],
		repl->hook_entry[NF_IP_LOCAL_IN],
		repl->hook_entry[NF_IP_FORWARD],
		repl->hook_entry[NF_IP_LOCAL_OUT],
		repl->hook_entry[NF_IP_POST_ROUTING]);
	DBG("underflow:  pre/in/fwd/out/post %d/%d/%d/%d/%d",
		repl->underflow[NF_IP_PRE_ROUTING],
		repl->underflow[NF_IP_LOCAL_IN],
		repl->underflow[NF_IP_FORWARD],
		repl->underflow[NF_IP_LOCAL_OUT],
		repl->underflow[NF_IP_POST_ROUTING]);

	iterate_entries(repl->entries, repl->valid_hooks,
			repl->hook_entry, repl->underflow,
			repl->size, print_entry, dump_entry);
}

static int iptables_get_entries(struct connman_iptables *table)
{
	socklen_t entry_size;
	int err;

	entry_size = sizeof(struct ipt_get_entries) + table->info->size;

	err = getsockopt(table->ipt_sock, IPPROTO_IP, IPT_SO_GET_ENTRIES,
				table->blob_entries, &entry_size);
	if (err < 0)
		return -errno;

	return 0;
}

static int iptables_replace(struct connman_iptables *table,
					struct ipt_replace *r)
{
	int err;

	err = setsockopt(table->ipt_sock, IPPROTO_IP, IPT_SO_SET_REPLACE, r,
			sizeof(*r) + r->size);
	if (err < 0)
		return -errno;

	return 0;
}

static int add_entry(struct ipt_entry *entry, int builtin, unsigned int hook,
			size_t size, unsigned offset, void *user_data)
{
	struct connman_iptables *table = user_data;
	struct ipt_entry *new_entry;

	new_entry = g_try_malloc0(entry->next_offset);
	if (!new_entry)
		return -ENOMEM;

	memcpy(new_entry, entry, entry->next_offset);

	return iptables_add_entry(table, new_entry, NULL, builtin);
}

static void table_cleanup(struct connman_iptables *table)
{
	GList *list;
	struct connman_iptables_entry *entry;

	if (!table)
		return;

	if (table->ipt_sock >= 0)
		close(table->ipt_sock);

	for (list = table->entries; list; list = list->next) {
		entry = list->data;

		g_free(entry->entry);
		g_free(entry);
	}

	g_list_free(table->entries);
	g_free(table->name);
	g_free(table->info);
	g_free(table->blob_entries);
	g_free(table);
}

static struct connman_iptables *iptables_init(const char *table_name)
{
	struct connman_iptables *table = NULL;
	char *module = NULL;
	socklen_t s;

	DBG("%s", table_name);

	if (xtables_insmod("ip_tables", NULL, TRUE) != 0)
		DBG("ip_tables module loading gives error but trying anyway");

	module = g_strconcat("iptable_", table_name, NULL);
	if (!module)
		return NULL;

	if (xtables_insmod(module, NULL, TRUE) != 0)
		DBG("%s module loading gives error but trying anyway", module);

	g_free(module);

	table = g_try_new0(struct connman_iptables, 1);
	if (!table)
		return NULL;

	table->info = g_try_new0(struct ipt_getinfo, 1);
	if (!table->info)
		goto err;

	table->ipt_sock = socket(AF_INET, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_RAW);
	if (table->ipt_sock < 0)
		goto err;

	s = sizeof(*table->info);
	g_stpcpy(table->info->name, table_name);
	if (getsockopt(table->ipt_sock, IPPROTO_IP, IPT_SO_GET_INFO,
						table->info, &s) < 0) {
		connman_error("iptables support missing error %d (%s)", errno,
			strerror(errno));
		goto err;
	}

	table->blob_entries = g_try_malloc0(sizeof(struct ipt_get_entries) +
						table->info->size);
	if (!table->blob_entries)
		goto err;

	g_stpcpy(table->blob_entries->name, table_name);
	table->blob_entries->size = table->info->size;

	if (iptables_get_entries(table) < 0)
		goto err;

	table->num_entries = 0;
	table->old_entries = table->info->num_entries;
	table->size = 0;

	memcpy(table->underflow, table->info->underflow,
				sizeof(table->info->underflow));
	memcpy(table->hook_entry, table->info->hook_entry,
				sizeof(table->info->hook_entry));

	iterate_entries(table->blob_entries->entrytable,
			table->info->valid_hooks, table->info->hook_entry,
			table->info->underflow, table->blob_entries->size,
			add_entry, table);

	if (debug_enabled)
		dump_table(table);

	return table;

err:
	table_cleanup(table);

	return NULL;
}

static struct option iptables_opts[] = {
	{.name = "append",        .has_arg = 1, .val = 'A'},
	{.name = "compare",       .has_arg = 1, .val = 'C'},
	{.name = "delete",        .has_arg = 1, .val = 'D'},
	{.name = "flush-chain",   .has_arg = 1, .val = 'F'},
	{.name = "insert",        .has_arg = 1, .val = 'I'},
	{.name = "list",          .has_arg = 2, .val = 'L'},
	{.name = "new-chain",     .has_arg = 1, .val = 'N'},
	{.name = "policy",        .has_arg = 1, .val = 'P'},
	{.name = "delete-chain",  .has_arg = 1, .val = 'X'},
	{.name = "destination",   .has_arg = 1, .val = 'd'},
	{.name = "in-interface",  .has_arg = 1, .val = 'i'},
	{.name = "jump",          .has_arg = 1, .val = 'j'},
	{.name = "match",         .has_arg = 1, .val = 'm'},
	{.name = "out-interface", .has_arg = 1, .val = 'o'},
	{.name = "source",        .has_arg = 1, .val = 's'},
	{.name = "table",         .has_arg = 1, .val = 't'},
	{NULL},
};

struct xtables_globals iptables_globals = {
	.option_offset = 0,
	.opts = iptables_opts,
	.orig_opts = iptables_opts,
};

static struct xtables_target *prepare_target(struct connman_iptables *table,
							const char *target_name)
{
	struct xtables_target *xt_t = NULL;
	bool is_builtin, is_user_defined;
	GList *chain_head = NULL;
	size_t target_size;

	is_builtin = false;
	is_user_defined = false;

	if (is_builtin_target(target_name))
		is_builtin = true;
	else {
		chain_head = find_chain_head(table, target_name);
		if (chain_head && chain_head->next)
			is_user_defined = true;
	}

	if (is_builtin || is_user_defined)
		xt_t = xtables_find_target(IPT_STANDARD_TARGET,
						XTF_LOAD_MUST_SUCCEED);
	else
		xt_t = xtables_find_target(target_name, XTF_TRY_LOAD);

	if (!xt_t)
		return NULL;

	target_size = ALIGN(sizeof(struct ipt_entry_target)) + xt_t->size;

	xt_t->t = g_try_malloc0(target_size);
	if (!xt_t->t)
		return NULL;

	xt_t->t->u.target_size = target_size;

	if (is_builtin || is_user_defined) {
		struct xt_standard_target *target;

		target = (struct xt_standard_target *)(xt_t->t);
		g_stpcpy(target->target.u.user.name, IPT_STANDARD_TARGET);

		if (is_builtin)
			target->verdict = target_to_verdict(target_name);
		else if (is_user_defined) {
			struct connman_iptables_entry *target_rule;

			target_rule = chain_head->next->data;
			target->verdict = target_rule->offset;
		}
	} else {
		g_stpcpy(xt_t->t->u.user.name, target_name);
		xt_t->t->u.user.revision = xt_t->revision;
		if (xt_t->init)
			xt_t->init(xt_t->t);
	}

	if (xt_t->x6_options)
		iptables_globals.opts =
			xtables_options_xfrm(
				iptables_globals.orig_opts,
				iptables_globals.opts,
				xt_t->x6_options,
				&xt_t->option_offset);
	else
		iptables_globals.opts =
			xtables_merge_options(
				iptables_globals.orig_opts,
				iptables_globals.opts,
				xt_t->extra_opts,
				&xt_t->option_offset);

	if (!iptables_globals.opts) {
		g_free(xt_t->t);
		xt_t = NULL;
	}

	return xt_t;
}

static struct xtables_match *prepare_matches(struct connman_iptables *table,
					struct xtables_rule_match **xt_rm,
					const char *match_name)
{
	struct xtables_match *xt_m;
	size_t match_size;

	if (!match_name)
		return NULL;

	xt_m = xtables_find_match(match_name, XTF_LOAD_MUST_SUCCEED, xt_rm);
	match_size = ALIGN(sizeof(struct ipt_entry_match)) + xt_m->size;

	xt_m->m = g_try_malloc0(match_size);
	if (!xt_m->m)
		return NULL;

	xt_m->m->u.match_size = match_size;
	g_stpcpy(xt_m->m->u.user.name, xt_m->name);
	xt_m->m->u.user.revision = xt_m->revision;

	if (xt_m->init)
		xt_m->init(xt_m->m);

	if (xt_m->x6_options)
		iptables_globals.opts =
			xtables_options_xfrm(
				iptables_globals.orig_opts,
				iptables_globals.opts,
				xt_m->x6_options,
				&xt_m->option_offset);
	else
			iptables_globals.opts =
			xtables_merge_options(
				iptables_globals.orig_opts,
				iptables_globals.opts,
				xt_m->extra_opts,
				&xt_m->option_offset);

	if (!iptables_globals.opts) {
		g_free(xt_m->m);

		if (xt_m == xt_m->next)
			free(xt_m);

		xt_m = NULL;
	}

	return xt_m;
}

static int parse_ip_and_mask(const char *str, struct in_addr *ip,
				struct in_addr *mask)
{
	char **tokens;
	uint32_t prefixlength;
	uint32_t tmp;
	int err;

	tokens = g_strsplit(str, "/", 2);
	if (!tokens)
		return -1;

	if (!inet_pton(AF_INET, tokens[0], ip)) {
		err = -1;
		goto out;
	}

	if (tokens[1]) {
		prefixlength = strtol(tokens[1], NULL, 10);
		if (prefixlength > 31) {
			err = -1;
			goto out;
		}

		tmp = ~(0xffffffff >> prefixlength);
	} else {
		tmp = 0xffffffff;
	}

	mask->s_addr = htonl(tmp);
	ip->s_addr = ip->s_addr & mask->s_addr;
	err = 0;
out:
	g_strfreev(tokens);

	return err;
}

static struct connman_iptables *get_table(const char *table_name)
{
	struct connman_iptables *table;

	if (!table_name)
		table_name = "filter";

	table = g_hash_table_lookup(table_hash, table_name);
	if (table)
		return table;

	table = iptables_init(table_name);
	if (!table)
		return NULL;

	table->name = g_strdup(table_name);
	g_hash_table_replace(table_hash, table->name, table);

	return table;
}

struct parse_context {
	int argc;
	char **argv;
	struct ipt_ip *ip;
	struct xtables_target *xt_t;
	GList *xt_m;
	struct xtables_rule_match *xt_rm;
};

static int prepare_getopt_args(const char *str, struct parse_context *ctx)
{
	char **tokens;
	int i;

	tokens = g_strsplit_set(str, " ", -1);

	i = g_strv_length(tokens);

	/* Add space for the argv[0] value */
	ctx->argc = i + 1;

	/* Don't forget the last NULL entry */
	ctx->argv = g_try_malloc0((ctx->argc + 1) * sizeof(char *));
	if (!ctx->argv) {
		g_strfreev(tokens);
		return -ENOMEM;
	}

	/*
	 * getopt_long() jumps over the first token; we need to add some
	 * random argv[0] entry.
	 */
	ctx->argv[0] = g_strdup("argh");
	for (i = 1; i < ctx->argc; i++)
		ctx->argv[i] = tokens[i - 1];

	g_free(tokens);

	return 0;
}

static int parse_xt_modules(int c, bool invert,
				struct parse_context *ctx)
{
	struct xtables_match *m;
	struct xtables_rule_match *rm;

	for (rm = ctx->xt_rm; rm; rm = rm->next) {
		if (rm->completed != 0)
			continue;

		m = rm->match;

		if (!m->x6_parse && !m->parse)
			continue;

		if (c < (int) m->option_offset ||
				c >= (int) m->option_offset
					+ XT_OPTION_OFFSET_SCALE)
			continue;

		xtables_option_mpcall(c, ctx->argv, invert, m, NULL);
	}

	if (!ctx->xt_t)
		return 0;

	if (!ctx->xt_t->x6_parse && !ctx->xt_t->parse)
		return 0;

	if (c < (int) ctx->xt_t->option_offset ||
			c >= (int) ctx->xt_t->option_offset
					+ XT_OPTION_OFFSET_SCALE)
		return 0;

	xtables_option_tpcall(c, ctx->argv, invert, ctx->xt_t, NULL);

	return 0;
}

static int final_check_xt_modules(struct parse_context *ctx)
{
	struct xtables_rule_match *rm;

	for (rm = ctx->xt_rm; rm; rm = rm->next)
		xtables_option_mfcall(rm->match);

	if (ctx->xt_t)
		xtables_option_tfcall(ctx->xt_t);

	return 0;
}

static int parse_rule_spec(struct connman_iptables *table,
				struct parse_context *ctx)
{
	/*
	 * How the parser works:
	 *
	 *  - If getopt finds 's', 'd', 'i', 'o'.
	 *    just extract the information.
	 *  - if '!' is found, set the invert flag to true and
	 *    removes the '!' from the optarg string and jumps
	 *    back to getopt to reparse the current optarg string.
	 *    After reparsing the invert flag is reseted to false.
	 *  - If 'm' or 'j' is found then call either
	 *    prepare_matches() or prepare_target(). Those function
	 *    will modify (extend) the longopts for getopt_long.
	 *    That means getopt will change its matching context according
	 *    the loaded target.
	 *
	 *    Here an example with iptables-test
	 *
	 *    argv[0] = ./tools/iptables-test
	 *    argv[1] = -t
	 *    argv[2] = filter
	 *    argv[3] = -A
	 *    argv[4] = INPUT
	 *    argv[5] = -m
	 *    argv[6] = mark
	 *    argv[7] = --mark
	 *    argv[8] = 999
	 *    argv[9] = -j
	 *    argv[10] = LOG
	 *
	 *    getopt found 'm' then the optarg is "mark" and optind 7
	 *    The longopts array containts before hitting the `case 'm'`
	 *
	 *    val A has_arg 1 name append
	 *    val C has_arg 1 name compare
	 *    val D has_arg 1 name delete
	 *    val F has_arg 1 name flush-chain
	 *    val I has_arg 1 name insert
	 *    val L has_arg 2 name list
	 *    val N has_arg 1 name new-chain
	 *    val P has_arg 1 name policy
	 *    val X has_arg 1 name delete-chain
	 *    val d has_arg 1 name destination
	 *    val i has_arg 1 name in-interface
	 *    val j has_arg 1 name jump
	 *    val m has_arg 1 name match
	 *    val o has_arg 1 name out-interface
	 *    val s has_arg 1 name source
	 *    val t has_arg 1 name table
	 *
	 *    After executing the `case 'm'` block longopts is
	 *
	 *    val A has_arg 1 name append
	 *    val C has_arg 1 name compare
	 *    val D has_arg 1 name delete
	 *    val F has_arg 1 name flush-chain
	 *    val I has_arg 1 name insert
	 *    val L has_arg 2 name list
	 *    val N has_arg 1 name new-chain
	 *    val P has_arg 1 name policy
	 *    val X has_arg 1 name delete-chain
	 *    val d has_arg 1 name destination
	 *    val i has_arg 1 name in-interface
	 *    val j has_arg 1 name jump
	 *    val m has_arg 1 name match
	 *    val o has_arg 1 name out-interface
	 *    val s has_arg 1 name source
	 *    val t has_arg 1 name table
	 *    val   has_arg 1 name mark
	 *
	 *    So the 'mark' matcher has added the 'mark' options
	 *    and getopt will then return c '256' optarg "999" optind 9
	 *    And we will hit the 'default' statement which then
	 *    will call the matchers parser (xt_m->parser() or
	 *    xtables_option_mpcall() depending on which version
	 *    of libxtables is found.
	 */
	struct xtables_match *xt_m;
	bool invert = false;
	int len, c, err;

	ctx->ip = g_try_new0(struct ipt_ip, 1);
	if (!ctx->ip)
		return -ENOMEM;

	/*
	 * Tell getopt_long not to generate error messages for unknown
	 * options and also reset optind back to 0.
	 */
	opterr = 0;
	optind = 0;

	while ((c = getopt_long(ctx->argc, ctx->argv,
					"-:d:i:o:s:m:j:",
					iptables_globals.opts, NULL)) != -1) {
		switch (c) {
		case 's':
			/* Source specification */
			if (!parse_ip_and_mask(optarg,
						&ctx->ip->src,
						&ctx->ip->smsk))
				break;

			if (invert)
				ctx->ip->invflags |= IPT_INV_SRCIP;

			break;
		case 'd':
			/* Destination specification */
			if (!parse_ip_and_mask(optarg,
						&ctx->ip->dst,
						&ctx->ip->dmsk))
				break;

			if (invert)
				ctx->ip->invflags |= IPT_INV_DSTIP;
			break;
		case 'i':
			/* In interface specification */
			len = strlen(optarg);

			if (len + 1 > IFNAMSIZ)
				break;

			g_stpcpy(ctx->ip->iniface, optarg);
			memset(ctx->ip->iniface_mask, 0xff, len + 1);

			if (invert)
				ctx->ip->invflags |= IPT_INV_VIA_IN;

			break;
		case 'o':
			/* Out interface specification */
			len = strlen(optarg);

			if (len + 1 > IFNAMSIZ)
				break;

			g_stpcpy(ctx->ip->outiface, optarg);
			memset(ctx->ip->outiface_mask, 0xff, len + 1);

			if (invert)
				ctx->ip->invflags |= IPT_INV_VIA_OUT;

			break;
		case 'm':
			/* Matches */
			xt_m = prepare_matches(table, &ctx->xt_rm, optarg);
			if (!xt_m) {
				err = -EINVAL;
				goto out;
			}
			ctx->xt_m = g_list_append(ctx->xt_m, xt_m);

			break;
		case 'j':
			/* Target */
			ctx->xt_t = prepare_target(table, optarg);
			if (!ctx->xt_t) {
				err = -EINVAL;
				goto out;
			}

			break;
		case 1:
			if (optarg[0] == '!' && optarg[1] == '\0') {
				invert = true;

				/* Remove the '!' from the optarg */
				optarg[0] = '\0';

				/*
				 * And recall getopt_long without reseting
				 * invert.
				 */
				continue;
			}

			break;
		default:
			err = parse_xt_modules(c, invert, ctx);
			if (err == 1)
				continue;

			break;
		}

		invert = false;
	}

	err = final_check_xt_modules(ctx);

out:
	return err;
}

static void reset_xtables(void)
{
	struct xtables_match *xt_m;
	struct xtables_target *xt_t;

	/*
	 * As side effect parsing a rule sets some global flags
	 * which will be evaluated/verified. Let's reset them
	 * to ensure we can parse more than one rule.
	 *
	 * Clear all flags because the flags are only valid
	 * for one rule.
	 */
	for (xt_m = xtables_matches; xt_m; xt_m = xt_m->next)
		xt_m->mflags = 0;

	for (xt_t = xtables_targets; xt_t; xt_t = xt_t->next) {
		xt_t->tflags = 0;
		xt_t->used = 0;
	}

	/*
	 * We need also to free the memory implicitly allocated
	 * during parsing (see xtables_options_xfrm()).
	 * Note xt_params is actually iptables_globals.
	 */
	if (xt_params->opts != xt_params->orig_opts) {
		g_free(xt_params->opts);
		xt_params->opts = xt_params->orig_opts;
	}
	xt_params->option_offset = 0;
}

static void cleanup_parse_context(struct parse_context *ctx)
{
	return 0;
	struct xtables_rule_match *rm, *tmp;
	GList *list;

	g_strfreev(ctx->argv);
	g_free(ctx->ip);
	if (ctx->xt_t) {
		g_free(ctx->xt_t->t);
		ctx->xt_t->t = NULL;
	}

	for (list = ctx->xt_m; list; list = list->next) {
		struct xtables_match *xt_m = list->data;

		g_free(xt_m->m);

		if (xt_m != xt_m->next)
			continue;

		g_free(xt_m);
	}
	g_list_free(ctx->xt_m);

	for (tmp = NULL, rm = ctx->xt_rm; rm; rm = rm->next) {
		if (tmp)
			g_free(tmp);
		tmp = rm;
	}
	g_free(tmp);

	g_free(ctx);
}

int __connman_iptables_dump(const char *table_name)
{
	return 0;
	struct connman_iptables *table;

	DBG("-t %s -L", table_name);

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	dump_table(table);

	return 0;
}

int __connman_iptables_new_chain(const char *table_name,
					const char *chain)
{
	return 0;
	struct connman_iptables *table;

	DBG("-t %s -N %s", table_name, chain);

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	return iptables_add_chain(table, chain);
}

int __connman_iptables_delete_chain(const char *table_name,
					const char *chain)
{
	return 0;
	struct connman_iptables *table;

	DBG("-t %s -X %s", table_name, chain);

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	return iptables_delete_chain(table, chain);
}

int __connman_iptables_flush_chain(const char *table_name,
					const char *chain)
{
	return 0;
	struct connman_iptables *table;

	DBG("-t %s -F %s", table_name, chain);

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	return iptables_flush_chain(table, chain);
}

int __connman_iptables_change_policy(const char *table_name,
					const char *chain,
					const char *policy)
{
	return 0;
	struct connman_iptables *table;

	DBG("-t %s -F %s", table_name, chain);

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	return iptables_change_policy(table, chain, policy);
}

int __connman_iptables_append(const char *table_name,
				const char *chain,
				const char *rule_spec)
{
	return 0;
	struct connman_iptables *table;
	struct parse_context *ctx;
	const char *target_name;
	int err;

	ctx = g_try_new0(struct parse_context, 1);
	if (!ctx)
		return -ENOMEM;

	DBG("-t %s -A %s %s", table_name, chain, rule_spec);

	err = prepare_getopt_args(rule_spec, ctx);
	if (err < 0)
		goto out;

	table = get_table(table_name);
	if (!table) {
		err = -EINVAL;
		goto out;
	}

	err = parse_rule_spec(table, ctx);
	if (err < 0)
		goto out;

	if (!ctx->xt_t)
		target_name = NULL;
	else
		target_name = ctx->xt_t->name;

	err = iptables_append_rule(table, ctx->ip, chain,
				target_name, ctx->xt_t, ctx->xt_rm);
out:
	cleanup_parse_context(ctx);
	reset_xtables();

	return err;
}

int __connman_iptables_insert(const char *table_name,
				const char *chain,
				const char *rule_spec)
{
	return 0;
	struct connman_iptables *table;
	struct parse_context *ctx;
	const char *target_name;
	int err;

	ctx = g_try_new0(struct parse_context, 1);
	if (!ctx)
		return -ENOMEM;

	DBG("-t %s -I %s %s", table_name, chain, rule_spec);

	err = prepare_getopt_args(rule_spec, ctx);
	if (err < 0)
		goto out;

	table = get_table(table_name);
	if (!table) {
		err = -EINVAL;
		goto out;
	}

	err = parse_rule_spec(table, ctx);
	if (err < 0)
		goto out;

	if (!ctx->xt_t)
		target_name = NULL;
	else
		target_name = ctx->xt_t->name;

	err = iptables_insert_rule(table, ctx->ip, chain,
				target_name, ctx->xt_t, ctx->xt_rm);
out:
	cleanup_parse_context(ctx);
	reset_xtables();

	return err;
}

int __connman_iptables_delete(const char *table_name,
				const char *chain,
				const char *rule_spec)
{
	return 0;
	struct connman_iptables *table;
	struct parse_context *ctx;
	const char *target_name;
	int err;

	ctx = g_try_new0(struct parse_context, 1);
	if (!ctx)
		return -ENOMEM;

	DBG("-t %s -D %s %s", table_name, chain, rule_spec);

	err = prepare_getopt_args(rule_spec, ctx);
	if (err < 0)
		goto out;

	table = get_table(table_name);
	if (!table) {
		err = -EINVAL;
		goto out;
	}

	err = parse_rule_spec(table, ctx);
	if (err < 0)
		goto out;

	if (!ctx->xt_t)
		target_name = NULL;
	else
		target_name = ctx->xt_t->name;

	err = iptables_delete_rule(table, ctx->ip, chain,
				target_name, ctx->xt_t, ctx->xt_m,
				ctx->xt_rm);
out:
	cleanup_parse_context(ctx);
	reset_xtables();

	return err;
}

int __connman_iptables_commit(const char *table_name)
{
	return 0;
	struct connman_iptables *table;
	struct ipt_replace *repl;
	int err;

	DBG("%s", table_name);

	table = g_hash_table_lookup(table_hash, table_name);
	if (!table)
		return -EINVAL;

	repl = iptables_blob(table);

	if (debug_enabled)
		dump_ipt_replace(repl);

	err = iptables_replace(table, repl);

	g_free(repl->counters);
	g_free(repl);

	if (err < 0)
		return err;

	g_hash_table_remove(table_hash, table_name);

	return 0;
}

static void remove_table(gpointer user_data)
{
	struct connman_iptables *table = user_data;

	table_cleanup(table);
}

static int iterate_chains_cb(struct ipt_entry *entry, int builtin,
				unsigned int hook, size_t size,
				unsigned int offset, void *user_data)
{
	struct cb_data *cbd = user_data;
	connman_iptables_iterate_chains_cb_t cb = cbd->cb;
	struct xt_entry_target *target;

	if (offset + entry->next_offset == size)
		return 0;

	target = ipt_get_target(entry);

	if (!g_strcmp0(target->u.user.name, IPT_ERROR_TARGET))
		(*cb)((const char *)target->data, cbd->user_data);
	else if (builtin >= 0)
		(*cb)(hooknames[builtin], cbd->user_data);

	return 0;
}

int __connman_iptables_iterate_chains(const char *table_name,
				connman_iptables_iterate_chains_cb_t cb,
				void *user_data)
{
	return 0;
	struct cb_data *cbd = cb_data_new(cb, user_data);
	struct connman_iptables *table;

	table = get_table(table_name);
	if (!table)
		return -EINVAL;

	iterate_entries(table->blob_entries->entrytable,
			table->info->valid_hooks,
			table->info->hook_entry,
			table->info->underflow,
			table->blob_entries->size,
			iterate_chains_cb, cbd);

	g_free(cbd);

	return 0;
}

int __connman_iptables_init(void)
{
	return 0;
	DBG("");

	if (getenv("CONNMAN_IPTABLES_DEBUG"))
		debug_enabled = true;

	table_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, remove_table);

	xtables_init_all(&iptables_globals, NFPROTO_IPV4);

	return 0;
}

void __connman_iptables_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(table_hash);
}
