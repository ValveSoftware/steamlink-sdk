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

#pragma once

#include <stdbool.h>
#include <stdarg.h>

#include "macro.h"

struct test;
typedef int (*testfunc)(const struct test *t);

enum test_config {
	/*
	 * Where's the roots dir for this test. It will LD_PRELOAD path.so in
	 * order to trap calls to functions using paths.
	 */
	TC_ROOTFS = 0,

	/*
	 * What's the desired string to be returned by `uname -r`. It will
	 * trap calls to uname(3P) by LD_PRELOAD'ing uname.so and then filling
	 * in the information in u.release.
	 */
	TC_UNAME_R,

	/*
	 * Fake calls to init_module(2), returning return-code and setting
	 * errno to err-code. Set this variable with the following format:
	 *
	 *        modname:return-code:err-code
	 *
	 * When this variable is used, all calls to init_module() are trapped
	 * and by default the return code is 0. In other words, they fake
	 * "success" for all modules, except the ones in the list above, for
	 * which the return codes are used.
	 */
	TC_INIT_MODULE_RETCODES,

	/*
	 * Fake calls to delete_module(2), returning return-code and setting
	 * errno to err-code. Set this variable with the following format:
	 *
	 *        modname:return-code:err-code
	 *
	 * When this variable is used, all calls to init_module() are trapped
	 * and by default the return code is 0. In other words, they fake
	 * "success" for all modules, except the ones in the list above, for
	 * which the return codes are used.
	 */
	TC_DELETE_MODULE_RETCODES,

	_TC_LAST,
};

#define S_TC_ROOTFS "TESTSUITE_ROOTFS"
#define S_TC_UNAME_R "TESTSUITE_UNAME_R"
#define S_TC_INIT_MODULE_RETCODES "TESTSUITE_INIT_MODULE_RETCODES"
#define S_TC_DELETE_MODULE_RETCODES "TESTSUITE_DELETE_MODULE_RETCODES"

struct keyval {
	const char *key;
	const char *val;
};

struct test {
	const char *name;
	const char *description;
	struct {
		/* File with correct stdout */
		const char *out;
		/* File with correct stderr */
		const char *err;

		/*
		 * Vector with pair of files
		 * key = correct file
		 * val = file to check
		 */
		const struct keyval *files;
	} output;
	/* comma-separated list of loaded modules at the end of the test */
	const char *modules_loaded;
	testfunc func;
	const char *config[_TC_LAST];
	const char *path;
	const struct keyval *env_vars;
	bool need_spawn;
	bool expected_fail;
};


const struct test *test_find(const struct test *tests[], const char *name);
int test_init(int argc, char *const argv[], const struct test *tests[]);
int test_spawn_prog(const char *prog, const char *const args[]);

int test_run(const struct test *t);

#define TS_EXPORT __attribute__ ((visibility("default")))

#define _LOG(prefix, fmt, ...) printf("TESTSUITE: " prefix fmt, ## __VA_ARGS__)
#define LOG(fmt, ...) _LOG("", fmt, ## __VA_ARGS__)
#define WARN(fmt, ...) _LOG("WARN: ", fmt, ## __VA_ARGS__)
#define ERR(fmt, ...) _LOG("ERR: ", fmt, ## __VA_ARGS__)

#define assert_return(expr, r)						\
	do {								\
		if ((!(expr))) {					\
			ERR("Failed assertion: " #expr "%s:%d %s",			\
			    __FILE__, __LINE__, __PRETTY_FUNCTION__);	\
			return (r);					\
		}							\
	} while (false)


/* Test definitions */
#define DEFINE_TEST(_name, ...) \
	const struct test s##_name = { \
		.name = #_name, \
		.func = _name, \
		## __VA_ARGS__ \
	}

#define TESTSUITE_MAIN(_tests) \
	int main(int argc, char *argv[])			\
	{							\
		const struct test *t;				\
		int arg;					\
		size_t i;					\
								\
		arg = test_init(argc, argv, tests);		\
		if (arg == 0)					\
			return 0;				\
								\
		if (arg < argc) {				\
			t = test_find(tests, argv[arg]);	\
			if (t == NULL) {			\
				fprintf(stderr, "could not find test %s\n", argv[arg]);\
				exit(EXIT_FAILURE);		\
			}					\
								\
			return test_run(t);			\
		}						\
								\
		for (i = 0; tests[i] != NULL; i++) {		\
			if (test_run(tests[i]) != 0)		\
				exit(EXIT_FAILURE);		\
		}						\
								\
		exit(EXIT_SUCCESS);				\
	}							\

#ifdef noreturn
# define __noreturn noreturn
#elif __STDC_VERSION__ >= 201112L
# define __noreturn _Noreturn
#else
# define __noreturn __attribute__((noreturn))
#endif
