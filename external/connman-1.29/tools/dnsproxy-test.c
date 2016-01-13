/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2013  Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <glib.h>
#include <fcntl.h>

#if 0
#define DEBUG
#endif
#ifdef DEBUG
#include <stdio.h>

#define LOG(fmt, arg...) do { \
	fprintf(stdout, "%s:%s() " fmt "\n",   \
			__FILE__, __func__ , ## arg); \
} while (0)
#else
#define LOG(fmt, arg...)
#endif

static unsigned char msg[] = {
	0x00, 0x1c, /* len 28 */
	0x31, 0x82, /* tran id */
	0x01, 0x00, /* flags (recursion required) */
	0x00, 0x01, /* questions (1) */
	0x00, 0x00, /* answer rr */
	0x00, 0x00, /* authority rr */
	0x00, 0x00, /* additional rr */
	0x06, 0x6c, 0x6f, 0x6c, 0x67, 0x65, 0x30, /* lolge0, just some name */
	0x03, 0x63, 0x6f, 0x6d, /* com */
	0x00,       /* null terminator */
	0x00, 0x01, /* type A */
	0x00, 0x01, /* class IN */
};

static unsigned char msg2[] = {
	0x00, 0x1c, /* len 28 */
	0x31, 0x83, /* tran id */
	0x01, 0x00, /* flags (recursion required) */
	0x00, 0x01, /* questions (1) */
	0x00, 0x00, /* answer rr */
	0x00, 0x00, /* authority rr */
	0x00, 0x00, /* additional rr */
	0x06, 0x6c, 0x6f, 0x67, 0x6c, 0x67, 0x65, /* loelge */
	0x03, 0x63, 0x6f, 0x6d, /* com */
	0x00,       /* null terminator */
	0x00, 0x01, /* type A */
	0x00, 0x01, /* class IN */

	0x00, 0x1c, /* len 28 */
	0x31, 0x84, /* tran id */
	0x01, 0x00, /* flags (recursion required) */
	0x00, 0x01, /* questions (1) */
	0x00, 0x00, /* answer rr */
	0x00, 0x00, /* authority rr */
	0x00, 0x00, /* additional rr */
	0x06, 0x6c, 0x6f, 0x67, 0x6c, 0x67, 0x65, /* loelge */
	0x03, 0x6e, 0x65, 0x74, /* net */
	0x00,       /* null terminator */
	0x00, 0x01, /* type A */
	0x00, 0x01, /* class IN */
};

static unsigned char msg_invalid[] = {
	0x00, 0x1c, /* len 28 */
	0x31, 0xC0, /* tran id */
};

static int create_tcp_socket(int family)
{
	int sk, err;

	sk = socket(family, SOCK_STREAM, IPPROTO_TCP);
	if (sk < 0) {
		err = errno;
		LOG("Failed to create TCP socket %d/%s", err, strerror(err));
		return -err;
	}

	return sk;
}

static int create_udp_socket(int family)
{
	int sk, err;

	sk = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (sk < 0) {
		err = errno;
		LOG("Failed to create UDP socket %d/%s", err, strerror(err));
		return -err;
	}

	return sk;
}

static int connect_tcp_socket(char *server)
{
	int sk;
	int err = 0;

	struct addrinfo hints, *rp;

	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICSERV | AI_NUMERICHOST;
	getaddrinfo(server, "53", &hints, &rp);

	sk = create_tcp_socket(rp->ai_family);
	err = sk;

	LOG("sk %d family %d", sk, rp->ai_family);

	if (sk >= 0 && connect(sk, rp->ai_addr, rp->ai_addrlen) < 0) {
		err = -errno;
		close(sk);
		sk = -1;

		fprintf(stderr, "Failed to connect to DNS server at %s "
				"errno %d/%s\n",
			server, -err, strerror(-err));
	}

	if (sk >= 0)
		fcntl(sk, F_SETFL, O_NONBLOCK);

	freeaddrinfo(rp);

	return err;
}

static int send_msg(int sk, unsigned char *msg, unsigned int len,
		int sleep_between)
{
	unsigned int i;
	int err;

	for (i = 0; i < len; i++) {
		err = write(sk, &msg[i], 1);
		if (err < 0) {
			err = -errno;
			LOG("write failed errno %d/%s", -err, strerror(-err));
		}

		g_assert_cmpint(err, >=, 0);
		if (err < 0)
			return err;

		if (sleep_between)
			usleep(1000);
	}

	return 0;
}

static int connect_udp_socket(char *server, struct sockaddr *sa,
							socklen_t *sa_len)
{
	int sk;
	int err = 0;

	struct addrinfo hints, *rp;

	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICSERV | AI_NUMERICHOST;
	getaddrinfo(server, "53", &hints, &rp);

	sk = create_udp_socket(rp->ai_family);
	err = sk;

	LOG("sk %d family %d", sk, rp->ai_family);

	if (sk < 0) {
		err = -errno;
		fprintf(stderr, "Failed to connect to DNS server at %s "
				"errno %d/%s\n",
			server, -err, strerror(-err));
	} else {
		memcpy(sa, rp->ai_addr, *sa_len);
		*sa_len = rp->ai_addrlen;
	}

	freeaddrinfo(rp);

	return err;
}

static int sendto_msg(int sk, struct sockaddr *sa, socklen_t salen,
		unsigned char *msg, unsigned int len)
{
	int err;

	err = sendto(sk, msg, len, MSG_NOSIGNAL, sa, salen);
	if (err < 0) {
		err = -errno;
		LOG("sendto failed errno %d/%s", -err, strerror(-err));
	}

	g_assert_cmpint(err, >=, 0);
	if (err < 0)
		return err;

	return 0;
}

static unsigned short get_id(void)
{
	return random();
}

static void change_msg(unsigned char *msg, unsigned int tranid,
		unsigned int host_loc, unsigned char host_count)
{
	unsigned short id = get_id();

	msg[tranid] = id >> 8;
	msg[tranid+1] = id;
	msg[host_loc] = host_count;
}

static void change_msg2(unsigned char *msg, unsigned char host_count)
{
	change_msg(msg, 2, 20, host_count);
	change_msg(msg, 32, 50, host_count+1);
}

static int receive_message(int sk, int timeout, int expected, int *server_closed)
{
	int ret, received = 0;
	unsigned char buf[512];

	while (timeout > 0) {
		ret = read(sk, buf, sizeof(buf));
		if (ret > 0) {
			LOG("received %d bytes from server %d", ret, sk);
			received += ret;
			if (received >= expected)
				break;
		} else if (ret == 0) {
			LOG("server %d closed socket", sk);
			if (server_closed)
				*server_closed = 1;
			break;
		} else {
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				break;
		}

		sleep(1);
		timeout--;
	}

	return received;
}

static int receive_from_message(int sk, struct sockaddr *sa, socklen_t len,
				int timeout, int expected)
{
	int ret, received = 0;
	unsigned char buf[512];

	fcntl(sk, F_SETFL, O_NONBLOCK);

	while (timeout > 0) {
		ret = recvfrom(sk, buf, sizeof(buf), 0, sa, &len);
		if (ret > 0) {
			LOG("received %d bytes from server %d", ret, sk);
			received += ret;
			if (received >= expected)
				break;
		} else if (ret < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				break;
		}

		sleep(1);
		timeout--;
	}

	return received;
}

static void test_ipv4_udp_msg(void)
{
	int sk, received = 0;
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	sk = connect_udp_socket("127.0.0.1", (struct sockaddr *)&sa, &len);
	g_assert_cmpint(sk, >=, 0);
	change_msg(msg, 2, 20, '1');
	sendto_msg(sk, (struct sockaddr *)&sa, len, msg, sizeof(msg));
	received = receive_from_message(sk, (struct sockaddr *)&sa, len,
					10, sizeof(msg));
	close(sk);

	g_assert_cmpint(received, >=, sizeof(msg));
}

static void test_ipv6_udp_msg(void)
{
	int sk, received = 0;
	struct sockaddr_in6 sa;
	socklen_t len = sizeof(sa);

	sk = connect_udp_socket("::1", (struct sockaddr *)&sa, &len);
	g_assert_cmpint(sk, >=, 0);
	change_msg(msg, 2, 20, '1');
	sendto_msg(sk, (struct sockaddr *)&sa, len, msg, sizeof(msg));
	received = receive_from_message(sk, (struct sockaddr *)&sa, len,
					10, sizeof(msg));
	close(sk);

	g_assert_cmpint(received, >=, sizeof(msg));
}

static void test_partial_ipv4_tcp_msg(void)
{
	int sk, received = 0;

	sk = connect_tcp_socket("127.0.0.1");
	g_assert_cmpint(sk, >=, 0);
	change_msg(msg, 2, 20, '1');
	send_msg(sk, msg, sizeof(msg), 1);
	received = receive_message(sk, 10, sizeof(msg), NULL);
	close(sk);

	g_assert_cmpint(received, >=, sizeof(msg));
}

static void test_partial_ipv6_tcp_msg(void)
{
	int sk, received = 0;

	sk = connect_tcp_socket("::1");
	g_assert_cmpint(sk, >=, 0);
	change_msg(msg, 2, 20, '2');
	send_msg(sk, msg, sizeof(msg), 1);
	received = receive_message(sk, 10, sizeof(msg), NULL);
	close(sk);

	g_assert_cmpint(received, >=, sizeof(msg));
}

static void test_multiple_ipv4_tcp_msg(void)
{
	int sk, received = 0, server_closed = 0;

	sk = connect_tcp_socket("127.0.0.1");
	g_assert_cmpint(sk, >=, 0);
	change_msg2(msg2, '1');
	send_msg(sk, msg2, sizeof(msg2), 0);
	received = receive_message(sk, 35, sizeof(msg2), &server_closed);
	close(sk);

	/* If the final DNS server refuses to serve us, then do not consider
	 * it an error. This happens very often with unbound.
	 */
	if (server_closed == 0)
		g_assert_cmpint(received, >=, sizeof(msg2));
}

static void test_multiple_ipv6_tcp_msg(void)
{
	int sk, received = 0, server_closed = 0;

	sk = connect_tcp_socket("::1");
	g_assert_cmpint(sk, >=, 0);
	change_msg2(msg2, '2');
	send_msg(sk, msg2, sizeof(msg2), 0);
	received = receive_message(sk, 35, sizeof(msg2), &server_closed);
	close(sk);

	if (server_closed == 0)
		g_assert_cmpint(received, >=, sizeof(msg2));
}

static void test_failure_tcp_msg(void)
{
	int sk, received = 0;

	sk = connect_tcp_socket("127.0.0.1");
	g_assert_cmpint(sk, >=, 0);
	send_msg(sk, msg_invalid, sizeof(msg_invalid), 0);
	received = receive_message(sk, 10, 0, NULL);
	close(sk);

	g_assert_cmpint(received, ==, 0);
}

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/dnsproxy/ipv4 udp msg",
			test_ipv4_udp_msg);

	g_test_add_func("/dnsproxy/ipv6 udp msg",
			test_ipv6_udp_msg);

	g_test_add_func("/dnsproxy/failure tcp msg ",
			test_failure_tcp_msg);

	g_test_add_func("/dnsproxy/partial ipv4 tcp msg",
			test_partial_ipv4_tcp_msg);

	g_test_add_func("/dnsproxy/partial ipv6 tcp msg",
			test_partial_ipv6_tcp_msg);

	g_test_add_func("/dnsproxy/partial ipv4 tcp msg from cache",
			test_partial_ipv4_tcp_msg);

	g_test_add_func("/dnsproxy/partial ipv6 tcp msg from cache",
			test_partial_ipv6_tcp_msg);

	g_test_add_func("/dnsproxy/multiple ipv4 tcp msg",
			test_multiple_ipv4_tcp_msg);

	g_test_add_func("/dnsproxy/multiple ipv6 tcp msg",
			test_multiple_ipv6_tcp_msg);

	g_test_add_func("/dnsproxy/multiple ipv4 tcp msg from cache",
			test_multiple_ipv4_tcp_msg);

	g_test_add_func("/dnsproxy/multiple ipv6 tcp msg from cache",
			test_multiple_ipv6_tcp_msg);

	return g_test_run();
}
