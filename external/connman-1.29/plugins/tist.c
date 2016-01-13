/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/log.h>

#define TIST_SYSFS_INSTALL "/sys/devices/platform/kim/install"
#define TIST_SYSFS_UART "/sys/devices/platform/kim/dev_name"
#define TIST_SYSFS_BAUD "/sys/devices/platform/kim/baud_rate"

/* Shared transport line discipline */
#define N_TI_WL 22

static GIOChannel *install_channel = NULL;
static GIOChannel *uart_channel = NULL;
static char uart_dev_name[32];
static unsigned long baud_rate = 0;

static guint install_watch = 0;
static guint uart_watch = 0;

static int install_count = 0;

#define NCCS2 19
struct termios2 {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_line;			/* line discipline */
	cc_t c_cc[NCCS2];		/* control characters */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
};

#define  BOTHER         0x00001000

/* HCI definitions */
#define HCI_HDR_OPCODE          0xff36
#define HCI_COMMAND_PKT         0x01
#define HCI_EVENT_PKT           0x04
#define EVT_CMD_COMPLETE        0x0E

/* HCI Command structure to set the target baud rate */
struct speed_change_cmd {
	uint8_t uart_prefix;
	uint16_t opcode;
	uint8_t plen;
	uint32_t speed;
} __attribute__ ((packed));

/* HCI Event structure to set the cusrom baud rate*/
struct cmd_complete {
	uint8_t uart_prefix;
	uint8_t evt;
	uint8_t plen;
	uint8_t ncmd;
	uint16_t opcode;
	uint8_t status;
	uint8_t data[16];
} __attribute__ ((packed));

static int read_baud_rate(unsigned long *baud)
{
	int err;
	FILE *f;

	DBG("");

	f = fopen(TIST_SYSFS_BAUD, "r");
	if (!f)
		return -EIO;

	err = fscanf(f, "%lu", baud);
	fclose(f);

	DBG("baud rate %lu", *baud);

	return err;
}

static int read_uart_name(char uart_name[], size_t uart_name_len)
{
	int err;
	FILE *f;

	DBG("");

	memset(uart_name, 0, uart_name_len);

	f = fopen(TIST_SYSFS_UART, "r");
	if (!f)
		return -EIO;

        err = fscanf(f, "%s", uart_name);
	fclose(f);

	DBG("UART name %s", uart_name);

	return err;
}

static int read_hci_event(int fd, unsigned char *buf, int size)
{
	int prefix_len, param_len;

	if (size <= 0)
		return -EINVAL;

	/* First 3 bytes are prefix, event and param length */
	prefix_len = read(fd, buf, 3);
	if (prefix_len < 0)
		return prefix_len;

	if (prefix_len < 3) {
		connman_error("Truncated HCI prefix %d bytes 0x%x",
						prefix_len, buf[0]);
		return -EIO;
	}

	DBG("type 0x%x event 0x%x param len %d", buf[0], buf[1], buf[2]);

	param_len = buf[2];
	if (param_len > size - 3) {
		connman_error("Buffer is too small %d", size);
		return -EINVAL;
	}

	return read(fd, buf + 3, param_len);
}

static int read_command_complete(int fd, unsigned short opcode)
{
	struct cmd_complete resp;
	int err;

	DBG("");

	err = read_hci_event(fd, (unsigned char *)&resp, sizeof(resp));
	if (err < 0)
		return err;

	DBG("HCI event %d bytes", err);

	if (resp.uart_prefix != HCI_EVENT_PKT) {
		connman_error("Not an event packet");
		return -EIO;
	}

	if (resp.evt != EVT_CMD_COMPLETE) {
	        connman_error("Not a cmd complete event");
		return -EIO;
	}

	if (resp.plen < 4) {
		connman_error("HCI header length %d", resp.plen);
		return -EIO;
	}

	if (resp.opcode != (unsigned short) opcode) {
		connman_error("opcode 0x%04x 0x%04x", resp.opcode, opcode);
		return -EIO;
	}

	return 0;
}

/* The default baud rate is 115200 */
static int set_default_baud_rate(int fd)
{
	struct termios ti;
	int err;

	DBG("");

	err = tcflush(fd, TCIOFLUSH);
	if (err < 0)
		goto err;

	err = tcgetattr(fd, &ti);
	if (err < 0)
		goto err;

	cfmakeraw(&ti);

	ti.c_cflag |= 1;
	ti.c_cflag |= CRTSCTS;

	err = tcsetattr(fd, TCSANOW, &ti);
	if (err < 0)
		goto err;

	cfsetospeed(&ti, B115200);
	cfsetispeed(&ti, B115200);

	err = tcsetattr(fd, TCSANOW, &ti);
	if (err < 0)
		goto err;

	err = tcflush(fd, TCIOFLUSH);
	if (err < 0)
		goto err;

	return 0;

err:
	connman_error("%s", strerror(errno));

	return err;
}

static int set_custom_baud_rate(int fd, unsigned long cus_baud_rate, int flow_ctrl)
{
	struct termios ti;
	struct termios2 ti2;
	int err;

	DBG("baud rate %lu flow_ctrl %d", cus_baud_rate, flow_ctrl);

	err = tcflush(fd, TCIOFLUSH);
	if (err < 0)
		goto err;

	err = tcgetattr(fd, &ti);
	if (err < 0)
		goto err;

	if (flow_ctrl)
		ti.c_cflag |= CRTSCTS;
	else
		ti.c_cflag &= ~CRTSCTS;

	/*
	 * Set the parameters associated with the UART
	 * The change will occur immediately by using TCSANOW.
	 */
	err = tcsetattr(fd, TCSANOW, &ti);
	if (err < 0)
		goto err;

	err = tcflush(fd, TCIOFLUSH);
	if (err < 0)
		goto err;

	/* Set the actual baud rate */
	err = ioctl(fd, TCGETS2, &ti2);
	if (err < 0)
		goto err;

	ti2.c_cflag &= ~CBAUD;
	ti2.c_cflag |= BOTHER;
	ti2.c_ospeed = cus_baud_rate;

	err = ioctl(fd, TCSETS2, &ti2);
	if (err < 0)
		goto err;

	return 0;

err:
	DBG("%s", strerror(errno));

	return err;
}

static gboolean uart_event(GIOChannel *channel,
				GIOCondition cond, gpointer data)
{
	int uart_fd, ldisc;

	DBG("");

	uart_fd = g_io_channel_unix_get_fd(channel);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		connman_error("UART event 0x%x", cond);
		if (uart_watch > 0)
			g_source_remove(uart_watch);

		goto err;
	}

	if (read_command_complete(uart_fd, HCI_HDR_OPCODE) < 0)
		goto err;

	if (set_custom_baud_rate(uart_fd, baud_rate, 1) < 0)
		goto err;

	ldisc = N_TI_WL;
	if (ioctl(uart_fd, TIOCSETD, &ldisc) < 0)
		goto err;

	install_count = 1;
	__sync_synchronize();

	return FALSE;

err:
	install_count = 0;
	__sync_synchronize();

	g_io_channel_shutdown(channel, TRUE, NULL);
	g_io_channel_unref(channel);

	return FALSE;
}

static int install_ldisc(GIOChannel *channel, bool install)
{
	int uart_fd, err;
	struct speed_change_cmd cmd;
	GIOFlags flags;

	DBG("%d %p", install, uart_channel);

	if (!install) {
		install_count = 0;
		__sync_synchronize();

		if (!uart_channel) {
			DBG("UART channel is NULL");
			return 0;
		}

		g_io_channel_shutdown(uart_channel, TRUE, NULL);
		g_io_channel_unref(uart_channel);

		uart_channel = NULL;

		return 0;
	}

	if (uart_channel) {
		g_io_channel_shutdown(uart_channel, TRUE, NULL);
		g_io_channel_unref(uart_channel);
		uart_channel = NULL;
	}

	DBG("opening %s custom baud %lu", uart_dev_name, baud_rate);

	uart_fd = open(uart_dev_name, O_RDWR | O_CLOEXEC);
	if (uart_fd < 0)
		return -EIO;

	uart_channel = g_io_channel_unix_new(uart_fd);
	g_io_channel_set_close_on_unref(uart_channel, TRUE);

	g_io_channel_set_encoding(uart_channel, NULL, NULL);
	g_io_channel_set_buffered(uart_channel, FALSE);

	flags = g_io_channel_get_flags(uart_channel);
	flags |= G_IO_FLAG_NONBLOCK;
	g_io_channel_set_flags(uart_channel, flags, NULL);

        err = set_default_baud_rate(uart_fd);
	if (err < 0) {
		g_io_channel_shutdown(uart_channel, TRUE, NULL);
		g_io_channel_unref(uart_channel);
		uart_channel = NULL;

		return err;
	}

	if (baud_rate == 115200) {
		int ldisc;

		ldisc = N_TI_WL;
		if (ioctl(uart_fd, TIOCSETD, &ldisc) < 0) {
			g_io_channel_shutdown(uart_channel, TRUE, NULL);
			g_io_channel_unref(uart_channel);
			uart_channel = NULL;
		}

		install_count = 0;
		__sync_synchronize();

		return 0;
	}

	cmd.uart_prefix = HCI_COMMAND_PKT;
	cmd.opcode = HCI_HDR_OPCODE;
	cmd.plen = sizeof(unsigned long);
	cmd.speed = baud_rate;

	uart_watch = g_io_add_watch(uart_channel,
			G_IO_IN | G_IO_NVAL | G_IO_HUP | G_IO_ERR,
				uart_event, NULL);

	err = write(uart_fd, &cmd, sizeof(cmd));
	if (err < 0) {
		connman_error("Write failed %d", err);

		g_io_channel_shutdown(uart_channel, TRUE, NULL);
		g_io_channel_unref(uart_channel);
		uart_channel = NULL;
	}

	return err;
}


static gboolean install_event(GIOChannel *channel,
				GIOCondition cond, gpointer data)
{
	GIOStatus status = G_IO_STATUS_NORMAL;
	unsigned int install_state;
	bool install;
	char buf[8];
	gsize len;

	DBG("");

	if (cond & (G_IO_HUP | G_IO_NVAL)) {
		connman_error("install event 0x%x", cond);
		return FALSE;
	}

	__sync_synchronize();
	if (install_count != 0) {
		status = g_io_channel_seek_position(channel, 0, G_SEEK_SET, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			g_io_channel_shutdown(channel, TRUE, NULL);
			g_io_channel_unref(channel);
			return FALSE;
		}

		/* Read the install value */
		status = g_io_channel_read_chars(channel, (gchar *) buf,
						8, &len, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			g_io_channel_shutdown(channel, TRUE, NULL);
			g_io_channel_unref(channel);
			return FALSE;
		}

		install_state = atoi(buf);
		DBG("install event while installing %d %c", install_state, buf[0]);

		return TRUE;
	} else {
		install_count = 1;
		__sync_synchronize();
	}

	status = g_io_channel_seek_position(channel, 0, G_SEEK_SET, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return FALSE;
	}

	/* Read the install value */
	status = g_io_channel_read_chars(channel, (gchar *) buf, 8, &len, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return FALSE;
	}

	install_state = atoi(buf);

	DBG("install state %d", install_state);

	install = !!install_state;

	if (install_ldisc(channel, install) < 0) {
		connman_error("ldisc installation failed");
		install_count = 0;
		__sync_synchronize();
		return TRUE;
	}

	return TRUE;
}


static int tist_init(void)
{
	GIOStatus status = G_IO_STATUS_NORMAL;
	GIOFlags flags;
	unsigned int install_state;
	char buf[8];
	int fd, err;
	gsize len;

	err = read_uart_name(uart_dev_name, sizeof(uart_dev_name));
	if (err < 0) {
		connman_error("Could not read the UART name");
		return err;
	}

	err = read_baud_rate(&baud_rate);
	if (err < 0) {
		connman_error("Could not read the baud rate");
		return err;
	}

	fd = open(TIST_SYSFS_INSTALL, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		connman_error("Failed to open TI ST sysfs install file");
		return -EIO;
	}

	install_channel = g_io_channel_unix_new(fd);
	g_io_channel_set_close_on_unref(install_channel, TRUE);

	g_io_channel_set_encoding(install_channel, NULL, NULL);
	g_io_channel_set_buffered(install_channel, FALSE);

	flags = g_io_channel_get_flags(install_channel);
	flags |= G_IO_FLAG_NONBLOCK;
	g_io_channel_set_flags(install_channel, flags, NULL);

	status = g_io_channel_read_chars(install_channel, (gchar *) buf, 8,
								&len, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		g_io_channel_shutdown(install_channel, TRUE, NULL);
		g_io_channel_unref(install_channel);
		return status;
	}

	status = g_io_channel_seek_position(install_channel, 0, G_SEEK_SET, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		connman_error("Initial seek failed");
		g_io_channel_shutdown(install_channel, TRUE, NULL);
		g_io_channel_unref(install_channel);
		return -EIO;
	}

	install_state = atoi(buf);

	DBG("Initial state %d", install_state);

	install_watch = g_io_add_watch_full(install_channel, G_PRIORITY_HIGH,
				G_IO_PRI | G_IO_ERR,
					    install_event, NULL, NULL);

	if (install_state) {
		install_count = 1;
		__sync_synchronize();

		err = install_ldisc(install_channel, true);
		if (err < 0) {
			connman_error("ldisc installtion failed");
			return err;
		}
	}

	return 0;
}


static void tist_exit(void)
{

	if (install_watch > 0)
		g_source_remove(install_watch);

	DBG("uart_channel %p", uart_channel);

	g_io_channel_shutdown(install_channel, TRUE, NULL);
	g_io_channel_unref(install_channel);

	if (uart_channel) {
		g_io_channel_shutdown(uart_channel, TRUE, NULL);
		g_io_channel_unref(uart_channel);
		uart_channel = NULL;
	}

}

CONNMAN_PLUGIN_DEFINE(tist, "TI shared transport support", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, tist_init, tist_exit)
