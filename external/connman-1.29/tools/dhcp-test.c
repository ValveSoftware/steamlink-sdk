/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/ethernet.h>
#include <linux/if_arp.h>

#include <gdhcp/gdhcp.h>

static GTimer *timer;

static GMainLoop *main_loop;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static void print_elapsed(void)
{
	gdouble elapsed;

	elapsed = g_timer_elapsed(timer, NULL);

	printf("elapsed: %f seconds\n", elapsed);
}

static void handle_error(GDHCPClientError error)
{
	switch (error) {
	case G_DHCP_CLIENT_ERROR_NONE:
		printf("dhcp client ok\n");
		break;
	case G_DHCP_CLIENT_ERROR_INTERFACE_UNAVAILABLE:
		printf("Interface unavailable\n");
		break;
	case G_DHCP_CLIENT_ERROR_INTERFACE_IN_USE:
		printf("Interface in use\n");
		break;
	case G_DHCP_CLIENT_ERROR_INTERFACE_DOWN:
		printf("Interface down\n");
		break;
	case G_DHCP_CLIENT_ERROR_NOMEM:
		printf("No memory\n");
		break;
	case G_DHCP_CLIENT_ERROR_INVALID_INDEX:
		printf("Invalid index\n");
		break;
	case G_DHCP_CLIENT_ERROR_INVALID_OPTION:
		printf("Invalid option\n");
		break;
	}
}

static void no_lease_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	print_elapsed();

	printf("No lease available\n");

	g_main_loop_quit(main_loop);
}

static void lease_available_cb(GDHCPClient *dhcp_client, gpointer user_data)
{
	GList *list, *option_value = NULL;
	char *address;

	print_elapsed();

	printf("Lease available\n");

	address = g_dhcp_client_get_address(dhcp_client);
	printf("address %s\n", address);
	if (!address)
		return;

	option_value = g_dhcp_client_get_option(dhcp_client, G_DHCP_SUBNET);
	for (list = option_value; list; list = list->next)
		printf("sub-mask %s\n", (char *) list->data);

	option_value = g_dhcp_client_get_option(dhcp_client, G_DHCP_DNS_SERVER);
	for (list = option_value; list; list = list->next)
		printf("domain-name-servers %s\n", (char *) list->data);

	option_value = g_dhcp_client_get_option(dhcp_client, G_DHCP_DOMAIN_NAME);
	for (list = option_value; list; list = list->next)
		printf("domain-name %s\n", (char *) list->data);

	option_value = g_dhcp_client_get_option(dhcp_client, G_DHCP_ROUTER);
	for (list = option_value; list; list = list->next)
		printf("routers %s\n", (char *) list->data);

	option_value = g_dhcp_client_get_option(dhcp_client, G_DHCP_HOST_NAME);
	for (list = option_value; list; list = list->next)
		printf("hostname %s\n", (char *) list->data);
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	GDHCPClientError error;
	GDHCPClient *dhcp_client;
	int index;

	if (argc < 2) {
		printf("Usage: dhcp-test <interface index>\n");
		exit(0);
	}

	index = atoi(argv[1]);

	printf("Create DHCP client for interface %d\n", index);

	dhcp_client = g_dhcp_client_new(G_DHCP_IPV4, index, &error);
	if (!dhcp_client) {
		handle_error(error);
		exit(0);
	}

	g_dhcp_client_set_send(dhcp_client, G_DHCP_HOST_NAME, "<hostname>");

	g_dhcp_client_set_request(dhcp_client, G_DHCP_HOST_NAME);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_SUBNET);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_DNS_SERVER);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_DOMAIN_NAME);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_NTP_SERVER);
	g_dhcp_client_set_request(dhcp_client, G_DHCP_ROUTER);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_LEASE_AVAILABLE,
						lease_available_cb, NULL);

	g_dhcp_client_register_event(dhcp_client,
			G_DHCP_CLIENT_EVENT_NO_LEASE, no_lease_cb, NULL);

	main_loop = g_main_loop_new(NULL, FALSE);

	printf("Start DHCP operation\n");

	timer = g_timer_new();

	g_dhcp_client_start(dhcp_client, NULL);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_main_loop_run(main_loop);

	g_timer_destroy(timer);

	g_dhcp_client_unref(dhcp_client);

	g_main_loop_unref(main_loop);

	return 0;
}
