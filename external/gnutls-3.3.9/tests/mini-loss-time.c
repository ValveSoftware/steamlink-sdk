/*
 * Copyright (C) 2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

int main()
{
	exit(77);
}

#else

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#include <signal.h>

#include "utils.h"

/* This program tests whether a DTLS handshake would timeout
 * in a minute.
 */

static void print_type(const unsigned char *buf, int size)
{
	if (buf[0] == 22 && size >= 13) {
		if (buf[13] == 1)
			fprintf(stderr, "Client Hello\n");
		else if (buf[13] == 2)
			fprintf(stderr, "Server Hello\n");
		else if (buf[13] == 12)
			fprintf(stderr, "Server Key exchange\n");
		else if (buf[13] == 14)
			fprintf(stderr, "Server Hello Done\n");
		else if (buf[13] == 11)
			fprintf(stderr, "Certificate\n");
		else if (buf[13] == 16)
			fprintf(stderr, "Client Key Exchange\n");
		else if (buf[4] == 1)
			fprintf(stderr, "Finished\n");
		else if (buf[13] == 11)
			fprintf(stderr, "Server Hello Done\n");
		else
			fprintf(stderr, "Unknown handshake\n");
	} else if (buf[0] == 20) {
		fprintf(stderr, "Change Cipher Spec\n");
	} else
		fprintf(stderr, "Unknown\n");
}

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

/* A very basic TLS client, with anonymous authentication.
 */

static int counter;
static int packet_to_lose;
gnutls_session_t session;

static ssize_t
push(gnutls_transport_ptr_t tr, const void *data, size_t len)
{
	int fd = (long int) tr;

	counter++;

	if (packet_to_lose != -1 && packet_to_lose == counter) {
		if (debug) {
			fprintf(stderr, "Discarding packet %d: ", counter);
			print_type(data, len);
		}

		packet_to_lose = 1;
		counter = 0;
		return len;
	}
	return send(fd, data, len, 0);
}

static void client(int fd, int packet)
{
	int ret;
	gnutls_anon_client_credentials_t anoncred;
	/* Need to enable anonymous KX specifically. */

	global_init();

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_anon_allocate_client_credentials(&anoncred);

	/* Initialize TLS session
	 */
	gnutls_init(&session, GNUTLS_CLIENT | GNUTLS_DATAGRAM);
	gnutls_dtls_set_mtu(session, 1500);

	/* Use default priorities */
	gnutls_priority_set_direct(session,
				   "NONE:+VERS-DTLS1.0:+CIPHER-ALL:+MAC-ALL:+SIGN-ALL:+COMP-ALL:+ANON-ECDH:+CURVE-ALL",
				   NULL);

	/* put the anonymous credentials to the current session
	 */
	gnutls_credentials_set(session, GNUTLS_CRD_ANON, anoncred);

	counter = 0;
	packet_to_lose = packet;

	gnutls_transport_set_int(session, fd);
	gnutls_transport_set_push_function(session, push);

	/* Perform the TLS handshake
	 */
	do {
		ret = gnutls_handshake(session);
	}
	while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	gnutls_deinit(session);
	gnutls_global_deinit();

	if (ret < 0) {
		if (ret == GNUTLS_E_TIMEDOUT)
			return;
		fail("client: Handshake failed\n");
		gnutls_perror(ret);
		exit(1);
	} else {
		if (debug)
			success("client: Handshake was completed\n");
	}

	exit(1);
}


/* These are global */
gnutls_anon_server_credentials_t anoncred;
pid_t child;

static gnutls_session_t initialize_tls_session(void)
{
	gnutls_session_t session;

	gnutls_init(&session, GNUTLS_SERVER | GNUTLS_DATAGRAM);
	gnutls_dtls_set_mtu(session, 1500);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	gnutls_priority_set_direct(session,
				   "NONE:+VERS-DTLS1.0:+CIPHER-ALL:+MAC-ALL:+SIGN-ALL:+COMP-ALL:+ANON-ECDH:+CURVE-ALL",
				   NULL);

	gnutls_credentials_set(session, GNUTLS_CRD_ANON, anoncred);

	return session;
}

static void server(int fd, int packet)
{
	int ret;
	/* this must be called once in the program
	 */
	global_init();

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_anon_allocate_server_credentials(&anoncred);

	session = initialize_tls_session();

	counter = 0;
	packet_to_lose = packet;

	gnutls_transport_set_int(session, fd);
	gnutls_transport_set_push_function(session, push);

	do {
		ret = gnutls_handshake(session);
	}
	while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	gnutls_deinit(session);
	gnutls_global_deinit();

	if (ret < 0) {
		return;
	}
}

static void start(int server_packet, int client_packet)
{
	int fd[2];
	int ret;

	if (debug)
		fprintf(stderr, "\nWill discard %s packet %d\n",
			(client_packet != -1) ? "client" : "server",
			(client_packet !=
			 -1) ? client_packet : server_packet);

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
	if (ret < 0) {
		perror("socketpair");
		exit(1);
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
		exit(1);
	}

	if (child) {
		/* parent */
		close(fd[1]);
		server(fd[0], server_packet);
		close(fd[0]);
		kill(child, SIGTERM);
	} else {
		close(fd[0]);
		client(fd[1], client_packet);
		close(fd[1]);
		exit(0);
	}
}

static void ch_handler(int sig)
{
	int status;
	wait(&status);
	if (WEXITSTATUS(status) != 0)
		fail("Child died with status %d\n", WEXITSTATUS(status));
	return;
}

void doit(void)
{
	time_t tstart, tstop;

	signal(SIGCHLD, ch_handler);

	tstart = time(0);
	start(2, -1);

	tstop = time(0);

	tstop = tstop - tstart;

	if (!(tstop < 70 && tstop > 55))
		fail("Time difference: %u\n", (unsigned) tstop);
}

#endif				/* _WIN32 */
