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

#include <errno.h>
#include <net/if.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP	0x10000
#endif

#include <glib.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/technology.h>
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/rtnl.h>
#include <connman/inet.h>
#include <connman/log.h>

static bool gadget_tethering = false;

struct gadget_data {
	int index;
	unsigned flags;
	unsigned int watch;
	struct connman_network *network;
};

static int gadget_network_probe(struct connman_network *network)
{
	DBG("network %p", network);

	return 0;
}

static void gadget_network_remove(struct connman_network *network)
{
	DBG("network %p", network);
}

static int gadget_network_connect(struct connman_network *network)
{
	DBG("network %p", network);

	connman_network_set_connected(network, true);

	return 0;
}

static int gadget_network_disconnect(struct connman_network *network)
{
	DBG("network %p", network);

	connman_network_set_connected(network, false);

	return 0;
}

static struct connman_network_driver gadget_network_driver = {
	.name		= "usb",
	.type		= CONNMAN_NETWORK_TYPE_GADGET,
	.probe		= gadget_network_probe,
	.remove		= gadget_network_remove,
	.connect	= gadget_network_connect,
	.disconnect	= gadget_network_disconnect,
};

static void add_network(struct connman_device *device,
			struct gadget_data *gadget)
{
	struct connman_network *network;
	int index;

	network = connman_network_create("gadget",
					CONNMAN_NETWORK_TYPE_GADGET);
	if (!network)
		return;

	index = connman_device_get_index(device);
	connman_network_set_index(network, index);

	connman_network_set_name(network, "Wired");

	if (connman_device_add_network(device, network) < 0) {
		connman_network_unref(network);
		return;
	}

	if (!gadget_tethering)
		/*
		 * Prevent service from starting the reconnect
		 * procedure as we do not want the DHCP client
		 * to run when tethering.
		 */
		connman_network_set_group(network, "usb");

	gadget->network = network;
}

static void remove_network(struct connman_device *device,
				struct gadget_data *gadget)
{
	if (!gadget->network)
		return;

	connman_device_remove_network(device, gadget->network);
	connman_network_unref(gadget->network);

	gadget->network = NULL;
}

static void gadget_newlink(unsigned flags, unsigned change, void *user_data)
{
	struct connman_device *device = user_data;
	struct gadget_data *gadget = connman_device_get_data(device);

	DBG("index %d flags %d change %d", gadget->index, flags, change);

	if ((gadget->flags & IFF_UP) != (flags & IFF_UP)) {
		if (flags & IFF_UP) {
			DBG("power on");
			connman_device_set_powered(device, true);
		} else {
			DBG("power off");
			connman_device_set_powered(device, false);
		}
	}

	if ((gadget->flags & IFF_LOWER_UP) != (flags & IFF_LOWER_UP)) {
		if (flags & IFF_LOWER_UP) {
			DBG("carrier on");
			add_network(device, gadget);
		} else {
			DBG("carrier off");
			remove_network(device, gadget);
		}
	}

	gadget->flags = flags;
}

static int gadget_dev_probe(struct connman_device *device)
{
	struct gadget_data *gadget;

	DBG("device %p", device);

	gadget = g_try_new0(struct gadget_data, 1);
	if (!gadget)
		return -ENOMEM;

	connman_device_set_data(device, gadget);

	gadget->index = connman_device_get_index(device);
	gadget->flags = 0;

	gadget->watch = connman_rtnl_add_newlink_watch(gadget->index,
						gadget_newlink, device);

	return 0;
}

static void gadget_dev_remove(struct connman_device *device)
{
	struct gadget_data *gadget = connman_device_get_data(device);

	DBG("device %p", device);

	connman_device_set_data(device, NULL);

	connman_rtnl_remove_watch(gadget->watch);

	remove_network(device, gadget);

	g_free(gadget);
}

static int gadget_dev_enable(struct connman_device *device)
{
	struct gadget_data *gadget = connman_device_get_data(device);

	DBG("device %p", device);

	return connman_inet_ifup(gadget->index);
}

static int gadget_dev_disable(struct connman_device *device)
{
	struct gadget_data *gadget = connman_device_get_data(device);

	DBG("device %p", device);

	return connman_inet_ifdown(gadget->index);
}

static struct connman_device_driver gadget_dev_driver = {
	.name		= "gadget",
	.type		= CONNMAN_DEVICE_TYPE_GADGET,
	.probe		= gadget_dev_probe,
	.remove		= gadget_dev_remove,
	.enable		= gadget_dev_enable,
	.disable	= gadget_dev_disable,
};

static GList *cdc_interface_list = NULL;

static void gadget_tech_add_interface(struct connman_technology *technology,
			int index, const char *name, const char *ident)
{
	DBG("index %d name %s ident %s", index, name, ident);

	if (g_list_find(cdc_interface_list, GINT_TO_POINTER((int)index)))
		return;

	cdc_interface_list = g_list_prepend(cdc_interface_list,
					(GINT_TO_POINTER((int) index)));
}

static void gadget_tech_remove_interface(struct connman_technology *technology,
								int index)
{
	DBG("index %d", index);

	cdc_interface_list = g_list_remove(cdc_interface_list,
					GINT_TO_POINTER((int) index));
}

static void gadget_tech_enable_tethering(struct connman_technology *technology,
						const char *bridge)
{
	GList *list;

	for (list = cdc_interface_list; list; list = list->next) {
		int index = GPOINTER_TO_INT(list->data);
		struct connman_device *device =
			connman_device_find_by_index(index);
		struct gadget_data *gadget;

		if (device) {
			gadget = connman_device_get_data(device);
			if (gadget)
				remove_network(device, gadget);
		}

		connman_technology_tethering_notify(technology, true);

		connman_inet_ifup(index);

		connman_inet_add_to_bridge(index, bridge);
	}
}

static void gadget_tech_disable_tethering(struct connman_technology *technology,
						const char *bridge)
{
	GList *list;

	for (list = cdc_interface_list; list; list = list->next) {
		int index = GPOINTER_TO_INT(list->data);

		connman_inet_remove_from_bridge(index, bridge);

		connman_inet_ifdown(index);

		connman_technology_tethering_notify(technology, false);
	}
}

static int gadget_tech_set_tethering(struct connman_technology *technology,
				const char *identifier, const char *passphrase,
				const char *bridge, bool enabled)
{
	DBG("bridge %s enabled %d", bridge, enabled);

	if (enabled)
		gadget_tech_enable_tethering(technology, bridge);
	else
		gadget_tech_disable_tethering(technology, bridge);

	return 0;
}

static int gadget_tech_probe(struct connman_technology *technology)
{
	return 0;
}

static void gadget_tech_remove(struct connman_technology *technology)
{
	g_list_free(cdc_interface_list);

	cdc_interface_list = NULL;
}

static struct connman_technology_driver gadget_tech_driver = {
	.name			= "cdc_ethernet",
	.type			= CONNMAN_SERVICE_TYPE_GADGET,
	.probe			= gadget_tech_probe,
	.remove			= gadget_tech_remove,
	.add_interface		= gadget_tech_add_interface,
	.remove_interface	= gadget_tech_remove_interface,
	.set_tethering		= gadget_tech_set_tethering,
};

static int gadget_init(void)
{
	int err;

	err = connman_technology_driver_register(&gadget_tech_driver);
	if (err < 0) {
		return err;
	}

	err = connman_network_driver_register(&gadget_network_driver);
	if (err < 0)
		return err;

	err = connman_device_driver_register(&gadget_dev_driver);
	if (err < 0) {
		connman_technology_driver_unregister(&gadget_tech_driver);
		return err;
	}

	return 0;
}

static void gadget_exit(void)
{
	connman_technology_driver_unregister(&gadget_tech_driver);
	connman_network_driver_unregister(&gadget_network_driver);
	connman_device_driver_unregister(&gadget_dev_driver);
}

CONNMAN_PLUGIN_DEFINE(gadget, "Gadget interface plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, gadget_init, gadget_exit)
