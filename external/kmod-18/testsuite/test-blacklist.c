/*
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <libkmod.h>

/* good luck bulding a kmod_list outside of the library... makes this blacklist
 * function rather pointless */
#include <libkmod-internal.h>

/* FIXME: hack, change name so we don't clash */
#undef ERR
#include "testsuite.h"

static int blacklist_1(const struct test *t)
{
	struct kmod_ctx *ctx;
	struct kmod_list *list = NULL, *l, *filtered;
	struct kmod_module *mod;
	int err;
	size_t len = 0;

	const char *names[] = { "pcspkr", "pcspkr2", "floppy", "ext4", NULL };
	const char **name;

	ctx = kmod_new(NULL, NULL);
	if (ctx == NULL)
		exit(EXIT_FAILURE);

	for(name = names; *name; name++) {
		err = kmod_module_new_from_name(ctx, *name, &mod);
		if (err < 0)
			goto fail_lookup;
		list = kmod_list_append(list, mod);
	}

	err = kmod_module_apply_filter(ctx, KMOD_FILTER_BLACKLIST, list,
								&filtered);
	if (err < 0) {
		ERR("Could not filter: %s\n", strerror(-err));
		goto fail;
	}
	if (filtered == NULL) {
		ERR("All modules were filtered out!\n");
		goto fail;
	}

	kmod_list_foreach(l, filtered) {
		const char *modname;
		mod = kmod_module_get_module(l);
		modname = kmod_module_get_name(mod);
		if (strcmp("pcspkr", modname) == 0 || strcmp("floppy", modname) == 0)
			goto fail;
		len++;
		kmod_module_unref(mod);
	}

	if (len != 2)
		goto fail;

	kmod_module_unref_list(filtered);
	kmod_module_unref_list(list);
	kmod_unref(ctx);

	return EXIT_SUCCESS;

fail:
	kmod_module_unref_list(list);
fail_lookup:
	kmod_unref(ctx);
	return EXIT_FAILURE;
}
static const struct test sblacklist_1 = {
	.name = "blacklist_1",
	.description = "check if modules are correctly blacklisted",
	.func = blacklist_1,
	.config = {
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-blacklist/",
	},
	.need_spawn = true,
};

static const struct test *tests[] = {
	&sblacklist_1,
	NULL,
};

TESTSUITE_MAIN(tests);
