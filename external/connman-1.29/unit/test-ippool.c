/*
 *
 *  Connection Manager
 *
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

#include <glib.h>

#include "../src/connman.h"

/* #define DEBUG */
#ifdef DEBUG
#include <stdio.h>

#define LOG(fmt, arg...) do { \
	fprintf(stdout, "%s:%s() " fmt "\n", \
			__FILE__, __func__ , ## arg); \
} while (0)
#else
#define LOG(fmt, arg...)
#endif

static void test_case_1(void)
{
	struct connman_ippool *pool;
	int i;

	__connman_ippool_init();

	pool = __connman_ippool_create(23, 1, 500, NULL, NULL);
	g_assert(!pool);

	for (i = 0; i < 100000; i++) {
		pool = __connman_ippool_create(23, 1, 20, NULL, NULL);
		g_assert(pool);

		__connman_ippool_unref(pool);
	}

	__connman_ippool_cleanup();
}

static void test_case_2(void)
{
	struct connman_ippool *pool;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	int i;

	__connman_ippool_init();

	/* Test the IP range */
	for (i = 1; i < 254; i++) {
		pool = __connman_ippool_create(23, 1, i, NULL, NULL);
		g_assert(pool);

		gateway = __connman_ippool_get_gateway(pool);
		broadcast = __connman_ippool_get_broadcast(pool);
		subnet_mask = __connman_ippool_get_subnet_mask(pool);
		start_ip = __connman_ippool_get_start_ip(pool);
		end_ip = __connman_ippool_get_end_ip(pool);

		g_assert(gateway);
		g_assert(broadcast);
		g_assert(subnet_mask);
		g_assert(start_ip);
		g_assert(end_ip);

		LOG("\n\tIP range %s --> %s\n"
			"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
			gateway, broadcast, subnet_mask);

		__connman_ippool_unref(pool);
	}

	__connman_ippool_cleanup();
}

static void test_case_3(void)
{
	struct connman_ippool *pool;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	GSList *list = NULL, *it;
	int i = 0;

	__connman_ippool_init();

	/*
	 *                                             Number of addresses
	 * 24-bit block         10.0.0.0    – 10.255.255.255    16,777,216
	 * 20-bit block         172.16.0.0  – 172.31.255.255     1,048,576
	 * 16-bit block         192.168.0.0 – 192.168.255.255       65,536
	 *
	 * Total                                                17,891,328
	 *
	 * Total numbers of 256 blocks:                             69,888
	 */

	/*
	 * Completely exhaust the first two pools because they are a bit
	 * too large.
	 */
	__connman_ippool_newaddr(45, "10.0.0.1", 8);
	__connman_ippool_newaddr(46, "172.16.0.1", 11);

	while (TRUE) {
		pool = __connman_ippool_create(23, 1, 100, NULL, NULL);
		if (!pool)
			break;
		i += 1;

		list = g_slist_prepend(list, pool);

		gateway = __connman_ippool_get_gateway(pool);
		broadcast = __connman_ippool_get_broadcast(pool);
		subnet_mask = __connman_ippool_get_subnet_mask(pool);
		start_ip = __connman_ippool_get_start_ip(pool);
		end_ip = __connman_ippool_get_end_ip(pool);

		g_assert(gateway);
		g_assert(broadcast);
		g_assert(subnet_mask);
		g_assert(start_ip);
		g_assert(end_ip);
	}

	g_assert(i == 255);

	LOG("Number of blocks %d", i);

	for (it = list; it; it = it->next) {
		pool = it->data;

		__connman_ippool_unref(pool);
	}

	g_slist_free(list);

	__connman_ippool_cleanup();
}

static void collision_cb(struct connman_ippool *pool, void *user_data)
{
	int *flag = user_data;

	LOG("collision detected");

	g_assert(*flag == 0);
	g_assert(pool);

	*flag = 1;
}

static void test_case_4(void)
{
	struct connman_ippool *pool;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	int flag;

	__connman_ippool_init();

	/* Test the IP range collision */

	flag = 0;
	pool = __connman_ippool_create(23, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	__connman_ippool_newaddr(23, start_ip, 24);

	g_assert(flag == 0);

	__connman_ippool_newaddr(42, start_ip, 16);

	g_assert(flag == 1);

	__connman_ippool_unref(pool);

	flag = 0;

	pool = __connman_ippool_create(23, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	__connman_ippool_newaddr(45, start_ip, 22);

	g_assert(flag == 1);

	__connman_ippool_unref(pool);

	__connman_ippool_cleanup();
}

static void test_case_5(void)
{
	struct connman_ippool *pool;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	int flag;

	__connman_ippool_init();

	/* Test the IP range collision */

	flag = 0;
	start_ip = "192.168.1.2";
	__connman_ippool_newaddr(25, start_ip, 24);
	g_assert(flag == 0);

	/* pool should return 192.168.0.1 now */
	pool = __connman_ippool_create(26, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	g_assert_cmpstr(gateway, ==, "192.168.0.1");
	g_assert_cmpstr(broadcast, ==, "192.168.0.255");
	g_assert_cmpstr(subnet_mask, ==, "255.255.255.0");
	g_assert_cmpstr(start_ip, ==, "192.168.0.1");
	g_assert_cmpstr(end_ip, ==, "192.168.0.101");

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	__connman_ippool_unref(pool);

	/*
	 * Now create the pool again, we should not get collision
	 * with existing allocated address.
	 */

	/* pool should return 192.168.2.1 now */
	flag = 0;
	pool = __connman_ippool_create(23, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	g_assert_cmpstr(gateway, ==, "192.168.2.1");
	g_assert_cmpstr(broadcast, ==, "192.168.2.255");
	g_assert_cmpstr(subnet_mask, ==, "255.255.255.0");
	g_assert_cmpstr(start_ip, ==, "192.168.2.1");
	g_assert_cmpstr(end_ip, ==, "192.168.2.101");

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	g_assert(flag == 0);

	__connman_ippool_unref(pool);

	__connman_ippool_cleanup();
}

static void test_case_6(void)
{
	struct connman_ippool *pool;
	const char *gateway;
	const char *broadcast;
	const char *subnet_mask;
	const char *start_ip;
	const char *end_ip;
	int flag;

	__connman_ippool_init();

	/* Test the IP range collision */

	flag = 0;
	start_ip = "192.168.1.2";
	__connman_ippool_newaddr(25, start_ip, 24);
	g_assert(flag == 0);

	flag = 0;
	start_ip = "192.168.0.2";
	__connman_ippool_newaddr(25, start_ip, 24);
	g_assert(flag == 0);

	/* pool should return 192.168.2.1 now */
	pool = __connman_ippool_create(26, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	g_assert_cmpstr(gateway, ==, "192.168.2.1");
	g_assert_cmpstr(broadcast, ==, "192.168.2.255");
	g_assert_cmpstr(subnet_mask, ==, "255.255.255.0");
	g_assert_cmpstr(start_ip, ==, "192.168.2.1");
	g_assert_cmpstr(end_ip, ==, "192.168.2.101");

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	__connman_ippool_unref(pool);

	/*
	 * Now create the pool again, we should not get collision
	 * with existing allocated address.
	 */

	/* pool should return 192.168.3.1 now */
	flag = 0;
	pool = __connman_ippool_create(23, 1, 100, collision_cb, &flag);
	g_assert(pool);

	gateway = __connman_ippool_get_gateway(pool);
	broadcast = __connman_ippool_get_broadcast(pool);
	subnet_mask = __connman_ippool_get_subnet_mask(pool);
	start_ip = __connman_ippool_get_start_ip(pool);
	end_ip = __connman_ippool_get_end_ip(pool);

	g_assert(gateway);
	g_assert(broadcast);
	g_assert(subnet_mask);
	g_assert(start_ip);
	g_assert(end_ip);

	g_assert_cmpstr(gateway, ==, "192.168.3.1");
	g_assert_cmpstr(broadcast, ==, "192.168.3.255");
	g_assert_cmpstr(subnet_mask, ==, "255.255.255.0");
	g_assert_cmpstr(start_ip, ==, "192.168.3.1");
	g_assert_cmpstr(end_ip, ==, "192.168.3.101");

	LOG("\n\tIP range %s --> %s\n"
		"\tgateway %s broadcast %s mask %s", start_ip, end_ip,
		gateway, broadcast, subnet_mask);

	g_assert(flag == 0);

	flag = 0;
	start_ip = "192.168.3.2";
	__connman_ippool_newaddr(25, start_ip, 24);
	g_assert(flag == 1);

	__connman_ippool_unref(pool);

	__connman_ippool_cleanup();
}

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/ippool/Test case 1", test_case_1);
	g_test_add_func("/ippool/Test case 2", test_case_2);
	g_test_add_func("/ippool/Test case 3", test_case_3);
	g_test_add_func("/ippool/Test case 4", test_case_4);
	g_test_add_func("/ippool/Test case 5", test_case_5);
	g_test_add_func("/ippool/Test case 6", test_case_6);

	return g_test_run();
}
