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

static noreturn int modprobe_show_depends(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--show-depends", "btusb",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_show_depends,
	.description = "check if output for modprobe --show-depends is correct for loaded modules",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/show-depends",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/show-depends/correct.txt",
	});

static noreturn int modprobe_show_depends2(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--show-depends", "psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_show_depends2,
	.description = "check if output for modprobe --show-depends is correct",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/show-depends",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/show-depends/correct-psmouse.txt",
	});


static noreturn int modprobe_show_alias_to_none(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--show-depends", "--ignore-install", "--quiet", "psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_show_alias_to_none,
	.description = "check if modprobe --show-depends doesn't explode with an alias to nothing",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/alias-to-none",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/show-depends/correct-psmouse.txt",
	},
	.modules_loaded = "",
	);


static noreturn int modprobe_builtin(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"unix",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_builtin,
	.description = "check if modprobe return 0 for builtin",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/builtin",
	});

static noreturn int modprobe_softdep_loop(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"bluetooth",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_softdep_loop,
	.description = "check if modprobe breaks softdep loop",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/softdep-loop",
		[TC_INIT_MODULE_RETCODES] = "",
	},
	.modules_loaded = "btusb,bluetooth",
	);

static noreturn int modprobe_install_cmd_loop(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"snd-pcm",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_install_cmd_loop,
	.description = "check if modprobe breaks install-commands loop",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/install-cmd-loop",
		[TC_INIT_MODULE_RETCODES] = "",
	},
	.env_vars = (const struct keyval[]) {
		{ "MODPROBE", ABS_TOP_BUILDDIR "/tools/modprobe" },
		{ }
		},
	.modules_loaded = "snd,snd-pcm",
	);

static noreturn int modprobe_param_kcmdline(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--show-depends", "psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_param_kcmdline,
	.description = "check if params from kcmdline are passed in fact passed to (f)init_module call",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline/correct.txt",
	},
	.modules_loaded = "",
	);

static noreturn int modprobe_param_kcmdline2(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"-c",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_param_kcmdline2,
	.description = "check if params with no value are parsed correctly from kcmdline",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline2",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline2/correct.txt",
	},
	.modules_loaded = "",
	);

static noreturn int modprobe_param_kcmdline3(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"-c",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_param_kcmdline3,
	.description = "check if unrelated strings in kcmdline are correctly ignored",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline3",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline3/correct.txt",
	},
	.modules_loaded = "",
	);

static noreturn int modprobe_param_kcmdline4(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"-c",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_param_kcmdline4,
	.description = "check if unrelated strings in kcmdline are correctly ignored",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline4",
	},
	.output = {
		.out = TESTSUITE_ROOTFS "test-modprobe/module-param-kcmdline4/correct.txt",
	},
	.modules_loaded = "",
	);

static noreturn int modprobe_force(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--force", "psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_force,
	.description = "check modprobe --force",
	.config = {
		[TC_UNAME_R] = "4.4.4",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/force",
		[TC_INIT_MODULE_RETCODES] = "",
	},
	.modules_loaded = "psmouse",
	);

static noreturn int modprobe_oldkernel(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_oldkernel,
	.description = "check modprobe --force",
	.config = {
		[TC_UNAME_R] = "3.3.3",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/oldkernel",
		[TC_INIT_MODULE_RETCODES] = "",
	},
	.modules_loaded = "psmouse",
	);

static noreturn int modprobe_oldkernel_force(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modprobe";
	const char *const args[] = {
		progname,
		"--force", "psmouse",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modprobe_oldkernel_force,
	.description = "check modprobe --force",
	.config = {
		[TC_UNAME_R] = "3.3.3",
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modprobe/oldkernel-force",
		[TC_INIT_MODULE_RETCODES] = "",
	},
	.modules_loaded = "psmouse",
	);


static const struct test *tests[] = {
	&smodprobe_show_depends,
	&smodprobe_show_depends2,
	&smodprobe_show_alias_to_none,
	&smodprobe_builtin,
	&smodprobe_softdep_loop,
	&smodprobe_install_cmd_loop,
	&smodprobe_param_kcmdline,
	&smodprobe_param_kcmdline2,
	&smodprobe_param_kcmdline3,
	&smodprobe_param_kcmdline4,
	&smodprobe_force,
	&smodprobe_oldkernel,
	&smodprobe_oldkernel_force,
	NULL,
};

TESTSUITE_MAIN(tests);
