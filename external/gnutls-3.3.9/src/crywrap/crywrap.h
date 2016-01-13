/* -*- mode: c; c-file-style: "gnu" -*-
 * crywrap.h -- Global definitions for CryWrap
 * Copyright (C) 2003, 2004 Gergely Nagy <algernon@bonehunter.rulez.org>
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

/** @file crywrap.h
 * Global variables and declarations for CryWrap.
 *
 * All of the global types, structures and whatnot are declared in
 * this file. Not variables, though. Those are in crywrap.c.
 */

#ifndef _CRYWRAP_H
#define _CRYWRAP_H 1 /**< crywrap.h multi-inclusion guard. */

/** @defgroup defaults Built-in defaults.
 * @{
 */
#define __CRYWRAP__ "crywrap" /**< Software name. */
/** Software version.
 */
#define _CRYWRAP_VERSION "0.2." CRYWRAP_PATCHLEVEL
/** Configuration directory.
 */
#define _CRYWRAP_CONFDIR SYSCONFDIR "/crywrap"
#define _CRYWRAP_UID 65534 /**< Default UID to run as. */
/** Default PID file.
 */
#define _CRYWRAP_PIDFILE "/var/run/crywrap.pid"
/** Maximum number of clients supported.
 */
#define _CRYWRAP_MAXCONN 1024
/** Maximum I/O buffer size.
 */
#define _CRYWRAP_MAXBUF 64 * 1024
/** Default server certificate and key.
 */
#define _CRYWRAP_PEMFILE _CRYWRAP_CONFDIR "/server.pem"
/** @} */

/** Configuration structure.
 * Most of the CryWrap configuration - those options that are settable
 * via the command-line are stored in a variable of this type.
 */
typedef struct {
  /** Properties of the listening socket.
   */
	struct {
		in_port_t port;
		struct sockaddr_storage *addr;
	} listen;

  /** Properties of the destination socket.
   */
	struct {
		in_port_t port;
		char *host;
		struct sockaddr_storage *addr;
	} dest;

	gnutls_priority_t priority;
			      /**< GnuTLS priority string. */
	const char *pidfile;
		       /**< File to store our PID in. */
	uid_t uid;
	     /**< User ID to run as. */
	int inetd;
	     /**< InetD-mode toggle. */
	int anon;
	    /**< Anon-DH toggle. */
	int verify;
	      /**< Client certificate verify level. */
	int debug;
} crywrap_config_t;

	 /** @} *//* End of the Options group */

#endif				/* !_CRYWRAP_H */
