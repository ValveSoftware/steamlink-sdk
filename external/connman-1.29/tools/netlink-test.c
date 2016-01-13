/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013-2014  BMW Car IT GmbH.
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

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>
#include <linux/netfilter/nfnetlink.h>
#include <net/if.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

#include <glib.h>

#include "../src/shared/netlink.h"

#define NFGEN_DATA(nlh) ((void *)((char *)(nlh) +			\
				NLMSG_ALIGN(sizeof(struct nfgenmsg))))
#define NLA_DATA(nla)  ((void *)((char*)(nla) + NLA_HDRLEN))
#define NLA_OK(nla,len) ((len) >= (int)sizeof(struct nlattr) &&		\
				(nla)->nla_len >= sizeof(struct nlattr) && \
				(nla)->nla_len <= (len))
#define NLA_NEXT(nla,attrlen) ((attrlen) -= NLA_ALIGN((nla)->nla_len),	\
				(struct nlattr*)(((char*)(nla)) +       \
						NLA_ALIGN((nla)->nla_len)))

static GMainLoop *mainloop;

static void do_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	printf("%s%s\n", prefix, str);
}

static void getlink_callback(unsigned int error, uint16_t type, const void *data,
						uint32_t len, void *user_data)
{
	const struct ifinfomsg *ifi = data;
	struct rtattr *rta;
	int bytes;
	char ifname[IF_NAMESIZE];
	uint32_t index, flags;

	g_assert_cmpuint(error, ==, 0);

	bytes = len - NLMSG_ALIGN(sizeof(struct ifinfomsg));

	memset(ifname, 0, sizeof(ifname));

	index = ifi->ifi_index;
	flags = ifi->ifi_flags;

	for (rta = IFLA_RTA(ifi); RTA_OK(rta, bytes);
					rta = RTA_NEXT(rta, bytes)) {
		switch (rta->rta_type) {
		case IFLA_IFNAME:
			if (RTA_PAYLOAD(rta) <= IF_NAMESIZE)
				strcpy(ifname, RTA_DATA(rta));
			break;
		}
	}

	printf("index=%d flags=0x%08x name=%s\n", index, flags, ifname);

	g_main_loop_quit(mainloop);
}

static void test_case_1(void)
{
	struct netlink_info *netlink;
	struct ifinfomsg msg;

	netlink = netlink_new(NETLINK_ROUTE);

	printf("\n");
	netlink_set_debug(netlink, do_debug, "[NETLINK] ", NULL);

	memset(&msg, 0, sizeof(msg));

	netlink_send(netlink, RTM_GETLINK, NLM_F_DUMP, &msg, sizeof(msg),
						getlink_callback, NULL, NULL);

	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);
	g_main_loop_unref(mainloop);

	netlink_destroy(netlink);
}

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/netlink/Test case 1", test_case_1);

	return g_test_run();
}
