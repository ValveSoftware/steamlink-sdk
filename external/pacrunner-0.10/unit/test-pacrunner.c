/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010-2012  Intel Corporation. All rights reserved.
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

#include <glib.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>
#include <CUnit/Console.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "pacrunner.h"

enum test_suite_part {
	SUITE_TITLE        = 0,
	SUITE_PAC          = 1,
	SUITE_SERVERS      = 2,
	SUITE_EXCLUDES     = 3,
	SUITE_BROWSER_ONLY = 4,
	SUITE_DOMAINS      = 5,
	SUITE_CONFIG       = 6,
	SUITE_TESTS        = 7,
	SUITE_NOTHING      = 8,
};

enum cu_test_mode {
	CU_MODE_BASIC   = 0,
	CU_MODE_AUTO    = 1,
	CU_MODE_CONSOLE = 2,
};

struct pacrunner_test_suite {
	gchar *title;
	gchar *pac;
	gchar **servers;
	gchar **excludes;
	gboolean browser_only;
	gchar **domains;

	bool config_result;

	gchar **tests;
};

static struct pacrunner_test_suite *test_suite;
static bool verbose = false;

static struct pacrunner_proxy *proxy, *proxy2 = NULL, *proxy3 = NULL;
static bool test_config;

static void free_pacrunner_test_suite(struct pacrunner_test_suite *suite)
{
	if (!suite)
		return;

	g_free(suite->title);
	g_free(suite->pac);
	g_strfreev(suite->servers);
	g_strfreev(suite->excludes);
	g_strfreev(suite->domains);
	g_strfreev(suite->tests);

	g_free(suite);
}

static gchar **_g_strappendv(gchar **str_array, const gchar *str)
{
	int length = 0;
	gchar **result;
	gchar *copy;

	if (!str)
		return NULL;

	if (str_array)
		length = g_strv_length(str_array);

	result = g_try_malloc0(sizeof(gchar *) * (length + 2));
	if (!result)
		return NULL;

	copy = g_strdup(str);
	if (!copy) {
		g_free(result);
		return NULL;
	}

	if (str_array) {
		memmove(result, str_array, length * sizeof(gchar *));
		memset(str_array, 0, length * sizeof(gchar *));
	}

	result[length] = copy;

	return result;
}

static void print_test_suite(struct pacrunner_test_suite *suite)
{
	gchar **line;

	if (!suite)
		return;

	printf("\nSuite: %s\n", suite->title);

	printf("\nPAC:\n%s\n", suite->pac);

	printf("\nServers:\n");
	if (suite->servers) {
		for (line = suite->servers; *line; line++)
			printf("%s\n", *line);
	} else
		printf("(none)\n");


	printf("\nExcludes:\n");
	if (suite->excludes) {
		for (line = suite->excludes; *line; line++)
			printf("%s\n", *line);
	} else
		printf("(none)\n");

	printf("\nBrowser Only: %s\n",
			suite->browser_only ? "TRUE" : "FALSE");

	printf("\nDomains:\n");
	if (suite->domains) {
		for (line = suite->domains; *line; line++)
			printf("%s\n", *line);
	} else
		printf("(none)\n");

	printf("\nConfig result: %s\n",
			suite->config_result ? "Valid" : "Invalid");

	printf("\nTests:\n");
	if (suite->tests) {
		short test = 0;

		for (line = suite->tests; *line; line++) {
			if (test == 0) {
				printf("%s --> ", *line);
				test++;
			} else {
				printf("%s\n", *line);
				test = 0;
			}
		}

		printf("\n");
	} else
		printf("(none)\n");
}

static struct pacrunner_test_suite *read_test_suite(const char *path)
{
	enum test_suite_part part = SUITE_NOTHING;
	struct pacrunner_test_suite *suite;
	gchar *content = NULL;
	gchar **lines = NULL;
	gchar **array;
	gchar **line;

	suite = g_try_malloc0(sizeof(struct pacrunner_test_suite));
	if (!suite)
		goto error;

	if (!g_file_get_contents(path, &content, NULL, NULL))
		goto error;

	if (strlen(content) <= 0)
		goto error;

	lines = g_strsplit(content, "\n", 0);
	if (!lines)
		goto error;

	for (line = lines; *line; line++) {
		if (strlen(*line) == 0)
			continue;

		if (*line[0] == '#')
			continue;

		if (*line[0] != '[') {
			switch (part) {
			case SUITE_TITLE:
				if (suite->title)
					goto error;

				suite->title = g_strdup(*line);

				if (!suite->title)
					goto error;

				break;
			case SUITE_PAC:
				if (!suite->pac)
					suite->pac = g_strdup_printf("%s\n",
									*line);
				else {
					gchar *oldpac = suite->pac;

					suite->pac = g_strdup_printf("%s%s\n",
								oldpac, *line);
					g_free(oldpac);
				}

				if (!suite->pac)
					goto error;

				break;
			case SUITE_SERVERS:
				array = _g_strappendv(suite->servers, *line);
				if (!array)
					goto error;

				g_free(suite->servers);
				suite->servers = array;

				break;
			case SUITE_EXCLUDES:
				array = _g_strappendv(suite->excludes, *line);
				if (!array)
					goto error;

				g_free(suite->excludes);
				suite->excludes = array;

				break;
			case SUITE_BROWSER_ONLY:
				if (strncmp(*line, "TRUE", 4) == 0)
					suite->browser_only = TRUE;
				else
					suite->browser_only = FALSE;

				break;
			case SUITE_DOMAINS:
				array = _g_strappendv(suite->domains, *line);
				if (!array)
					goto error;

				g_free(suite->domains);
				suite->domains = array;

				break;
			case SUITE_CONFIG:
				if (strncmp(*line, "VALID", 5) == 0)
					suite->config_result = true;
				else
					suite->config_result = false;

				break;
			case SUITE_TESTS:
				array = _g_strappendv(suite->tests, *line);
				if (!array)
					goto error;

				g_free(suite->tests);
				suite->tests = array;

				break;
			case SUITE_NOTHING:
			default:
				break;
			}

			continue;
		}

		if (strncmp(*line, "[title]", 7) == 0)
			part = SUITE_TITLE;
		else if (strncmp(*line, "[pac]", 5) == 0)
			part = SUITE_PAC;
		else if (strncmp(*line, "[servers]", 9) == 0)
			part = SUITE_SERVERS;
		else if (strncmp(*line, "[excludes]", 10) == 0)
			part = SUITE_EXCLUDES;
		else if (strncmp(*line, "[browseronly]", 13) == 0)
			part = SUITE_BROWSER_ONLY;
		else if (strncmp(*line, "[domains]", 9) == 0)
			part = SUITE_DOMAINS;
		else if (strncmp(*line, "[config]", 8) == 0)
			part = SUITE_CONFIG;
		else if (strncmp(*line, "[tests]", 7) == 0)
			part = SUITE_TESTS;
	}

	if (verbose)
		print_test_suite(suite);

	if (!suite->title || (suite->tests
			&& g_strv_length(suite->tests) % 2 != 0)
			|| (!suite->servers && !suite->pac))
		goto error;

	g_free(content);
	g_strfreev(lines);

	return suite;

error:
	g_free(content);
	g_strfreev(lines);

	free_pacrunner_test_suite(suite);

	return NULL;
}

static int test_suite_init(void)
{
	proxy = pacrunner_proxy_create("eth0");
	if (!proxy)
		return -1;

	return 0;
}

static int test_suite_cleanup(void)
{
	if (test_config) {
		if (pacrunner_proxy_disable(proxy) != 0)
			return -1;
	}

	pacrunner_proxy_unref(proxy);

	return 0;
}

static void test_pac_config(void)
{
	if (pacrunner_proxy_set_auto(proxy, NULL, test_suite->pac) == 0)
		test_config = true;

	CU_ASSERT_TRUE(test_suite->config_result == test_config);
}

static void test_manual_config(void)
{
	if (pacrunner_proxy_set_manual(proxy, test_suite->servers,
						test_suite->excludes) == 0)
		test_config = true;

	CU_ASSERT_TRUE(test_suite->config_result == test_config);
}

static void test_proxy_domain(void)
{
	int val = 0;

	if (pacrunner_proxy_set_domains(proxy, test_suite->domains,
					test_suite->browser_only) != 0)
		val = -1;

	proxy2 = pacrunner_proxy_create("eth1");
	if (proxy2) {
		char *servers[] = {
			"http://proxy2.com",
			"https://secproxy2.com",
			NULL};
		char *domains[] = {
			"intel.com",
			"domain2.com",
			"192.168.4.0/16",
			NULL};

		if (pacrunner_proxy_set_manual(proxy2, servers, NULL) != 0)
			val = -1;

		/* BrowserOnly = TRUE */
		if (pacrunner_proxy_set_domains(proxy2, domains, TRUE) != 0)
			val = -1;
	}

	proxy3 = pacrunner_proxy_create("wl0");
	if (proxy3) {
		char *servers[] = {
			"socks4://server.one.com",
			"socks5://server.two.com",
			NULL};
		char *domains[] = {
			"redhat.com",
			"domain3.com",
			"fe80:96db::/32",
			NULL};

		if (pacrunner_proxy_set_manual(proxy3, servers, NULL) != 0)
			val = -1;

		/* BrowserOnly = FALSE */
		if (pacrunner_proxy_set_domains(proxy3, domains, FALSE) != 0)
			val = -1;
	}

	CU_ASSERT_TRUE(val == 0);
}

static void test_proxy_requests(void)
{
	gchar **test_strings;
	bool verify;
	gchar *result;
	gchar **test;
	gchar *msg;

	if (!test_config)
		return;

	if (verbose)
		printf("\n");

	for (test = test_suite->tests; *test; test = test + 2) {
		gchar *test_result = *(test+1);

		test_strings = g_strsplit(*test, " ", 2);
		if (!test_strings || g_strv_length(test_strings) != 2) {
			g_strfreev(test_strings);
			continue;
		}

		result = pacrunner_proxy_lookup(test_strings[0],
						test_strings[1]);
		g_strfreev(test_strings);

		verify = false;

		if (strncmp(test_result, "DIRECT", 6) == 0) {
			if (!result ||
					strncmp(result, "DIRECT", 6) == 0)
				verify = true;
		} else {
			if (g_strcmp0(result, test_result) == 0)
				verify = true;
		}

		if (verbose) {
			if (verify)
				msg = g_strdup_printf(
						"\tTEST: %s -> %s verified",
							*test, test_result);
			else
				msg = g_strdup_printf(
					"\tTEST: %s -> %s FAILED (%s)",
					*test, test_result,
					!result ? "DIRECT" : result);

			printf("%s\n", msg);
			g_free(msg);
		}

		if (result)
			g_free(result);

		CU_ASSERT_TRUE(verify);
	}
}

static void run_test_suite(const char *test_file_path, enum cu_test_mode mode)
{
	CU_pTestRegistry cu_registry;
	CU_pSuite cu_suite;

	if (!test_file_path)
		return;

	test_suite = read_test_suite(test_file_path);
	if (!test_suite) {
		if (verbose)
			printf("Invalid suite\n");
		return;
	}

	if (verbose)
		printf("Valid suite\n");

	cu_registry = CU_create_new_registry();

	cu_registry = CU_set_registry(cu_registry);
	CU_destroy_existing_registry(&cu_registry);

	cu_suite = CU_add_suite(test_suite->title, test_suite_init,
						test_suite_cleanup);

	if (test_suite->pac)
		CU_add_test(cu_suite, "PAC config test", test_pac_config);
	else
		CU_add_test(cu_suite, "Manual config test",
						test_manual_config);

	CU_add_test(cu_suite, "Proxy Domain test", test_proxy_domain);

	if (test_suite->config_result && test_suite->tests)
		CU_add_test(cu_suite, "Proxy requests test",
						test_proxy_requests);

	test_config = false;

	if (proxy2)
		pacrunner_proxy_unref(proxy2);
	if (proxy3)
		pacrunner_proxy_unref(proxy3);

	switch (mode) {
	case CU_MODE_BASIC:
		CU_basic_set_mode(CU_BRM_VERBOSE);

		CU_basic_run_tests();

		break;
	case CU_MODE_AUTO:
		CU_set_output_filename(test_file_path);
		CU_list_tests_to_file();

		CU_automated_run_tests();

		break;
	case CU_MODE_CONSOLE:
		CU_console_run_tests();

		break;
	default:
		break;
	}

	free_pacrunner_test_suite(test_suite);
	test_suite = NULL;
}

static void find_and_run_test_suite(GDir *test_dir,
					const char *test_path,
					const char *file_path,
					enum cu_test_mode mode)
{
	const gchar *test_file;
	gchar *test_file_path;

	if (!test_dir && !test_path)
		return;

	if (CU_initialize_registry() != CUE_SUCCESS) {
		printf("%s\n", CU_get_error_msg());
		return;
	}

	if (test_dir) {
		for (test_file = g_dir_read_name(test_dir); test_file;
				test_file = g_dir_read_name(test_dir)) {
			test_file_path = g_strdup_printf("%s/%s",
						test_path, test_file);
			if (!test_file_path)
				return;

			run_test_suite(test_file_path, mode);

			g_free(test_file_path);
		}
	} else {
		if (test_path)
			test_file_path = g_strdup_printf("%s/%s",
						test_path, file_path);
		else
			test_file_path = g_strdup(file_path);

		run_test_suite(test_file_path, mode);

		g_free(test_file_path);
	}

	CU_cleanup_registry();
}

static GDir *open_test_dir(gchar **test_path)
{
	GDir *test_dir;

	if (!test_path || !*test_path)
		return NULL;

	test_dir = g_dir_open(*test_path, 0, NULL);
	if (!test_dir && !g_path_is_absolute(*test_path)) {
		gchar *current, *path;

		current = g_get_current_dir();

		path = g_strdup_printf("%s/%s", current, *test_path);
		g_free(*test_path);
		g_free(current);

		*test_path = path;

		test_dir = g_dir_open(*test_path, 0, NULL);
		if (!test_dir)
			return NULL;
	}

	return test_dir;
}

static void print_usage()
{
	printf("Usage: test-pacrunner <options>\n"
		"\t--dir, -d directory: specify a directory where test"
		" suites are found\n"
		"\t--file, -f file: specify a test suite file\n"
		"\t--help, -h: print this help\n"
		"\t--mode, -m CUnit mode: basic (default), auto, console\n"
		"\t--verbose, -v: verbose output mode\n");
}

static struct option test_options[] = {
	{"dir",     1, 0, 'd'},
	{"file",    1, 0, 'f'},
	{"help",    0, 0, 'h'},
	{"mode",    1, 0, 'm'},
	{"verbose", 0, 0, 'v'},
	{ NULL }
};

int main(int argc, char *argv[])
{
	enum cu_test_mode mode = CU_MODE_BASIC;
	gchar *test_path = NULL;
	gchar *file_path = NULL;
	GDir *test_dir = NULL;
	int opt_index = 0;
	int c;

	if (argc < 2)
		goto error;

	while ((c = getopt_long(argc, argv, "d:hm:v",
			test_options, &opt_index)) != -1) {
		switch (c) {
		case 'd':
			if (file_path)
				goto error;

			test_path = g_strdup(optarg);

			break;
		case 'f':
			if (test_path)
				goto error;

			file_path = g_strdup(optarg);

			break;
		case 'h':
			print_usage();
			return EXIT_SUCCESS;

			break;
		case 'm':
			if (strncmp(optarg, "basic", 5) == 0)
				mode = CU_MODE_BASIC;
			else if (strncmp(optarg, "auto", 4) == 0)
				mode = CU_MODE_AUTO;
			else if (strncmp(optarg, "console", 6) == 0)
				mode = CU_MODE_CONSOLE;

			break;
		case 'v':
			verbose = true;

			break;
		case '?':
		default:
			goto error;

		}
	}

	if (!test_path && !file_path)
		goto error;

	if (test_path) {
		test_dir = open_test_dir(&test_path);
		if (!test_dir) {
			g_free(test_path);
			return EXIT_FAILURE;
		}
	}

	if (file_path)
		test_path = g_get_current_dir();

	__pacrunner_proxy_init();
	__pacrunner_js_init();
	__pacrunner_manual_init();
	__pacrunner_plugin_init(NULL, NULL);

	find_and_run_test_suite(test_dir, test_path, file_path, mode);

	__pacrunner_plugin_cleanup();
	__pacrunner_manual_cleanup();
	__pacrunner_js_cleanup();
	__pacrunner_proxy_cleanup();

	g_free(test_path);

	if (test_dir)
		g_dir_close(test_dir);

	return EXIT_SUCCESS;

error:
	print_usage();
	return EXIT_FAILURE;
}
