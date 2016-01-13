/*
 * Copyright (C) 2012-2013  ProFUSION embedded systems
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

#include "testsuite.h"

#define MODULES_ORDER_UNAME "3.5.4-1-ARCH"
#define MODULES_ORDER_ROOTFS TESTSUITE_ROOTFS "test-depmod/modules-order-compressed"
#define MODULES_ORDER_LIB_MODULES MODULES_ORDER_ROOTFS "/lib/modules/" MODULES_ORDER_UNAME
static noreturn int depmod_modules_order_for_compressed(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/depmod";
	const char *const args[] = {
		progname,
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(depmod_modules_order_for_compressed,
	.description = "check if depmod let aliases in right order when using compressed modules",
	.config = {
		[TC_UNAME_R] = MODULES_ORDER_UNAME,
		[TC_ROOTFS] = MODULES_ORDER_ROOTFS,
	},
	.output = {
		.files = (const struct keyval[]) {
			{ MODULES_ORDER_LIB_MODULES "/correct-modules.alias",
			  MODULES_ORDER_LIB_MODULES "/modules.alias" },
			{ }
		},
	});

#define SEARCH_ORDER_SIMPLE_ROOTFS TESTSUITE_ROOTFS "test-depmod/search-order-simple"
static noreturn int depmod_search_order_simple(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/depmod";
	const char *const args[] = {
		progname,
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(depmod_search_order_simple,
	.description = "check if depmod honor search order in config",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = SEARCH_ORDER_SIMPLE_ROOTFS,
	},
	.output = {
		.files = (const struct keyval[]) {
			{ SEARCH_ORDER_SIMPLE_ROOTFS "/lib/modules/4.4.4/correct-modules.dep",
			  SEARCH_ORDER_SIMPLE_ROOTFS "/lib/modules/4.4.4/modules.dep" },
			{ }
		},
	});

#define SEARCH_ORDER_SAME_PREFIX_ROOTFS TESTSUITE_ROOTFS "test-depmod/search-order-same-prefix"
static noreturn int depmod_search_order_same_prefix(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/depmod";
	const char *const args[] = {
		progname,
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(depmod_search_order_same_prefix,
	.description = "check if depmod honor search order in config with same prefix",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = SEARCH_ORDER_SAME_PREFIX_ROOTFS,
	},
	.output = {
		.files = (const struct keyval[]) {
			{ SEARCH_ORDER_SAME_PREFIX_ROOTFS "/lib/modules/4.4.4/correct-modules.dep",
			  SEARCH_ORDER_SAME_PREFIX_ROOTFS "/lib/modules/4.4.4/modules.dep" },
			{ }
		},
	});

#define DETECT_LOOP_ROOTFS TESTSUITE_ROOTFS "test-depmod/detect-loop"
static noreturn int depmod_detect_loop(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/depmod";
	const char *const args[] = {
		progname,
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(depmod_detect_loop,
	.description = "check if depmod detects module loops correctly",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = DETECT_LOOP_ROOTFS,
	},
	.expected_fail = true,
	.output = {
		.err = DETECT_LOOP_ROOTFS "/correct.txt",
	});


static const struct test *tests[] = {
#ifdef ENABLE_ZLIB
	&sdepmod_modules_order_for_compressed,
#endif
	&sdepmod_search_order_simple,
	&sdepmod_search_order_same_prefix,
	&sdepmod_detect_loop,
	NULL,
};

TESTSUITE_MAIN(tests);
