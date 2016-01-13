/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <glib.h>

#include "connman.h"

struct ntp_short {
	uint16_t seconds;
	uint16_t fraction;
} __attribute__ ((packed));

struct ntp_time {
	uint32_t seconds;
	uint32_t fraction;
} __attribute__ ((packed));

struct ntp_msg {
	uint8_t flags;			/* Mode, version and leap indicator */
	uint8_t stratum;		/* Stratum details */
	int8_t poll;			/* Maximum interval in log2 seconds */
	int8_t precision;		/* Clock precision in log2 seconds */
	struct ntp_short rootdelay;	/* Root delay */
	struct ntp_short rootdisp;	/* Root dispersion */
	uint32_t refid;			/* Reference ID */
	struct ntp_time reftime;	/* Reference timestamp */
	struct ntp_time orgtime;	/* Origin timestamp */
	struct ntp_time rectime;	/* Receive timestamp */
	struct ntp_time xmttime;	/* Transmit timestamp */
} __attribute__ ((packed));

#define OFFSET_1900_1970  2208988800UL	/* 1970 - 1900 in seconds */

#define STEPTIME_MIN_OFFSET  0.128

#define LOGTOD(a)  ((a) < 0 ? 1. / (1L << -(a)) : 1L << (int)(a))

#define NTP_SEND_TIMEOUT       2
#define NTP_SEND_RETRIES       3

#define NTP_FLAG_LI_SHIFT      6
#define NTP_FLAG_LI_MASK       0x3
#define NTP_FLAG_LI_NOWARNING  0x0
#define NTP_FLAG_LI_ADDSECOND  0x1
#define NTP_FLAG_LI_DELSECOND  0x2
#define NTP_FLAG_LI_NOTINSYNC  0x3

#define NTP_FLAG_VN_SHIFT      3
#define NTP_FLAG_VN_MASK       0x7

#define NTP_FLAG_MD_SHIFT      0
#define NTP_FLAG_MD_MASK       0x7
#define NTP_FLAG_MD_UNSPEC     0
#define NTP_FLAG_MD_ACTIVE     1
#define NTP_FLAG_MD_PASSIVE    2
#define NTP_FLAG_MD_CLIENT     3
#define NTP_FLAG_MD_SERVER     4
#define NTP_FLAG_MD_BROADCAST  5
#define NTP_FLAG_MD_CONTROL    6
#define NTP_FLAG_MD_PRIVATE    7

#define NTP_FLAG_VN_VER3       3
#define NTP_FLAG_VN_VER4       4

#define NTP_FLAGS_ENCODE(li, vn, md)  ((uint8_t)( \
                      (((li) & NTP_FLAG_LI_MASK) << NTP_FLAG_LI_SHIFT) | \
                      (((vn) & NTP_FLAG_VN_MASK) << NTP_FLAG_VN_SHIFT) | \
                      (((md) & NTP_FLAG_MD_MASK) << NTP_FLAG_MD_SHIFT)))

#define NTP_FLAGS_LI_DECODE(flags)    ((uint8_t)(((flags) >> NTP_FLAG_LI_SHIFT) & NTP_FLAG_LI_MASK))
#define NTP_FLAGS_VN_DECODE(flags)    ((uint8_t)(((flags) >> NTP_FLAG_VN_SHIFT) & NTP_FLAG_VN_MASK))
#define NTP_FLAGS_MD_DECODE(flags)    ((uint8_t)(((flags) >> NTP_FLAG_MD_SHIFT) & NTP_FLAG_MD_MASK))

#define NTP_PRECISION_S    0
#define NTP_PRECISION_DS   -3
#define NTP_PRECISION_CS   -6
#define NTP_PRECISION_MS   -9
#define NTP_PRECISION_US   -19
#define NTP_PRECISION_NS   -29

static guint channel_watch = 0;
static struct timespec mtx_time;
static int transmit_fd = 0;

static char *timeserver = NULL;
static struct sockaddr_in timeserver_addr;
static gint poll_id = 0;
static gint timeout_id = 0;
static guint retries = 0;

static void send_packet(int fd, const char *server, uint32_t timeout);

static void next_server(void)
{
	if (timeserver) {
		g_free(timeserver);
		timeserver = NULL;
	}

	__connman_timeserver_sync_next();
}

static gboolean send_timeout(gpointer user_data)
{
	uint32_t timeout = GPOINTER_TO_UINT(user_data);

	DBG("send timeout %u (retries %d)", timeout, retries);

	if (retries++ == NTP_SEND_RETRIES)
		next_server();
	else
		send_packet(transmit_fd, timeserver, timeout << 1);

	return FALSE;
}

static void send_packet(int fd, const char *server, uint32_t timeout)
{
	struct ntp_msg msg;
	struct sockaddr_in addr;
	struct timeval transmit_timeval;
	ssize_t len;

	/*
	 * At some point, we could specify the actual system precision with:
	 *
	 *   clock_getres(CLOCK_REALTIME, &ts);
	 *   msg.precision = (int)log2(ts.tv_sec + (ts.tv_nsec * 1.0e-9));
	 */
	memset(&msg, 0, sizeof(msg));
	msg.flags = NTP_FLAGS_ENCODE(NTP_FLAG_LI_NOTINSYNC, NTP_FLAG_VN_VER4,
	    NTP_FLAG_MD_CLIENT);
	msg.poll = 4;	// min
	msg.poll = 10;	// max
	msg.precision = NTP_PRECISION_S;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(123);
	addr.sin_addr.s_addr = inet_addr(server);

	gettimeofday(&transmit_timeval, NULL);
	clock_gettime(CLOCK_MONOTONIC, &mtx_time);

	msg.xmttime.seconds = htonl(transmit_timeval.tv_sec + OFFSET_1900_1970);
	msg.xmttime.fraction = htonl(transmit_timeval.tv_usec * 1000);

	len = sendto(fd, &msg, sizeof(msg), MSG_DONTWAIT,
						&addr, sizeof(addr));
	if (len < 0) {
		connman_error("Time request for server %s failed (%d/%s)",
			server, errno, strerror(errno));

		if (errno == ENETUNREACH)
			__connman_timeserver_sync_next();

		return;
	}

	if (len != sizeof(msg)) {
		connman_error("Broken time request for server %s", server);
		return;
	}

	/*
	 * Add an exponential retry timeout to retry the existing
	 * request. After a set number of retries, we'll fallback to
	 * trying another server.
	 */

	timeout_id = g_timeout_add_seconds(timeout, send_timeout,
					GUINT_TO_POINTER(timeout));
}

static gboolean next_poll(gpointer user_data)
{
	poll_id = 0;

	if (!timeserver || transmit_fd == 0)
		return FALSE;

	send_packet(transmit_fd, timeserver, NTP_SEND_TIMEOUT);

	return FALSE;
}

static void reset_timeout(void)
{
	if (timeout_id > 0) {
		g_source_remove(timeout_id);
		timeout_id = 0;
	}

	retries = 0;
}

static void decode_msg(void *base, size_t len, struct timeval *tv,
		struct timespec *mrx_time)
{
	struct ntp_msg *msg = base;
	double m_delta, org, rec, xmt, dst;
	double delay, offset;
	static guint transmit_delay;

	if (len < sizeof(*msg)) {
		connman_error("Invalid response from time server");
		return;
	}

	if (!tv) {
		connman_error("Invalid packet timestamp from time server");
		return;
	}

	DBG("flags      : 0x%02x", msg->flags);
	DBG("stratum    : %u", msg->stratum);
	DBG("poll       : %f seconds (%d)",
				LOGTOD(msg->poll), msg->poll);
	DBG("precision  : %f seconds (%d)",
				LOGTOD(msg->precision), msg->precision);
	DBG("root delay : %u seconds (fraction %u)",
			msg->rootdelay.seconds, msg->rootdelay.fraction);
	DBG("root disp. : %u seconds (fraction %u)",
			msg->rootdisp.seconds, msg->rootdisp.fraction);
	DBG("reference  : 0x%04x", msg->refid);

	if (!msg->stratum) {
		/* RFC 4330 ch 8 Kiss-of-Death packet */
		uint32_t code = ntohl(msg->refid);

		connman_info("Skipping server %s KoD code %c%c%c%c",
			timeserver, code >> 24, code >> 16 & 0xff,
			code >> 8 & 0xff, code & 0xff);
		next_server();
		return;
	}

	transmit_delay = LOGTOD(msg->poll);

	if (NTP_FLAGS_LI_DECODE(msg->flags) == NTP_FLAG_LI_NOTINSYNC) {
		DBG("ignoring unsynchronized peer");
		return;
	}


	if (NTP_FLAGS_VN_DECODE(msg->flags) != NTP_FLAG_VN_VER4) {
		if (NTP_FLAGS_VN_DECODE(msg->flags) == NTP_FLAG_VN_VER3) {
			DBG("requested version %d, accepting version %d",
				NTP_FLAG_VN_VER4, NTP_FLAGS_VN_DECODE(msg->flags));
		} else {
			DBG("unsupported version %d", NTP_FLAGS_VN_DECODE(msg->flags));
			return;
		}
	}

	if (NTP_FLAGS_MD_DECODE(msg->flags) != NTP_FLAG_MD_SERVER) {
		DBG("unsupported mode %d", NTP_FLAGS_MD_DECODE(msg->flags));
		return;
	}

	m_delta = mrx_time->tv_sec - mtx_time.tv_sec +
		1.0e-9 * (mrx_time->tv_nsec - mtx_time.tv_nsec);

	org = tv->tv_sec + (1.0e-6 * tv->tv_usec) - m_delta + OFFSET_1900_1970;
	rec = ntohl(msg->rectime.seconds) +
			((double) ntohl(msg->rectime.fraction) / UINT_MAX);
	xmt = ntohl(msg->xmttime.seconds) +
			((double) ntohl(msg->xmttime.fraction) / UINT_MAX);
	dst = tv->tv_sec + (1.0e-6 * tv->tv_usec) + OFFSET_1900_1970;

	DBG("org=%f rec=%f xmt=%f dst=%f", org, rec, xmt, dst);

	offset = ((rec - org) + (xmt - dst)) / 2;
	delay = (dst - org) - (xmt - rec);

	DBG("offset=%f delay=%f", offset, delay);

	/* Remove the timeout, as timeserver has responded */

	reset_timeout();

	/*
	 * Now poll the server every transmit_delay seconds
	 * for time correction.
	 */
	if (poll_id > 0)
		g_source_remove(poll_id);

	DBG("Timeserver %s, next sync in %d seconds", timeserver, transmit_delay);

	poll_id = g_timeout_add_seconds(transmit_delay, next_poll, NULL);

	connman_info("ntp: time slew %+.6f s", offset);

	if (offset < STEPTIME_MIN_OFFSET && offset > -STEPTIME_MIN_OFFSET) {
		struct timeval adj;

		adj.tv_sec = (long) offset;
		adj.tv_usec = (offset - adj.tv_sec) * 1000000;

		DBG("adjusting time");

		if (adjtime(&adj, &adj) < 0) {
			connman_error("Failed to adjust time");
			return;
		}

		DBG("%lu seconds, %lu msecs", adj.tv_sec, adj.tv_usec);
	} else {
		struct timeval cur;
		double dtime;

		gettimeofday(&cur, NULL);
		dtime = offset + cur.tv_sec + 1.0e-6 * cur.tv_usec;
		cur.tv_sec = (long) dtime;
		cur.tv_usec = (dtime - cur.tv_sec) * 1000000;

		DBG("setting time");

		if (settimeofday(&cur, NULL) < 0) {
			connman_error("Failed to set time");
			return;
		}

		DBG("%lu seconds, %lu msecs", cur.tv_sec, cur.tv_usec);
	}
}

static gboolean received_data(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	unsigned char buf[128];
	struct sockaddr_in sender_addr;
	struct msghdr msg;
	struct iovec iov;
	struct cmsghdr *cmsg;
	struct timeval *tv;
	struct timespec mrx_time;
	char aux[128];
	ssize_t len;
	int fd;

	if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		connman_error("Problem with timer server channel");
		channel_watch = 0;
		return FALSE;
	}

	fd = g_io_channel_unix_get_fd(channel);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = aux;
	msg.msg_controllen = sizeof(aux);
	msg.msg_name = &sender_addr;
	msg.msg_namelen = sizeof(sender_addr);

	len = recvmsg(fd, &msg, MSG_DONTWAIT);
	if (len < 0)
		return TRUE;

	if (timeserver_addr.sin_addr.s_addr != sender_addr.sin_addr.s_addr)
		/* only accept messages from the timeserver */
		return TRUE;

	tv = NULL;
	clock_gettime(CLOCK_MONOTONIC, &mrx_time);

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;

		switch (cmsg->cmsg_type) {
		case SCM_TIMESTAMP:
			tv = (struct timeval *) CMSG_DATA(cmsg);
			break;
		}
	}

	decode_msg(iov.iov_base, iov.iov_len, tv, &mrx_time);

	return TRUE;
}

static void start_ntp(char *server)
{
	GIOChannel *channel;
	struct sockaddr_in addr;
	int tos = IPTOS_LOWDELAY, timestamp = 1;

	if (!server)
		return;

	DBG("server %s", server);

	if (channel_watch > 0)
		goto send;

	transmit_fd = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (transmit_fd < 0) {
		connman_error("Failed to open time server socket");
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;

	if (bind(transmit_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		connman_error("Failed to bind time server socket");
		close(transmit_fd);
		return;
	}

	if (setsockopt(transmit_fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
		connman_error("Failed to set type of service option");
		close(transmit_fd);
		return;
	}

	if (setsockopt(transmit_fd, SOL_SOCKET, SO_TIMESTAMP, &timestamp,
						sizeof(timestamp)) < 0) {
		connman_error("Failed to enable timestamp support");
		close(transmit_fd);
		return;
	}

	channel = g_io_channel_unix_new(transmit_fd);
	if (!channel) {
		close(transmit_fd);
		return;
	}

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	g_io_channel_set_close_on_unref(channel, TRUE);

	channel_watch = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				received_data, NULL, NULL);

	g_io_channel_unref(channel);

send:
	send_packet(transmit_fd, server, NTP_SEND_TIMEOUT);
}

int __connman_ntp_start(char *server)
{
	DBG("%s", server);

	if (!server)
		return -EINVAL;

	if (timeserver)
		g_free(timeserver);

	timeserver = g_strdup(server);
	timeserver_addr.sin_addr.s_addr = inet_addr(server);

	start_ntp(timeserver);

	return 0;
}

void __connman_ntp_stop()
{
	DBG("");

	if (poll_id > 0) {
		g_source_remove(poll_id);
		poll_id = 0;
	}

	reset_timeout();

	if (channel_watch > 0) {
		g_source_remove(channel_watch);
		channel_watch = 0;
		transmit_fd = 0;
	}

	if (timeserver) {
		g_free(timeserver);
		timeserver = NULL;
	}
}
