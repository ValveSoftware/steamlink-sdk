/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include <linux/if_vlan.h>
#include <linux/sockios.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000
#endif

#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/technology.h>
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/inet.h>
#include <connman/rtnl.h>
#include <connman/log.h>
#include <connman/setting.h>

static bool eth_tethering = false;

struct ethernet_data {
	int index;
	unsigned flags;
	unsigned int watch;
	struct connman_network *network;
};


static int get_vlan_vid(const char *ifname)
{
	struct vlan_ioctl_args vifr;
	int vid;
	int sk;

	memset(&vifr, 0, sizeof(vifr));

	sk = socket(AF_INET, SOCK_STREAM, 0);
	if (sk < 0)
		return -errno;

	vifr.cmd = GET_VLAN_VID_CMD;
	strncpy(vifr.device1, ifname, sizeof(vifr.device1));

	if(ioctl(sk, SIOCSIFVLAN, &vifr) >= 0)
		vid = vifr.u.VID;
	else
		vid = -errno;

	close(sk);

	return vid;
}

static int eth_network_probe(struct connman_network *network)
{
	DBG("network %p", network);

	return 0;
}

static void eth_network_remove(struct connman_network *network)
{
	DBG("network %p", network);
}

static int eth_network_connect(struct connman_network *network)
{
	DBG("network %p", network);

	connman_network_set_connected(network, true);

	return 0;
}

static int eth_network_disconnect(struct connman_network *network)
{
	DBG("network %p", network);

	connman_network_set_connected(network, false);

	return 0;
}

static struct connman_network_driver eth_network_driver = {
	.name		= "cable",
	.type		= CONNMAN_NETWORK_TYPE_ETHERNET,
	.probe		= eth_network_probe,
	.remove		= eth_network_remove,
	.connect	= eth_network_connect,
	.disconnect	= eth_network_disconnect,
};

static void add_network(struct connman_device *device,
			struct ethernet_data *ethernet)
{
	struct connman_network *network;
	int index, vid;
	char *ifname;

	network = connman_network_create("carrier",
					CONNMAN_NETWORK_TYPE_ETHERNET);
	if (!network)
		return;

	index = connman_device_get_index(device);
	connman_network_set_index(network, index);
	ifname = connman_inet_ifname(index);
	if (!ifname)
		return;
	vid = get_vlan_vid(ifname);

	connman_network_set_name(network, "Wired");

	if (connman_device_add_network(device, network) < 0) {
		connman_network_unref(network);
		return;
	}

	if (!eth_tethering) {
		char group[10] = "cable";
		/*
		 * Prevent service from starting the reconnect
		 * procedure as we do not want the DHCP client
		 * to run when tethering.
		 */
		if (vid >= 0)
			snprintf(group, sizeof(group), "%03x_cable", vid);

		connman_network_set_group(network, group);
	}

	ethernet->network = network;
	g_free(ifname);
}

static void remove_network(struct connman_device *device,
				struct ethernet_data *ethernet)
{
	if (!ethernet->network)
		return;

	connman_device_remove_network(device, ethernet->network);
	connman_network_unref(ethernet->network);

	ethernet->network = NULL;
}

static void ethernet_newlink(unsigned flags, unsigned change, void *user_data)
{
	struct connman_device *device = user_data;
	struct ethernet_data *ethernet = connman_device_get_data(device);

	DBG("index %d flags %d change %d", ethernet->index, flags, change);

	if ((ethernet->flags & IFF_UP) != (flags & IFF_UP)) {
		if (flags & IFF_UP) {
			DBG("power on");
			connman_device_set_powered(device, true);
		} else {
			DBG("power off");
			connman_device_set_powered(device, false);
		}
	}

	if ((ethernet->flags & IFF_LOWER_UP) != (flags & IFF_LOWER_UP)) {
		if (flags & IFF_LOWER_UP) {
			DBG("carrier on");
			add_network(device, ethernet);
		} else {
			DBG("carrier off");
			remove_network(device, ethernet);
		}
	}

	ethernet->flags = flags;
}

static int eth_dev_probe(struct connman_device *device)
{
	struct ethernet_data *ethernet;

	DBG("device %p", device);

	ethernet = g_try_new0(struct ethernet_data, 1);
	if (!ethernet)
		return -ENOMEM;

	connman_device_set_data(device, ethernet);

	ethernet->index = connman_device_get_index(device);
	ethernet->flags = 0;

	ethernet->watch = connman_rtnl_add_newlink_watch(ethernet->index,
						ethernet_newlink, device);

	return 0;
}

static void eth_dev_remove(struct connman_device *device)
{
	struct ethernet_data *ethernet = connman_device_get_data(device);

	DBG("device %p", device);

	connman_device_set_data(device, NULL);

	connman_rtnl_remove_watch(ethernet->watch);

	remove_network(device, ethernet);

	g_free(ethernet);
}

static int eth_dev_enable(struct connman_device *device)
{
	struct ethernet_data *ethernet = connman_device_get_data(device);

	DBG("device %p", device);

	return connman_inet_ifup(ethernet->index);
}

static int eth_dev_disable(struct connman_device *device)
{
	struct ethernet_data *ethernet = connman_device_get_data(device);

	DBG("device %p", device);

	return connman_inet_ifdown(ethernet->index);
}

static struct connman_device_driver eth_dev_driver = {
	.name		= "ethernet",
	.type		= CONNMAN_DEVICE_TYPE_ETHERNET,
	.probe		= eth_dev_probe,
	.remove		= eth_dev_remove,
	.enable		= eth_dev_enable,
	.disable	= eth_dev_disable,
};

static int eth_tech_probe(struct connman_technology *technology)
{
	return 0;
}

static void eth_tech_remove(struct connman_technology *technology)
{
	DBG("");
}

static GList *eth_interface_list = NULL;

static void eth_tech_add_interface(struct connman_technology *technology,
			int index, const char *name, const char *ident)
{
	DBG("index %d name %s ident %s", index, name, ident);

	if (g_list_find(eth_interface_list, GINT_TO_POINTER((int)index)))
		return;

	eth_interface_list = g_list_prepend(eth_interface_list,
					(GINT_TO_POINTER((int) index)));
}

static void eth_tech_remove_interface(struct connman_technology *technology,
								int index)
{
	DBG("index %d", index);

	eth_interface_list = g_list_remove(eth_interface_list,
					GINT_TO_POINTER((int) index));
}

static void eth_tech_enable_tethering(struct connman_technology *technology,
						const char *bridge)
{
	GList *list;
	struct ethernet_data *ethernet;

	for (list = eth_interface_list; list; list = list->next) {
		int index = GPOINTER_TO_INT(list->data);
		struct connman_device *device =
			connman_device_find_by_index(index);

		if (device) {
			ethernet = connman_device_get_data(device);
			if (ethernet)
				remove_network(device, ethernet);
		}

		connman_technology_tethering_notify(technology, true);

		connman_inet_ifup(index);

		connman_inet_add_to_bridge(index, bridge);

		eth_tethering = true;
	}
}

static void eth_tech_disable_tethering(struct connman_technology *technology,
						const char *bridge)
{
	GList *list;

	for (list = eth_interface_list; list; list = list->next) {
		int index = GPOINTER_TO_INT(list->data);
		struct connman_device *device =
			connman_device_find_by_index(index);

		connman_inet_remove_from_bridge(index, bridge);

		connman_technology_tethering_notify(technology, false);

		if (device)
			connman_device_reconnect_service(device);

		eth_tethering = false;
	}
}

static int eth_tech_set_tethering(struct connman_technology *technology,
				const char *identifier, const char *passphrase,
				const char *bridge, bool enabled)
{
	if (!connman_technology_is_tethering_allowed(
			CONNMAN_SERVICE_TYPE_ETHERNET))
		return 0;

	DBG("bridge %s enabled %d", bridge, enabled);

	if (enabled)
		eth_tech_enable_tethering(technology, bridge);
	else
		eth_tech_disable_tethering(technology, bridge);

	return 0;
}

static struct connman_technology_driver eth_tech_driver = {
	.name			= "ethernet",
	.type			= CONNMAN_SERVICE_TYPE_ETHERNET,
	.probe			= eth_tech_probe,
	.remove			= eth_tech_remove,
	.add_interface		= eth_tech_add_interface,
	.remove_interface	= eth_tech_remove_interface,
	.set_tethering		= eth_tech_set_tethering,
};

static int ethernet_init(void)
{
	int err;

	err = connman_technology_driver_register(&eth_tech_driver);
	if (err < 0)
		return err;

	err = connman_network_driver_register(&eth_network_driver);
	if (err < 0)
		return err;

	err = connman_device_driver_register(&eth_dev_driver);
	if (err < 0) {
		connman_network_driver_unregister(&eth_network_driver);
		return err;
	}

	return 0;
}

static void ethernet_exit(void)
{
	connman_technology_driver_unregister(&eth_tech_driver);

	connman_network_driver_unregister(&eth_network_driver);

	connman_device_driver_unregister(&eth_dev_driver);
}

CONNMAN_PLUGIN_DEFINE(ethernet, "Ethernet interface plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, ethernet_init, ethernet_exit)
