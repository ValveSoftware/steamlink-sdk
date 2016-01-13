/* -*- mode: c; c-file-style: "gnu" -*-
 * crywrap.c -- CryWrap
 * Copyright (C) 2003, 2004 Gergely Nagy <algernon@bonehunter.rulez.org>
 * Copyright (C) 2011 Nikos Mavrogiannopoulos
 *
 * This file is part of CryWrap.
 *
 * CryWrap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CryWrap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crywrap.c
 * CryWrap itself.
 */

#include <config.h>

#include <argp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <grp.h>
#include <idna.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringprep.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>

#include "crywrap.h"
#include "primes.h"

static int system_log(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
    ;

static int system_log_error(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
    ;

static int debug_log(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
    ;

typedef int (*cry_log_func) (const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)))
#endif
    ;

static cry_log_func cry_log = system_log;
static cry_log_func cry_error = system_log_error;

static void tls_audit_log_func(gnutls_session_t session, const char *str)
{
	char peer_name[NI_MAXHOST] = "Unknown";
	gnutls_transport_ptr_t r, s;
	struct sockaddr_storage faddr;
	socklen_t socklen = sizeof(struct sockaddr_storage);

	if (session != NULL) {
		gnutls_transport_get_ptr2(session, &r, &s);

		/* Log the connection */
		if (getpeername
		    ((int) (long) r, (struct sockaddr *) &faddr,
		     &socklen) != 0)
			cry_error("getpeername(): %s", strerror(errno));

		cry_log("Peer %s: %s", peer_name, str);
	} else
		cry_log("%s", str);

}

/** @defgroup globals Global variables.
 * @{
 */

/** An array of pids.
 * This array holds the PIDs of all of our children, indexed by the
 * socket the associated client connected to us.
 */
static pid_t main_pid = -1; /**< Pid of the main process */
static const char *pidfile = _CRYWRAP_PIDFILE; /**< File to log our PID
					    into. */

/** GNUTLS server credentials.
 */
static gnutls_certificate_server_credentials cred;
static gnutls_dh_params dh_params; /**< GNUTLS DH parameters. */
static gnutls_datum dh_file = { (void *) _crywrap_prime_dh_1024, sizeof(_crywrap_prime_dh_1024) };
												 /**< Diffie Hellman parameters */

/** Bugreport address.
 * Used by the argp suite.
 */
const char *argp_program_bug_address = "<bugs-gnutls@gnu.org>";
/** Porgram version.
 * Used by the argp suite.
 */
const char *argp_program_version = __CRYWRAP__ " " _CRYWRAP_VERSION;

/* The certificate and key files */
static char *pem_cert = NULL;
static char *pem_key = NULL;


/** The options CryWrap takes.
 * Used by the argp suite.
 */
static const struct argp_option _crywrap_options[] = {
	{NULL, 0, NULL, 0, "Mandatory options:", 1},
	{"destination", 'd', "IP/PORT", 0, "IP and port to connect to", 1},
	{"listen", 'l', "IP/PORT", 0, "IP and port to listen on", 1},
	{NULL, 0, NULL, 0, "TLS certificates:", 2},
	{"key", 'k', "FILE", 0, "Server key", 2},
	{"cert", 'c', "FILE", 0, "Server certificate", 2},
	{"ca", 'z', "FILE", 0, "CA certificate", 2},
	{"anon", 'a', NULL, 0,
	 "Enable anonymous authentication (no certificates)", 2},
	{"verify", 'v', "LEVEL", OPTION_ARG_OPTIONAL,
	 "Verify clients certificate (1: verify if exists, 2: require)",
	 2},
	{NULL, 0, NULL, 0, "Other options:", 3},
	{"dhparams", 'r', "FILE", 0,
	 "Diffie Hellman (PKCS #3) parameters file", 3},
	{"user", 'u', "UID", 0, "User ID to run as", 3},
	{"pidfile", 'P', "PATH", 0, "File to log the PID into", 3},
	{"priority", 'p', "STRING", 0,
	 "GnuTLS ciphersuite priority string", 3},
	{"inetd", 'i', NULL, 0, "Enable inetd mode", 3},
	{"debug", 'D', NULL, 0, "Run the server into foreground", 3},
	{0, 0, 0, 0, NULL, 0}
};

static error_t _crywrap_config_parse_opt(int key, char *arg,
					 struct argp_state *state);
/** The main argp structure for Crywrap.
 */
static const struct argp _crywrap_argp =
    { _crywrap_options, _crywrap_config_parse_opt, 0,
	__CRYWRAP__ " -- Security for the masses\v"
	    "The --destination option is mandatory, as is --listen if --inetd "
	    "was not used.",
	NULL, NULL, NULL
};

/** @} */

/** @defgroup signal Signal handlers & co.
 * @{
 */

/** SIGCHLD handler
 */
static void _crywrap_sigchld_handler(int sig)
{
	pid_t child;
	int status;

	while ((child = waitpid(-1, &status, WNOHANG)) > (pid_t) 0)
		signal(sig, _crywrap_sigchld_handler);
}

/* Helper functions to load a certificate and key
 * files into memory.
 */
static gnutls_datum_t load_file(const char *file)
{
	gnutls_datum_t loaded_file = { NULL, 0 };

	gnutls_load_file(file, &loaded_file);

	return loaded_file;
}

/** Generic signal handler.
 * This one removes the #pidfile, if necessary.
 */
static void _crywrap_sighandler(int sig)
{
	if (getpid() == main_pid) {
		cry_log("Exiting on signal %d", sig);
		if (pidfile && *pidfile)
			unlink(pidfile);
		closelog();
		exit(0);
	}
}

/** @} */

/** @defgroup parsing Option parsing
 * @{
 */

/** Service resolver.
 * Resolves a service - be it a name or a number.
 *
 * @param serv is the port to resolve.
 *
 * @returns The purt number, or -1 on error.
 */
static int _crywrap_port_get(const char *serv)
{
	int port;
	struct servent *se;

	if (!serv)
		return -1;

	se = getservbyname(serv, "tcp");
	if (!se)
		port = atoi(serv);
	else
		port = ntohs(se->s_port);

	return port;
}

/** Address resolver.
 * Resolves an address - be it numeric or a hostname, IPv4 or IPv6.
 *
 * @param hostname is the host to resolve.
 * @param addr is the structure to put the result into.
 *
 * @returns Zero on success, -1 on error.
 */
static int
_crywrap_addr_get(const char *hostname, struct sockaddr_storage **addr)
{
	struct addrinfo *res;
	struct addrinfo hints;
	ssize_t len;
	char *lz = NULL;

	if (idna_to_ascii_lz(hostname, &lz, 0) != IDNA_SUCCESS)
		return -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;
	*addr = calloc(1, sizeof(struct sockaddr_storage));
	if (*addr == NULL) {
		free(lz);
		return -1;
	}

	if (getaddrinfo(lz, NULL, &hints, &res) != 0) {
		free(lz);
		return -1;
	}

	free(lz);

	switch (res->ai_addr->sa_family) {
	case AF_INET:
		len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
	default:
		freeaddrinfo(res);
		return -1;
	}

	if (len < (ssize_t) res->ai_addrlen) {
		freeaddrinfo(res);
		return -1;
	}

	memcpy(*addr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	return 0;
}

/** Parse a HOST/IP pair.
 * Splits up a given HOST/IP pair, and converts them into structures
 * directly usable by libc routines.
 *
 * @param ip is the HOST/IP pair to parse.
 * @param port is a pointer to an integer where the port number should
 * go.
 * @param addr is the destination of the resolved and parsed IP.
 *
 * @returns Zero on success, -1 on error.
 */
static int
_crywrap_parse_ip(const char *ip, in_port_t * port,
		  struct sockaddr_storage **addr, char **host)
{
	char *s_ip = NULL;
	char *tmp;
	int ret;

	tmp = strchr(ip, '/');

	if (!tmp)
		return -1;

	if (tmp == ip) {
		s_ip = strdup("0.0.0.0");
		*port = (in_port_t) _crywrap_port_get(&ip[1]);
	} else {
		*port = (in_port_t) _crywrap_port_get(&tmp[1]);
		s_ip = strndup(ip, tmp - ip);
	}

	if (!*port)
		return -1;

	if (host)
		*host = strdup(s_ip);

	ret = _crywrap_addr_get(s_ip, addr);
	free(s_ip);
	return ret;
}

/** Argument parsing routine.
 * Used by the argp suite.
 */
static error_t
_crywrap_config_parse_opt(int key, char *arg, struct argp_state *state)
{
	crywrap_config_t *cfg = (crywrap_config_t *) state->input;
	int ret;

	switch (key) {
	case 'D':
		cfg->debug = 1;
		cry_log = debug_log;
		cry_error = debug_log;
		break;
	case 'd':
		if (_crywrap_parse_ip
		    (arg, &cfg->dest.port, &cfg->dest.addr,
		     &cfg->dest.host) < 0)
			argp_error(state,
				   "Could not resolve address: `%s'", arg);
		break;
	case 'l':
		if (_crywrap_parse_ip(arg, &cfg->listen.port,
				      &cfg->listen.addr, NULL) < 0)
			argp_error(state,
				   "Could not resolve address: `%s'", arg);
		break;
	case 'u':
		cfg->uid = atoi(arg);
		break;
	case 'P':
		if (arg && *arg)
			cfg->pidfile = strdup(arg);
		else
			cfg->pidfile = NULL;
		break;
	case 'r':
		if (arg && *arg) {
			dh_file = load_file(arg);
			if (dh_file.data == NULL)
				argp_error(state,
					   "error loading Diffie Hellman parameters file: %s.",
					   arg);
		}
		break;
	case 'p':
		if (arg && *arg) {
			const char *pos;
			ret =
			    gnutls_priority_init(&cfg->priority, arg,
						 &pos);
			if (ret < 0)
				argp_error(state,
					   "error in priority string at: %s.",
					   pos);
		}
		break;
	case 'c':
		if (arg && *arg)
			pem_cert = strdup(arg);
		break;
	case 'k':
		if (arg && *arg)
			pem_key = strdup(arg);
		break;

		break;
	case 'i':
		cfg->inetd = 1;
		break;
	case 'a':
		{
			const char *pos;
			ret =
			    gnutls_priority_init(&cfg->priority,
						 "NORMAL:+ANON-ECDH:+ANON-DH",
						 &pos);
			if (ret < 0)
				argp_error(state,
					   "error in priority string at: %s.",
					   pos);
		}
		cfg->verify = 0;
		cfg->anon = 1;
		break;
	case 'v':
		cfg->verify = (arg) ? atoi(arg) : 1;
		break;
	case 'z':
		ret = gnutls_certificate_set_x509_trust_file(cred, arg,
							     GNUTLS_X509_FMT_PEM);
		if (ret < 0)
			argp_error(state,
				   "error reading X.509 CA file: %s.",
				   gnutls_strerror(ret));
		break;

	case ARGP_KEY_END:
		if (!cfg->inetd) {
			if (!cfg->listen.addr || !cfg->dest.addr)
				argp_error
				    (state,
				     "a listening and a destination address must be set!");
		} else if (!cfg->dest.addr)
			argp_error(state,
				   "a destination address must be set!");
		if (cfg->anon)
			break;
		if (pem_cert == NULL || pem_key == NULL)
			ret =
			    gnutls_certificate_set_x509_key_file(cred,
								 _CRYWRAP_PEMFILE,
								 _CRYWRAP_PEMFILE,
								 GNUTLS_X509_FMT_PEM);
		else
			ret =
			    gnutls_certificate_set_x509_key_file(cred,
								 pem_cert,
								 pem_key,
								 GNUTLS_X509_FMT_PEM);

		if (ret < 0)
			argp_error(state,
				   "Error reading X.509 key or certificate file: %s",
				   gnutls_strerror(ret));
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/** Configuration parsing.
 * Sets up the default values, and parses the command-line options
 * using argp.
 *
 * @note Does not return if an error occurred.
 */
static crywrap_config_t *_crywrap_config_parse(int argc, char **argv)
{
	crywrap_config_t *config =
	    (crywrap_config_t *) malloc(sizeof(crywrap_config_t));

	if (config == NULL)
		return NULL;

	config->listen.port = 0;
	config->listen.addr = NULL;
	config->dest.port = 0;
	config->dest.addr = NULL;
	config->priority = NULL;
	config->uid = _CRYWRAP_UID;
	config->pidfile = _CRYWRAP_PIDFILE;
	config->inetd = 0;
	config->anon = 0;
	config->verify = 0;

	argp_parse(&_crywrap_argp, argc, argv, 0, 0, config);

	if (config->priority == NULL)
		gnutls_priority_init(&config->priority, "NORMAL", NULL);

	return config;
}

/** @} */

/** @defgroup tls Lower-level TLS routines.
 * @{
 */

/** Create a GNUTLS session.
 * Initialises the cyphers and the session database for a new TLS
 * session.
 *
 * @returns The newly created TLS session.
 */
static gnutls_session_t
_crywrap_tls_session_create(const crywrap_config_t * config)
{
	gnutls_session_t session;
	int ret;

	gnutls_init(&session, GNUTLS_SERVER);

	if (config->anon) {
		gnutls_credentials_set(session, GNUTLS_CRD_ANON, cred);
	} else {
		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				       cred);
	}

	ret = gnutls_priority_set(session, config->priority);
	if (ret < 0) {
		cry_error("Error setting priority %s: ",
			  gnutls_strerror(ret));
		exit(4);
	}

	if (config->verify == 1)
		gnutls_certificate_server_set_request(session,
						      GNUTLS_CERT_REQUEST);
	else if (config->verify == 2)
		gnutls_certificate_server_set_request(session,
						      GNUTLS_CERT_REQUIRE);

	return session;
}

/** Generate initial DH and RSA params.
 * Loads the pre-generated DH primes.
 */
static void _crywrap_tls_init(void)
{

	gnutls_dh_params_init(&dh_params);
	gnutls_dh_params_import_pkcs3(dh_params, &dh_file,
				      GNUTLS_X509_FMT_PEM);

	gnutls_certificate_set_dh_params(cred, dh_params);
}

/** @} */

/** @defgroup networking Networking
 * @{
 */

/** Bind to an address.
 * This one binds to an address, handles errors and anything that may
 * arise.
 *
 * @param ai is the address information.
 * @param listen_port is the port to bind to, and listen on.
 *
 * @returns The bound filedescriptor, or -1 on error.
 */
static int _crywrap_bind(const struct addrinfo *ai, int listen_port)
{
	int ret;
	const int one = 1;
	int listenfd;
	char sock_name[NI_MAXHOST];

	listenfd = socket(ai->ai_family, SOCK_STREAM, IPPROTO_IP);
	if (listenfd == -1) {
		cry_error("socket: %s", strerror(errno));
		return -1;
	}

	memset(sock_name, 0, sizeof(sock_name));
	getnameinfo((struct sockaddr *) ai->ai_addr, ai->ai_addrlen,
		    sock_name, sizeof(sock_name), NULL, 0, NI_NUMERICHOST);

	switch (ai->ai_family) {
	case AF_INET6:
		((struct sockaddr_in6 *) (ai->ai_addr))->sin6_port =
		    listen_port;
		break;
	case AF_INET:
		((struct sockaddr_in *) (ai->ai_addr))->sin_port =
		    listen_port;
		break;
	}

	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
			 &one, sizeof(one));
	if (ret != 0) {
		cry_error("setsockopt: %s (%s)", strerror(errno),
			  sock_name);
		return -1;
	}

	ret = bind(listenfd, ai->ai_addr, ai->ai_addrlen);
	if (ret != 0) {
		cry_error("bind to %s failed: %s", sock_name,
			  strerror(errno));
		return -1;
	}

	if (listen(listenfd, _CRYWRAP_MAXCONN) != 0) {
		cry_error("listen on %s failed: %s", sock_name,
			  strerror(errno));
		return -1;
	}

	cry_log("Socket bound to port %d on %s.", ntohs(listen_port),
		sock_name);

	return listenfd;
}

/** Set up a listening socket.
 * Sets up a listening socket on all the required addresses.
 *
 * @param config holds the CryWrap configuration, from where the
 * listen address and port will be extracted.
 *
 * @returns The listening FD on success, -1 on error.
 */
static int _crywrap_listen(const crywrap_config_t * config)
{
	struct addrinfo *cur;
	int ret;

	cur = calloc(1, sizeof(struct addrinfo));
	if (cur == NULL)
		return -1;

	cur->ai_family = config->listen.addr->ss_family;

	switch (cur->ai_family) {
	case AF_INET6:
		cur->ai_addrlen = sizeof(struct sockaddr_in6);
		break;
	case AF_INET:
		cur->ai_addrlen = sizeof(struct sockaddr_in);
		break;
	default:
		ret = -1;
		goto cleanup;
	}

	cur->ai_addr = malloc(cur->ai_addrlen);
	if (cur->ai_addr == NULL)
		return -1;

	memcpy(cur->ai_addr, config->listen.addr, cur->ai_addrlen);

	ret = _crywrap_bind(cur, htons(config->listen.port));
	free(cur->ai_addr);

 cleanup:
	free(cur);

	return ret;
}

/** Connect to a remote server.
 * Estabilishes a connection to a remote server, and handles all
 * errors and anything that may arise during this process.
 *
 * @param addr is the address of the remote server.
 * @param port is the port to connect to.
 *
 * @returns the connected socket on success, otherwise it exits.
 */
static int
_crywrap_remote_connect(const struct sockaddr_storage *addr, int port)
{
	struct addrinfo *cur;
	int sock;

	cur = calloc(1, sizeof(struct addrinfo));
	if (cur == NULL)
		return -1;

	cur->ai_family = addr->ss_family;

	switch (cur->ai_family) {
	case AF_INET6:
		cur->ai_addrlen = sizeof(struct sockaddr_in6);
		break;
	case AF_INET:
		cur->ai_addrlen = sizeof(struct sockaddr_in);
		break;
	default:
		sock = -1;
		goto cleanup;
	}

	cur->ai_addr = malloc(cur->ai_addrlen);
	if (cur->ai_addr == NULL)
		return -1;

	memcpy(cur->ai_addr, addr, cur->ai_addrlen);

	switch (cur->ai_family) {
	case AF_INET6:
		((struct sockaddr_in6 *) (cur->ai_addr))->sin6_port = port;
		break;
	case AF_INET:
		((struct sockaddr_in *) (cur->ai_addr))->sin_port = port;
		break;
	}

	sock = socket(cur->ai_family, SOCK_STREAM, IPPROTO_IP);
	if (sock < 0) {
		cry_error("socket(): %s", strerror(errno));
		exit(1);
	}

	if (connect(sock, cur->ai_addr, cur->ai_addrlen) < 0) {
		cry_error("connect(): %s", strerror(errno));
		exit(1);
	}

	free(cur->ai_addr);

 cleanup:
	free(cur);

	return sock;
}

/** @} */

/** @defgroup crywrap Main CryWrap code.
 * @{
 */

/** Drop privileges.
 * Drop privileges, if running as root.
 * Upon failure, it will make CryWrap exit.
 */
static void _crywrap_privs_drop(const crywrap_config_t * config)
{
	struct passwd *pwd;

	if (getuid() != 0) {
		cry_log("%s",
			"Not running as root, not dropping privileges.");
		return;
	}

	if ((pwd = getpwuid(config->uid)) == NULL) {
		cry_error("getpwuid(): %s", strerror(errno));
		exit(1);
	}

	if (initgroups(pwd->pw_name, pwd->pw_gid) == -1) {
		cry_error("initgroups(): %s", strerror(errno));
		exit(1);
	}

	if (setgid(pwd->pw_gid) == -1) {
		cry_error("setgid(): %s", strerror(errno));
		exit(1);
	}

	if (setuid(config->uid)) {
		cry_error("setuid(): %s", strerror(errno));
		exit(1);
	}
}

/** Set up the PID file.
 * Checks if a #pidfile already exists, and create one - containing the
 * current PID - if one does not.
 *
 * @note Exits upon error.
 */
static void _crywrap_setup_pidfile(const crywrap_config_t * config)
{
	char mypid[128];
	int pidfilefd;

	if (!config->pidfile || !*(config->pidfile))
		return;

	if (!access(config->pidfile, F_OK)) {
		cry_error("Pidfile (%s) already exists. Exiting.",
			  config->pidfile);
		exit(1);
	}
	if ((pidfilefd = open(config->pidfile,
			      O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
		cry_error("Cannot create pidfile (%s): %s.\n",
			  config->pidfile, strerror(errno));
		exit(1);
	}
	fchown(pidfilefd, config->uid, (gid_t) - 1);

	main_pid = getpid();
	snprintf(mypid, sizeof(mypid), "%d\n", main_pid);
	write(pidfilefd, mypid, strlen(mypid));
	close(pidfilefd);
	pidfile = config->pidfile;
}


/** Handles one client.
 * This one connects to the remote server, and proxies every traffic
 * between our client and the server.
 *
 * @param config is the main CryWrap configuration structure.
 * @param insock is the socket through which the client sends input.
 * @param outsock is the socket through which we send output.
 *
 * @note Exits on error.
 */
static int
_crywrap_do_one(const crywrap_config_t * config, int insock, int outsock)
{
	int sock, ret, tls_pending;
	gnutls_session_t session;
	char buffer[_CRYWRAP_MAXBUF + 2];
	fd_set fdset;
	unsigned int status = 0;
	struct sockaddr_storage faddr;
	socklen_t socklen = sizeof(struct sockaddr_storage);
	char peer_name[NI_MAXHOST];

	/* Log the connection */
	if (getpeername(insock, (struct sockaddr *) &faddr, &socklen) != 0)
		cry_error("getpeername(): %s", strerror(errno));
	else {
		getnameinfo((struct sockaddr *) &faddr,
			    sizeof(struct sockaddr_storage), peer_name,
			    sizeof(peer_name), NULL, 0, NI_NUMERICHOST);
		cry_log("Accepted connection from %s on %d to %s/%d",
			peer_name, insock, config->dest.host,
			config->dest.port);
	}

	/* Do the handshake with our peer */
	session = _crywrap_tls_session_create(config);
	gnutls_transport_set_ptr2(session,
				  (gnutls_transport_ptr_t) insock,
				  (gnutls_transport_ptr_t) outsock);

	do {
		ret = gnutls_handshake(session);
	}
	while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);

	if (ret < 0) {
		cry_error("Handshake failed: %s", gnutls_strerror(ret));
		gnutls_alert_send_appropriate(session, ret);
		goto error;
	}

	/* Verify the client's certificate, if any. */
	if (config->verify) {
		ret = gnutls_certificate_verify_peers2(session, &status);
		if (ret < 0)
			cry_log
			    ("Error getting certificate from client: %s",
			     gnutls_strerror(ret));

		if (ret == 0 && status != 0) {
			if (status & GNUTLS_CERT_INVALID)
				cry_log("%s",
					"Client certificate not trusted or invalid");
		}

		if (config->verify > 0 && status != 0) {
			ret = -1;
			gnutls_alert_send(session, GNUTLS_AL_FATAL,
					  GNUTLS_A_INSUFFICIENT_SECURITY);
			goto error;
		}
	}

	/* Connect to the remote host */
	sock = _crywrap_remote_connect(config->dest.addr,
				       htons(config->dest.port));

	for (;;) {
		FD_ZERO(&fdset);
		FD_SET(insock, &fdset);
		FD_SET(sock, &fdset);

		memset(buffer, 0, _CRYWRAP_MAXBUF + 1);

		tls_pending = 0;

		if (gnutls_record_check_pending(session) > 0)
			tls_pending = 1;
		else {
			select(sock + 1, &fdset, NULL, NULL, NULL);
			if (FD_ISSET(insock, &fdset))
				tls_pending = 1;
		}
		/* TLS client */
		if (tls_pending != 0) {
			ret =
			    gnutls_record_recv(session, buffer,
					       _CRYWRAP_MAXBUF);
			if (ret == 0) {
				cry_log("%s",
					"Peer has closed the GNUTLS connection");
				break;
			} else if (ret < 0) {
				cry_log("Received corrupted data: %s.",
					gnutls_strerror(ret));
				break;
			} else
				send(sock, buffer, ret, 0);
		}

		/* Remote server */
		if (FD_ISSET(sock, &fdset)) {
			ret = recv(sock, buffer, _CRYWRAP_MAXBUF, 0);
			if (ret == 0) {
				cry_log("%s",
					"Server has closed the connection");
				break;
			} else if (ret < 0) {
				cry_log("Received corrupted data: %s.",
					strerror(errno));
				break;
			} else {
				int r, o = 0;

				do {
					r = gnutls_record_send(session,
							       &buffer[o],
							       ret - o);
					o += r;
				} while (r > 0 && ret > o);

				if (r < 0)
					cry_log
					    ("Received corrupt data: %s",
					     gnutls_strerror(r));
			}
		}
	}

      error:
	gnutls_bye(session, GNUTLS_SHUT_WR);
	gnutls_deinit(session);
	close(insock);
	close(outsock);

	return (ret == 0) ? 0 : 1;
}

/** CryWrap entry point.
 * This is the main entry point - controls the whole program and so
 * on...
 */
int main(int argc, char **argv, char **envp)
{
	crywrap_config_t *config;
	int server_socket;

	openlog(__CRYWRAP__, LOG_PID, LOG_DAEMON);

	gnutls_global_set_audit_log_function(tls_audit_log_func);

	if (gnutls_global_init() < 0) {
		cry_error("%s", "Global TLS state initialisation failed.");
		exit(1);
	}
	if (gnutls_certificate_allocate_credentials(&cred) < 0) {
		cry_error("%s", "Couldn't allocate credentials.");
		exit(1);
	}

	stringprep_locale_charset();

	config = _crywrap_config_parse(argc, argv);

	_crywrap_tls_init();

	if (config->inetd) {
		_crywrap_privs_drop(config);
		exit(_crywrap_do_one(config, 0, 1));
	}

	if (!config->debug)
		if (daemon(0, 0)) {
			cry_error("daemon: %s", strerror(errno));
			exit(1);
		}

	cry_log("%s", "Crywrap starting...");

	server_socket = _crywrap_listen(config);
	if (server_socket < 0)
		exit(1);

	if (!config->debug)
		_crywrap_setup_pidfile(config);
	_crywrap_privs_drop(config);

	signal(SIGTERM, _crywrap_sighandler);
	signal(SIGQUIT, _crywrap_sighandler);
	signal(SIGSEGV, _crywrap_sighandler);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, _crywrap_sigchld_handler);

	cry_log("%s", "Accepting connections");


	for (;;) {
		int csock;
		int child;

		csock = accept(server_socket, NULL, NULL);
		if (csock < 0)
			continue;

		child = fork();
		switch (child) {
		case 0:
			exit(_crywrap_do_one(config, csock, csock));
			break;
		case -1:
			cry_error("%s", "Forking error.");
			exit(1);
			break;
		}
		close(csock);
	}

	return 0;
}

static int system_log(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsyslog(LOG_NOTICE, fmt, args);
	va_end(args);

	return 0;
}

static int system_log_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsyslog(LOG_ERR, fmt, args);
	va_end(args);

	return 0;
}

static int debug_log(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	puts("");
	va_end(args);

	return 0;
}

/** @} */
