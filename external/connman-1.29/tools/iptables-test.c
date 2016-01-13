/*
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>

#include "../src/connman.h"

enum iptables_command {
	IPTABLES_COMMAND_APPEND,
	IPTABLES_COMMAND_INSERT,
	IPTABLES_COMMAND_DELETE,
	IPTABLES_COMMAND_POLICY,
	IPTABLES_COMMAND_CHAIN_INSERT,
	IPTABLES_COMMAND_CHAIN_DELETE,
	IPTABLES_COMMAND_CHAIN_FLUSH,
	IPTABLES_COMMAND_DUMP,
	IPTABLES_COMMAND_UNKNOWN,
};

int main(int argc, char *argv[])
{
	enum iptables_command cmd = IPTABLES_COMMAND_UNKNOWN;
	char *table = NULL, *chain = NULL, *rule = NULL, *tmp;
	int err, c, i;

	opterr = 0;

	while ((c = getopt_long(argc, argv,
				"-A:I:D:P:N:X:F:Lt:", NULL, NULL)) != -1) {
		switch (c) {
		case 'A':
			chain = optarg;
			cmd = IPTABLES_COMMAND_APPEND;
			break;
		case 'I':
			chain = optarg;
			cmd = IPTABLES_COMMAND_INSERT;
			break;
		case 'D':
			chain = optarg;
			cmd = IPTABLES_COMMAND_DELETE;
			break;
		case 'P':
			chain = optarg;
			/* The policy will be stored in rule. */
			cmd = IPTABLES_COMMAND_POLICY;
			break;
		case 'N':
			chain = optarg;
			cmd = IPTABLES_COMMAND_CHAIN_INSERT;
			break;
		case 'X':
			chain = optarg;
			cmd = IPTABLES_COMMAND_CHAIN_DELETE;
			break;
		case 'F':
			chain = optarg;
			cmd = IPTABLES_COMMAND_CHAIN_FLUSH;
			break;
		case 'L':
			cmd = IPTABLES_COMMAND_DUMP;
			break;
		case 't':
			table = optarg;
			break;
		default:
			goto out;
		}
	}

out:
	if (!table)
		table = "filter";

	for (i = optind - 1; i < argc; i++) {
		if (rule) {
			tmp = rule;
			rule = g_strdup_printf("%s %s", rule,  argv[i]);
			g_free(tmp);
		} else
			rule = g_strdup(argv[i]);
	}

	__connman_iptables_init();

	switch (cmd) {
	case IPTABLES_COMMAND_APPEND:
		err = __connman_iptables_append(table, chain, rule);
		break;
	case IPTABLES_COMMAND_INSERT:
		err = __connman_iptables_insert(table, chain, rule);
		break;
	case IPTABLES_COMMAND_DELETE:
		err = __connman_iptables_delete(table, chain, rule);
		break;
	case IPTABLES_COMMAND_POLICY:
		err = __connman_iptables_change_policy(table, chain, rule);
		break;
	case IPTABLES_COMMAND_CHAIN_INSERT:
		err = __connman_iptables_new_chain(table, chain);
		break;
	case IPTABLES_COMMAND_CHAIN_DELETE:
		err = __connman_iptables_delete_chain(table, chain);
		break;
	case IPTABLES_COMMAND_CHAIN_FLUSH:
		err = __connman_iptables_flush_chain(table, chain);
		break;
	case IPTABLES_COMMAND_DUMP:
		__connman_log_init(argv[0], "*", false, false,
			"iptables-test", "1");
		err = __connman_iptables_dump(table);
		break;
	case IPTABLES_COMMAND_UNKNOWN:
		printf("Missing command\n");
		printf("usage: iptables-test [-t table] {-A|-I|-D} chain rule\n");
		printf("       iptables-test [-t table] {-N|-X|-F} chain\n");
		printf("       iptables-test [-t table] -L\n");
		printf("       iptables-test [-t table] -P chain target\n");
		exit(-EINVAL);
	}

	if (err < 0) {
		printf("Error: %s\n", strerror(-err));
		exit(err);
	}

	err = __connman_iptables_commit(table);
	if (err < 0) {
		printf("Failed to commit changes: %s\n", strerror(-err));
		exit(err);
	}

	g_free(rule);

	__connman_iptables_cleanup();

	return 0;
}
