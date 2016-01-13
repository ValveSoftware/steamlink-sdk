/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  BMW Car IT GmbH.
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

#include <errno.h>

#include <xtables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include "connman.h"

#define CHAIN_PREFIX "connman-"

static const char *builtin_chains[] = {
	[NF_IP_PRE_ROUTING]	= "PREROUTING",
	[NF_IP_LOCAL_IN]	= "INPUT",
	[NF_IP_FORWARD]		= "FORWARD",
	[NF_IP_LOCAL_OUT]	= "OUTPUT",
	[NF_IP_POST_ROUTING]	= "POSTROUTING",
};

struct connman_managed_table {
	char *name;
	unsigned int chains[NF_INET_NUMHOOKS];
};

struct fw_rule {
	char *table;
	char *chain;
	char *rule_spec;
};

struct firewall_context {
	GList *rules;
};

static GSList *managed_tables;

static bool firewall_is_up;

static int chain_to_index(const char *chain_name)
{
	if (!g_strcmp0(builtin_chains[NF_IP_PRE_ROUTING], chain_name))
		return NF_IP_PRE_ROUTING;
	if (!g_strcmp0(builtin_chains[NF_IP_LOCAL_IN], chain_name))
		return NF_IP_LOCAL_IN;
	if (!g_strcmp0(builtin_chains[NF_IP_FORWARD], chain_name))
		return NF_IP_FORWARD;
	if (!g_strcmp0(builtin_chains[NF_IP_LOCAL_OUT], chain_name))
		return NF_IP_LOCAL_OUT;
	if (!g_strcmp0(builtin_chains[NF_IP_POST_ROUTING], chain_name))
		return NF_IP_POST_ROUTING;

	return -1;
}

static int managed_chain_to_index(const char *chain_name)
{
	if (!g_str_has_prefix(chain_name, CHAIN_PREFIX))
		return -1;

	return chain_to_index(chain_name + strlen(CHAIN_PREFIX));
}

static int insert_managed_chain(const char *table_name, int id)
{
	char *rule, *managed_chain;
	int err;

	managed_chain = g_strdup_printf("%s%s", CHAIN_PREFIX,
					builtin_chains[id]);

	err = __connman_iptables_new_chain(table_name, managed_chain);
	if (err < 0)
		goto out;

	rule = g_strdup_printf("-j %s", managed_chain);
	err = __connman_iptables_insert(table_name, builtin_chains[id], rule);
	g_free(rule);
	if (err < 0) {
		__connman_iptables_delete_chain(table_name, managed_chain);
		goto out;
	}

out:
	g_free(managed_chain);

	return err;
}

static int delete_managed_chain(const char *table_name, int id)
{
	char *rule, *managed_chain;
	int err;

	managed_chain = g_strdup_printf("%s%s", CHAIN_PREFIX,
					builtin_chains[id]);

	rule = g_strdup_printf("-j %s", managed_chain);
	err = __connman_iptables_delete(table_name, builtin_chains[id], rule);
	g_free(rule);

	if (err < 0)
		goto out;

	err =  __connman_iptables_delete_chain(table_name, managed_chain);

out:
	g_free(managed_chain);

	return err;
}

static int insert_managed_rule(const char *table_name,
				const char *chain_name,
				const char *rule_spec)
{
	struct connman_managed_table *mtable = NULL;
	GSList *list;
	char *chain;
	int id, err;

	id = chain_to_index(chain_name);
	if (id < 0) {
		/* This chain is not managed */
		chain = g_strdup(chain_name);
		goto out;
	}

	for (list = managed_tables; list; list = list->next) {
		mtable = list->data;

		if (g_strcmp0(mtable->name, table_name) == 0)
			break;

		mtable = NULL;
	}

	if (!mtable) {
		mtable = g_new0(struct connman_managed_table, 1);
		mtable->name = g_strdup(table_name);

		managed_tables = g_slist_prepend(managed_tables, mtable);
	}

	if (mtable->chains[id] == 0) {
		DBG("table %s add managed chain for %s",
			table_name, chain_name);

		err = insert_managed_chain(table_name, id);
		if (err < 0)
			return err;
	}

	mtable->chains[id]++;
	chain = g_strdup_printf("%s%s", CHAIN_PREFIX, chain_name);

out:
	err = __connman_iptables_append(table_name, chain, rule_spec);

	g_free(chain);

	return err;
 }

static int delete_managed_rule(const char *table_name,
				const char *chain_name,
				const char *rule_spec)
 {
	struct connman_managed_table *mtable = NULL;
	GSList *list;
	int id, err;
	char *managed_chain;

	id = chain_to_index(chain_name);
	if (id < 0) {
		/* This chain is not managed */
		return __connman_iptables_delete(table_name, chain_name,
							rule_spec);
	}

	managed_chain = g_strdup_printf("%s%s", CHAIN_PREFIX, chain_name);

	err = __connman_iptables_delete(table_name, managed_chain,
					rule_spec);

	for (list = managed_tables; list; list = list->next) {
		mtable = list->data;

		if (g_strcmp0(mtable->name, table_name) == 0)
			break;

		mtable = NULL;
	}

	if (!mtable) {
		err = -ENOENT;
		goto out;
	}

	mtable->chains[id]--;
	if (mtable->chains[id] > 0)
		goto out;

	DBG("table %s remove managed chain for %s",
			table_name, chain_name);

	err = delete_managed_chain(table_name, id);

 out:
	g_free(managed_chain);

	return err;
}

static void cleanup_managed_table(gpointer user_data)
{
	struct connman_managed_table *table = user_data;

	g_free(table->name);
	g_free(table);
}

static void cleanup_fw_rule(gpointer user_data)
{
	struct fw_rule *rule = user_data;

	g_free(rule->rule_spec);
	g_free(rule->chain);
	g_free(rule->table);
	g_free(rule);
}

struct firewall_context *__connman_firewall_create(void)
{
	struct firewall_context *ctx;

	ctx = g_new0(struct firewall_context, 1);

	return ctx;
}

void __connman_firewall_destroy(struct firewall_context *ctx)
{
	g_list_free_full(ctx->rules, cleanup_fw_rule);
	g_free(ctx);
}

int __connman_firewall_add_rule(struct firewall_context *ctx,
				const char *table,
				const char *chain,
				const char *rule_fmt, ...)
{
	va_list args;
	char *rule_spec;
	struct fw_rule *rule;

	va_start(args, rule_fmt);

	rule_spec = g_strdup_vprintf(rule_fmt, args);

	va_end(args);

	rule = g_new0(struct fw_rule, 1);

	rule->table = g_strdup(table);
	rule->chain = g_strdup(chain);
	rule->rule_spec = rule_spec;

	ctx->rules = g_list_append(ctx->rules, rule);

	return 0;
}

static int firewall_disable(GList *rules)
{
	struct fw_rule *rule;
	GList *list;
	int err;

	for (list = rules; list; list = g_list_previous(list)) {
		rule = list->data;

		err = delete_managed_rule(rule->table,
						rule->chain, rule->rule_spec);
		if (err < 0) {
			connman_error("Cannot remove previously installed "
				"iptables rules: %s", strerror(-err));
			return err;
		}

		err = __connman_iptables_commit(rule->table);
		if (err < 0) {
			connman_error("Cannot remove previously installed "
				"iptables rules: %s", strerror(-err));
			return err;
		}
	}

	return 0;
}

int __connman_firewall_enable(struct firewall_context *ctx)
{
	struct fw_rule *rule;
	GList *list;
	int err;

	for (list = g_list_first(ctx->rules); list;
			list = g_list_next(list)) {
		rule = list->data;

		DBG("%s %s %s", rule->table, rule->chain, rule->rule_spec);

		err = insert_managed_rule(rule->table,
						rule->chain, rule->rule_spec);
		if (err < 0)
			goto err;

		err = __connman_iptables_commit(rule->table);
		if (err < 0)
			goto err;
	}

	firewall_is_up = true;

	return 0;

err:
	connman_warn("Failed to install iptables rules: %s", strerror(-err));

	firewall_disable(g_list_previous(list));

	return err;
}

int __connman_firewall_disable(struct firewall_context *ctx)
{
	return firewall_disable(g_list_last(ctx->rules));
}

bool __connman_firewall_is_up(void)
{
	return firewall_is_up;
}

static void iterate_chains_cb(const char *chain_name, void *user_data)
{
	GSList **chains = user_data;
	int id;

	id = managed_chain_to_index(chain_name);
	if (id < 0)
		return;

	*chains = g_slist_prepend(*chains, GINT_TO_POINTER(id));
}

static void flush_table(const char *table_name)
{
	GSList *chains = NULL, *list;
	char *rule, *managed_chain;
	int id, err;

	__connman_iptables_iterate_chains(table_name, iterate_chains_cb,
						&chains);

	for (list = chains; list; list = list->next) {
		id = GPOINTER_TO_INT(list->data);

		managed_chain = g_strdup_printf("%s%s", CHAIN_PREFIX,
						builtin_chains[id]);

		rule = g_strdup_printf("-j %s", managed_chain);
		err = __connman_iptables_delete(table_name,
						builtin_chains[id], rule);
		if (err < 0) {
			connman_warn("Failed to delete jump rule '%s': %s",
				rule, strerror(-err));
		}
		g_free(rule);

		err = __connman_iptables_flush_chain(table_name, managed_chain);
		if (err < 0) {
			connman_warn("Failed to flush chain '%s': %s",
				managed_chain, strerror(-err));
		}
		err = __connman_iptables_delete_chain(table_name, managed_chain);
		if (err < 0) {
			connman_warn("Failed to delete chain '%s': %s",
				managed_chain, strerror(-err));
		}

		g_free(managed_chain);
	}

	err = __connman_iptables_commit(table_name);
	if (err < 0) {
		connman_warn("Failed to flush table '%s': %s",
			table_name, strerror(-err));
	}

	g_slist_free(chains);
}

static void flush_all_tables(void)
{
	/* Flush the tables ConnMan might have modified
	 * But do so if only ConnMan has done something with
	 * iptables */

	if (!g_file_test("/proc/net/ip_tables_names",
			G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		firewall_is_up = false;
		return;
	}

	firewall_is_up = true;

	flush_table("filter");
	flush_table("mangle");
	flush_table("nat");
}

int __connman_firewall_init(void)
{
	DBG("");

	flush_all_tables();

	return 0;
}

void __connman_firewall_cleanup(void)
{
	DBG("");

	g_slist_free_full(managed_tables, cleanup_managed_table);
}
