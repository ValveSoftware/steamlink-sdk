/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "ortp-config.h" /*needed for HAVE_SYS_UIO_H and HAVE_ARC4RANDOM */
#endif
#include <bctoolbox/port.h>
#include "ortp/ortp.h"
#include "utils.h"
#include "ortp/rtpsession.h"
#include "rtpsession_priv.h"

#if (_WIN32_WINNT >= 0x0600)
#include <delayimp.h>
#undef ExternC
#ifdef ORTP_WINDOWS_DESKTOP
#include <QOS2.h>
#include <VersionHelpers.h>
#endif
#endif

#if (defined(_WIN32) || defined(_WIN32_WCE)) && defined(ORTP_WINDOWS_DESKTOP)
#include <mswsock.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#define USE_SENDMSG 1
#endif

#define can_connect(s)	( (s)->use_connect && !(s)->symmetric_rtp)

#if defined(_WIN32) || defined(_WIN32_WCE)
#ifndef WSAID_WSARECVMSG
/* http://source.winehq.org/git/wine.git/blob/HEAD:/include/mswsock.h */
#define WSAID_WSARECVMSG {0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}
#ifndef MAX_NATURAL_ALIGNMENT
#define MAX_NATURAL_ALIGNMENT sizeof(DWORD)
#endif
#ifndef TYPE_ALIGNMENT
#define TYPE_ALIGNMENT(t) FIELD_OFFSET(struct { char x; t test; },test)
#endif
typedef WSACMSGHDR *LPWSACMSGHDR;
#ifndef WSA_CMSGHDR_ALIGN
#define WSA_CMSGHDR_ALIGN(length) (((length) + TYPE_ALIGNMENT(WSACMSGHDR)-1) & (~(TYPE_ALIGNMENT(WSACMSGHDR)-1)))
#endif
#ifndef WSA_CMSGDATA_ALIGN
#define WSA_CMSGDATA_ALIGN(length) (((length) + MAX_NATURAL_ALIGNMENT-1) & (~(MAX_NATURAL_ALIGNMENT-1)))
#endif
#ifndef WSA_CMSG_FIRSTHDR
#define WSA_CMSG_FIRSTHDR(msg) (((msg)->Control.len >= sizeof(WSACMSGHDR)) ? (LPWSACMSGHDR)(msg)->Control.buf : (LPWSACMSGHDR)NULL)
#endif
#ifndef WSA_CMSG_NXTHDR
#define WSA_CMSG_NXTHDR(msg,cmsg) ((!(cmsg)) ? WSA_CMSG_FIRSTHDR(msg) : ((((u_char *)(cmsg) + WSA_CMSGHDR_ALIGN((cmsg)->cmsg_len) + sizeof(WSACMSGHDR)) > (u_char *)((msg)->Control.buf) + (msg)->Control.len) ? (LPWSACMSGHDR)NULL : (LPWSACMSGHDR)((u_char *)(cmsg) + WSA_CMSGHDR_ALIGN((cmsg)->cmsg_len))))
#endif
#ifndef WSA_CMSG_DATA
#define WSA_CMSG_DATA(cmsg) ((u_char *)(cmsg) + WSA_CMSGDATA_ALIGN(sizeof(WSACMSGHDR)))
#endif
#endif
#undef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR WSA_CMSG_FIRSTHDR
#undef CMSG_NXTHDR
#define CMSG_NXTHDR WSA_CMSG_NXTHDR
#undef CMSG_DATA
#define CMSG_DATA WSA_CMSG_DATA
typedef INT  (WINAPI * LPFN_WSARECVMSG)(SOCKET, LPWSAMSG, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
static LPFN_WSARECVMSG ortp_WSARecvMsg = NULL;

#endif

#if defined(_WIN32) || defined(_WIN32_WCE) || defined(__QNX__)
/* Mingw32 does not define AI_V4MAPPED, however it is supported starting from Windows Vista. QNX also does not define AI_V4MAPPED. */
#	ifndef AI_V4MAPPED
#	define AI_V4MAPPED 0x00000800
#	endif
#	ifndef AI_ALL
#	define AI_ALL 0x00000100
#	endif
#	ifndef IPV6_V6ONLY
#	define IPV6_V6ONLY 27
#	endif
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(i)	(((uint8_t *) (i))[0] == 0xff)
#endif

static int
_rtp_session_set_remote_addr_full (RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port, bool_t is_aux);

static bool_t try_connect(ortp_socket_t fd, const struct sockaddr *dest, socklen_t addrlen){
	if (connect(fd,dest,addrlen)<0){
		ortp_warning("Could not connect() socket: %s",getSocketError());
		return FALSE;
	}
	return TRUE;
}

static int set_multicast_group(ortp_socket_t sock, const char *addr){
#ifndef __hpux
	struct addrinfo *res;
	int err;

	res = bctbx_name_to_addrinfo(AF_UNSPEC, SOCK_DGRAM, addr, 0);
	if (res == NULL) return -1;

	switch (res->ai_family){
		case AF_INET:
			if (IN_MULTICAST(ntohl(((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr)))
			{
				struct ip_mreq mreq;
				mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr;
				mreq.imr_interface.s_addr = INADDR_ANY;
				err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (SOCKET_OPTION_VALUE) &mreq, sizeof(mreq));
				if (err < 0){
					ortp_warning ("Fail to join address group: %s.", getSocketError());
				} else {
					ortp_message ("RTP socket [%i] has joined address group [%s]",sock, addr);
				}
			}
		break;
		case AF_INET6:
			if IN6_IS_ADDR_MULTICAST(&(((struct sockaddr_in6 *) res->ai_addr)->sin6_addr))
			{
				struct ipv6_mreq mreq;
				mreq.ipv6mr_multiaddr = ((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
				mreq.ipv6mr_interface = 0;
				err = setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (SOCKET_OPTION_VALUE)&mreq, sizeof(mreq));
				if (err < 0 ){
					ortp_warning ("Fail to join address group: %s.", getSocketError());
				} else {
					ortp_message ("RTP socket 6 [%i] has joined address group [%s]",sock, addr);
				}
			}
		break;
	}
	freeaddrinfo(res);
	return 0;
#else
	return -1;
#endif
}

static ortp_socket_t create_and_bind(const char *addr, int *port, int *sock_family, bool_t reuse_addr,struct sockaddr_storage* bound_addr,socklen_t *bound_addr_len){
	int err;
	int optval = 1;
	ortp_socket_t sock=-1;
	struct addrinfo *res0, *res;

	if (*port==-1) *port=0;
	if (*port==0) reuse_addr=FALSE;

	res0 = bctbx_name_to_addrinfo(AF_UNSPEC, SOCK_DGRAM, addr, *port);
	if (res0 == NULL) return -1;

	for (res = res0; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype, 0);
		if (sock==-1)
			continue;

		if (reuse_addr){
			err = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
					(SOCKET_OPTION_VALUE)&optval, sizeof (optval));
			if (err < 0)
			{
				ortp_warning ("Fail to set rtp address reusable: %s.", getSocketError());
			}
#ifdef SO_REUSEPORT
			/*SO_REUSEPORT is required on mac and ios especially for doing multicast*/
			err = setsockopt (sock, SOL_SOCKET, SO_REUSEPORT,
					(SOCKET_OPTION_VALUE)&optval, sizeof (optval));
			if (err < 0)
			{
				ortp_warning ("Fail to set rtp port reusable: %s.", getSocketError());
			}
#endif
		}
		/*enable dual stack operation, default is enabled on unix, disabled on windows.*/
		if (res->ai_family==AF_INET6){
			optval=0;
			err=setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&optval, sizeof(optval));
			if (err < 0){
				ortp_warning ("Fail to IPV6_V6ONLY: %s.",getSocketError());
			}
		}

#if defined(ORTP_TIMESTAMP)
		optval=1;
		err = setsockopt (sock, SOL_SOCKET, SO_TIMESTAMP,
			(SOCKET_OPTION_VALUE)&optval, sizeof (optval));
		if (err < 0)
		{
			ortp_warning ("Fail to set rtp timestamp: %s.",getSocketError());
		}
#endif
		err = 0;
		optval=1;
		switch (res->ai_family) {
			default:
			case AF_INET:
#ifdef IP_RECVTTL
				err = setsockopt(sock, IPPROTO_IP, IP_RECVTTL, &optval, sizeof(optval));
#endif
				break;
			case AF_INET6:
#ifdef IPV6_RECVHOPLIMIT
				err = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &optval, sizeof(optval));
#endif
				break;
		}
		if (err < 0) {
			ortp_warning("Fail to set recv TTL/HL socket option: %s.", getSocketError());
		}

		*sock_family=res->ai_family;
		err = bind(sock, res->ai_addr, (int)res->ai_addrlen);
		if (err != 0){
			ortp_error ("Fail to bind rtp socket to (addr=%s port=%i) : %s.", addr, *port, getSocketError());
			close_socket(sock);
			sock=-1;
			continue;
		}
		/*compatibility mode. New applications should use rtp_session_set_multicast_group() instead*/
		set_multicast_group(sock, addr);
		break;
	}
	memcpy(bound_addr,res0->ai_addr,res0->ai_addrlen);
	*bound_addr_len=(socklen_t)res0->ai_addrlen;

	bctbx_freeaddrinfo(res0);

#if defined(_WIN32) || defined(_WIN32_WCE)
	if (ortp_WSARecvMsg == NULL) {
		GUID guid = WSAID_WSARECVMSG;
		DWORD bytes_returned;
		if (WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
			&ortp_WSARecvMsg, sizeof(ortp_WSARecvMsg), &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
			ortp_warning("WSARecvMsg function not found.");
		}
	}
#endif
	if (sock!=-1){
		set_non_blocking_socket (sock);
		if (*port==0){
			struct sockaddr_storage saddr;
			socklen_t slen=sizeof(saddr);
			err=getsockname(sock,(struct sockaddr*)&saddr,&slen);
			if (err==-1){
				ortp_error("getsockname(): %s",getSocketError());
				close_socket(sock);
				return (ortp_socket_t)-1;
			}
			err = bctbx_sockaddr_to_ip_address((struct sockaddr *)&saddr, slen, NULL, 0, port);
			if (err!=0){
				close_socket(sock);
				return (ortp_socket_t)-1;
			}
		}

	}
	return sock;
}

void _rtp_session_apply_socket_sizes(RtpSession * session){
	int err;
	bool_t done=FALSE;
	ortp_socket_t sock = session->rtp.gs.socket;
	unsigned int sndbufsz = session->rtp.snd_socket_size;
	unsigned int rcvbufsz = session->rtp.rcv_socket_size;

	if (sock == (ortp_socket_t)-1) return;

	if (sndbufsz>0){
#ifdef SO_SNDBUFFORCE
		err = setsockopt(sock, SOL_SOCKET, SO_SNDBUFFORCE, (void *)&sndbufsz, sizeof(sndbufsz));
		if (err == -1) {
			ortp_warning("Fail to increase socket's send buffer size with SO_SNDBUFFORCE: %s.", getSocketError());
		}else done=TRUE;
#endif
		if (!done){
			err = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&sndbufsz, sizeof(sndbufsz));
			if (err == -1) {
				ortp_error("Fail to increase socket's send buffer size with SO_SNDBUF: %s.", getSocketError());
			}
		}
	}
	done=FALSE;
	if (rcvbufsz>0){
#ifdef SO_RCVBUFFORCE
		err = setsockopt(sock, SOL_SOCKET, SO_RCVBUFFORCE, (void *)&rcvbufsz, sizeof(rcvbufsz));
		if (err == -1) {
			ortp_warning("Fail to increase socket's recv buffer size with SO_RCVBUFFORCE: %s.", getSocketError());
		}
#endif
		if (!done){
			err = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbufsz, sizeof(rcvbufsz));
			if (err == -1) {
				ortp_error("Fail to increase socket's recv buffer size with SO_RCVBUF: %s.", getSocketError());
			}
		}

	}
}

/**
 *rtp_session_set_local_addr:
 *@param session:		a rtp session freshly created.
 *@param addr:		a local IP address in the xxx.xxx.xxx.xxx form.
 *@param rtp_port:		a local port or -1 to let oRTP choose the port randomly
 *@param rtcp_port:		a local port or -1 to let oRTP choose the port randomly
 *
 *	Specify the local addr to be use to listen for rtp packets or to send rtp packet from.
 *	In case where the rtp session is send-only, then it is not required to call this function:
 *	when calling rtp_session_set_remote_addr(), if no local address has been set, then the
 *	default INADRR_ANY (0.0.0.0) IP address with a random port will be used. Calling
 *	rtp_session_set_local_addr() is mandatory when the session is recv-only or duplex.
 *
 *	Returns: 0 on success.
**/

int
rtp_session_set_local_addr (RtpSession * session, const char * addr, int rtp_port, int rtcp_port)
{
	ortp_socket_t sock;
	int sockfamily;
	if (session->rtp.gs.socket!=(ortp_socket_t)-1){
		/* don't rebind, but close before*/
		_rtp_session_release_sockets(session, FALSE);
	}
	/* try to bind the rtp port */

	sock=create_and_bind(addr,&rtp_port,&sockfamily,session->reuseaddr,&session->rtp.gs.loc_addr,&session->rtp.gs.loc_addrlen);
	if (sock!=-1){
		session->rtp.gs.sockfamily=sockfamily;
		session->rtp.gs.socket=sock;
		session->rtp.gs.loc_port=rtp_port;
		_rtp_session_apply_socket_sizes(session);
		/*try to bind rtcp port */
		sock=create_and_bind(addr,&rtcp_port,&sockfamily,session->reuseaddr,&session->rtcp.gs.loc_addr,&session->rtcp.gs.loc_addrlen);
		if (sock!=(ortp_socket_t)-1){
			session->rtcp.gs.sockfamily=sockfamily;
			session->rtcp.gs.socket=sock;
			session->rtcp.gs.loc_port=rtcp_port;
		}else {
			ortp_error("Could not create and bind rtcp socket for session [%p]",session);
			return -1;
		}

		/* set socket options (but don't change chosen states) */
		rtp_session_set_dscp( session, -1 );
		rtp_session_set_multicast_ttl( session, -1 );
		rtp_session_set_multicast_loopback( session, -1 );
		if (session->use_pktinfo) rtp_session_set_pktinfo(session, TRUE);
		ortp_message("RtpSession bound to [%s] ports [%i] [%i]", addr, rtp_port, rtcp_port);
		return 0;
	}
	ortp_error("Could not bind RTP socket to %s on port %i for session [%p]",addr,rtp_port,session);
	return -1;
}

static void _rtp_session_recreate_sockets(RtpSession *session){
	char addr[NI_MAXHOST];
	int err = bctbx_sockaddr_to_ip_address((struct sockaddr *)&session->rtp.gs.loc_addr, session->rtp.gs.loc_addrlen, addr, sizeof(addr), NULL);
	if (err != 0) return;
	/*re create and bind sockets as they were done previously*/
	ortp_message("RtpSession %p is going to re-create its socket.", session);
	rtp_session_set_local_addr(session, addr, session->rtp.gs.loc_port, session->rtcp.gs.loc_port);
}

static void _rtp_session_check_socket_refresh(RtpSession *session){
	if (session->flags & RTP_SESSION_SOCKET_REFRESH_REQUESTED){
		session->flags &= ~RTP_SESSION_SOCKET_REFRESH_REQUESTED;
		_rtp_session_recreate_sockets(session);
	}
}

/**
 * Requests the session to re-create and bind its RTP and RTCP sockets same as they are currently.
 * This is used when a change in the routing rules of the host or process was made, in order to have
 * this routing rules change taking effect on the RTP/RTCP packets sent by the session.
**/
void rtp_session_refresh_sockets(RtpSession *session){
	if (session->rtp.gs.socket != (ortp_socket_t) -1){
		session->flags |= RTP_SESSION_SOCKET_REFRESH_REQUESTED;
	}
}

int rtp_session_join_multicast_group(RtpSession *session, const char *ip){
	int err;
	if (session->rtp.gs.socket==(ortp_socket_t)-1){
		ortp_error("rtp_session_set_multicast_group() must be done only on bound sockets, use rtp_session_set_local_addr() first");
		return -1;
	}
	err=set_multicast_group(session->rtp.gs.socket,ip);
	set_multicast_group(session->rtcp.gs.socket,ip);
	return err;
}

/**
 *rtp_session_set_pktinfo:
 *@param session: a rtp session
 *@param activate: activation flag (0 to deactivate, other value to activate)
 *
 * (De)activates packet info for incoming and outgoing packets.
 *
 * Returns: 0 on success.
 *
**/
int rtp_session_set_pktinfo(RtpSession *session, int activate)
{
	int retval;
	int optname;
#if defined(_WIN32) || defined(_WIN32_WCE)
	char optval[sizeof(DWORD)];
	int optlen = sizeof(optval);
#else
	int *optval = &activate;
	int optlen = sizeof(activate);
#endif
	session->use_pktinfo = activate;
	// Dont't do anything if socket hasn't been created yet
	if (session->rtp.gs.socket == (ortp_socket_t)-1) return 0;

#if defined(_WIN32) || defined(_WIN32_WCE)
	memset(optval, activate, sizeof(optval));
#endif

#ifdef IP_PKTINFO
	optname = IP_PKTINFO;
#else
	optname = IP_RECVDSTADDR;
#endif
	retval = setsockopt(session->rtp.gs.socket, IPPROTO_IP, optname, optval, optlen);
	if (retval < 0) {
		ortp_warning ("Fail to set IPv4 packet info on RTP socket: %s.", getSocketError());
	}
	retval = setsockopt(session->rtcp.gs.socket, IPPROTO_IP, optname, optval, optlen);
	if (retval < 0) {
		ortp_warning ("Fail to set IPv4 packet info on RTCP socket: %s.", getSocketError());
	}

	if (session->rtp.gs.sockfamily != AF_INET) {
#if defined(_WIN32) || defined(_WIN32_WCE)
		memset(optval, activate, sizeof(optval));
#endif

#ifdef IPV6_RECVPKTINFO
		optname = IPV6_RECVPKTINFO;
#else
		optname = IPV6_RECVDSTADDR;
#endif
		retval = setsockopt(session->rtp.gs.socket, IPPROTO_IPV6, optname, optval, optlen);
		if (retval < 0) {
			ortp_warning("Fail to set IPv6 packet info on RTP socket: %s.", getSocketError());
		}
		retval = setsockopt(session->rtcp.gs.socket, IPPROTO_IPV6, optname, optval, optlen);
		if (retval < 0) {
			ortp_warning("Fail to set IPv6 packet info on RTCP socket: %s.", getSocketError());
		}
	}

	return retval;
}


/**
 *rtp_session_set_multicast_ttl:
 *@param session: a rtp session
 *@param ttl: desired Multicast Time-To-Live
 *
 * Sets the TTL (Time-To-Live) for outgoing multicast packets.
 *
 * Returns: 0 on success.
 *
**/
int rtp_session_set_multicast_ttl(RtpSession *session, int ttl)
{
	int retval;

	// Store new TTL if one is specified
	if (ttl>0) session->multicast_ttl = ttl;

	// Don't do anything if socket hasn't been created yet
	if (session->rtp.gs.socket == (ortp_socket_t)-1) return 0;

	switch (session->rtp.gs.sockfamily) {
		case AF_INET: {

			retval= setsockopt(session->rtp.gs.socket, IPPROTO_IP, IP_MULTICAST_TTL,
						 (SOCKET_OPTION_VALUE)  &session->multicast_ttl, sizeof(session->multicast_ttl));

			if (retval<0) break;

			retval= setsockopt(session->rtcp.gs.socket, IPPROTO_IP, IP_MULTICAST_TTL,
					 (SOCKET_OPTION_VALUE)	   &session->multicast_ttl, sizeof(session->multicast_ttl));

		} break;
		case AF_INET6: {

			retval= setsockopt(session->rtp.gs.socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
					 (SOCKET_OPTION_VALUE)&session->multicast_ttl, sizeof(session->multicast_ttl));

			if (retval<0) break;

			retval= setsockopt(session->rtcp.gs.socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
					 (SOCKET_OPTION_VALUE) &session->multicast_ttl, sizeof(session->multicast_ttl));
		} break;
	default:
		retval=-1;
	}

	if (retval<0)
		ortp_warning("Failed to set multicast TTL on socket.");


	return retval;
}


/**
 *rtp_session_get_multicast_ttl:
 *@param session: a rtp session
 *
 * Returns the TTL (Time-To-Live) for outgoing multicast packets.
 *
**/
int rtp_session_get_multicast_ttl(RtpSession *session)
{
	return session->multicast_ttl;
}


/**
 *@param session: a rtp session
 *@param yesno: enable multicast loopback
 *
 * Enable multicast loopback.
 *
 * Returns: 0 on success.
 *
**/
int rtp_session_set_multicast_loopback(RtpSession *session, int yesno)
{
	int retval;

	// Store new loopback state if one is specified
	if (yesno==0) {
		// Don't loop back
		session->multicast_loopback = 0;
	} else if (yesno>0) {
		// Do loop back
		session->multicast_loopback = 1;
	}

	// Don't do anything if socket hasn't been created yet
	if (session->rtp.gs.socket == (ortp_socket_t)-1) return 0;

	switch (session->rtp.gs.sockfamily) {
		case AF_INET: {

			retval= setsockopt(session->rtp.gs.socket, IPPROTO_IP, IP_MULTICAST_LOOP,
						 (SOCKET_OPTION_VALUE)   &session->multicast_loopback, sizeof(session->multicast_loopback));

			if (retval<0) break;

			retval= setsockopt(session->rtcp.gs.socket, IPPROTO_IP, IP_MULTICAST_LOOP,
						 (SOCKET_OPTION_VALUE)   &session->multicast_loopback, sizeof(session->multicast_loopback));

		} break;
		case AF_INET6: {

			retval= setsockopt(session->rtp.gs.socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
				 (SOCKET_OPTION_VALUE)	&session->multicast_loopback, sizeof(session->multicast_loopback));

			if (retval<0) break;

			retval= setsockopt(session->rtcp.gs.socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
				 (SOCKET_OPTION_VALUE)	&session->multicast_loopback, sizeof(session->multicast_loopback));
		} break;
	default:
		retval=-1;
	}

	if (retval<0)
		ortp_warning("Failed to set multicast loopback on socket.");

	return retval;
}


/**
 *rtp_session_get_multicast_loopback:
 *@param session: a rtp session
 *
 * Returns the multicast loopback state of rtp session (true or false).
 *
**/
int rtp_session_get_multicast_loopback(RtpSession *session)
{
	return session->multicast_loopback;
}

/**
 *rtp_session_set_dscp:
 *@param session: a rtp session
 *@param dscp: desired DSCP PHB value
 *
 * Sets the DSCP (Differentiated Services Code Point) for outgoing RTP packets.
 *
 * Returns: 0 on success.
 *
**/
int rtp_session_set_dscp(RtpSession *session, int dscp){
	int retval=0;
	int tos;
	int proto;
	int value_type;

	// Store new DSCP value if one is specified
	if (dscp>=0) session->dscp = dscp;

	// Don't do anything if socket hasn't been created yet
	if (session->rtp.gs.socket == (ortp_socket_t)-1) return 0;

#if (_WIN32_WINNT >= 0x0600) && defined(ORTP_WINDOWS_DESKTOP)
	ortp_message("check OS support for qwave.lib");
	if (IsWindowsVistaOrGreater()) {
		if (session->dscp==0)
			tos=QOSTrafficTypeBestEffort;
		else if (session->dscp==0x8)
			tos=QOSTrafficTypeBackground;
		else if (session->dscp==0x28)
			tos=QOSTrafficTypeAudioVideo;
		else if (session->dscp==0x38)
			tos=QOSTrafficTypeVoice;
		else
			tos=QOSTrafficTypeExcellentEffort; /* 0x28 */

		if (session->rtp.QoSHandle==NULL) {
			QOS_VERSION version;
			BOOL QoSResult;

			version.MajorVersion = 1;
			version.MinorVersion = 0;

			QoSResult = QOSCreateHandle(&version, &session->rtp.QoSHandle);

			if (QoSResult != TRUE){
				ortp_error("QOSCreateHandle failed to create handle with error %d", GetLastError());
				retval=-1;
			}
		}
		if (session->rtp.QoSHandle!=NULL) {
			BOOL QoSResult;
			QoSResult = QOSAddSocketToFlow(
				session->rtp.QoSHandle,
				session->rtp.gs.socket,
				(struct sockaddr*)&session->rtp.gs.rem_addr,
				tos,
				QOS_NON_ADAPTIVE_FLOW,
				&session->rtp.QoSFlowID);

			if (QoSResult != TRUE){
				ortp_error("QOSAddSocketToFlow failed to add a flow with error %d", GetLastError());
				retval=-1;
			}
		}
	} else {
#endif
		// DSCP value is in the upper six bits of the TOS field
		tos = (session->dscp << 2) & 0xFC;
		switch (session->rtp.gs.sockfamily) {
			case AF_INET:
				proto=IPPROTO_IP;
				value_type=IP_TOS;
			break;
		case AF_INET6:
			proto=IPPROTO_IPV6;
#	ifdef IPV6_TCLASS /*seems not defined by my libc*/
			value_type=IPV6_TCLASS;
#	else
			value_type=IP_TOS;
#	endif
			break;
		default:
			ortp_error("Cannot set DSCP because socket family is unspecified.");
			return -1;
		}
		retval = setsockopt(session->rtp.gs.socket, proto, value_type, (SOCKET_OPTION_VALUE)&tos, sizeof(tos));
		if (retval==-1)
			ortp_error("Fail to set DSCP value on rtp socket: %s",getSocketError());
		if (session->rtcp.gs.socket != (ortp_socket_t)-1){
			if (setsockopt(session->rtcp.gs.socket, proto, value_type, (SOCKET_OPTION_VALUE)&tos, sizeof(tos))==-1){
				ortp_error("Fail to set DSCP value on rtcp socket: %s",getSocketError());
			}
		}
#if (_WIN32_WINNT >= 0x0600) && defined(ORTP_WINDOWS_DESKTOP)
	}
#endif
	return retval;
}


/**
 *rtp_session_get_dscp:
 *@param session: a rtp session
 *
 * Returns the DSCP (Differentiated Services Code Point) for outgoing RTP packets.
 *
**/
int rtp_session_get_dscp(const RtpSession *session)
{
	return session->dscp;
}


/**
 *rtp_session_get_local_port:
 *@param session:	a rtp session for which rtp_session_set_local_addr() or rtp_session_set_remote_addr() has been called
 *
 *	This function can be useful to retrieve the local port that was randomly choosen by
 *	rtp_session_set_remote_addr() when rtp_session_set_local_addr() was not called.
 *
 *	Returns: the local port used to listen for rtp packets, -1 if not set.
**/

int rtp_session_get_local_port(const RtpSession *session){
	return (session->rtp.gs.loc_port>0) ? session->rtp.gs.loc_port : -1;
}

int rtp_session_get_local_rtcp_port(const RtpSession *session){
	return (session->rtcp.gs.loc_port>0) ? session->rtcp.gs.loc_port : -1;
}

/**
 *rtp_session_set_remote_addr:
 *@param session:		a rtp session freshly created.
 *@param addr:		a remote IP address in the xxx.xxx.xxx.xxx form.
 *@param port:		a remote port.
 *
 *	Sets the remote address of the rtp session, ie the destination address where rtp packet
 *	are sent. If the session is recv-only or duplex, it also sets the origin of incoming RTP
 *	packets. Rtp packets that don't come from addr:port are discarded.
 *
 *	Returns: 0 on success.
**/
int
rtp_session_set_remote_addr (RtpSession * session, const char * addr, int port){
	return rtp_session_set_remote_addr_full(session, addr, port, addr, port+1);
}

/**
 *rtp_session_set_remote_addr_full:
 *@param session:		a rtp session freshly created.
 *@param rtp_addr:		a remote IP address in the xxx.xxx.xxx.xxx form.
 *@param rtp_port:		a remote rtp port.
 *@param rtcp_addr:		a remote IP address in the xxx.xxx.xxx.xxx form.
 *@param rtcp_port:		a remote rtcp port.
 *
 *	Sets the remote address of the rtp session, ie the destination address where rtp packet
 *	are sent. If the session is recv-only or duplex, it also sets the origin of incoming RTP
 *	packets. Rtp packets that don't come from addr:port are discarded.
 *
 *	Returns: 0 on success.
**/

int
rtp_session_set_remote_addr_full (RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port){
	return _rtp_session_set_remote_addr_full(session,rtp_addr,rtp_port,rtcp_addr,rtcp_port,FALSE);
}

static int
_rtp_session_set_remote_addr_full (RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port, bool_t is_aux){
	char rtp_printable_addr[64];
	char rtcp_printable_addr[64];
	int err;
	struct addrinfo *res0, *res;
	struct sockaddr_storage *rtp_saddr=&session->rtp.gs.rem_addr;
	socklen_t *rtp_saddr_len=&session->rtp.gs.rem_addrlen;
	struct sockaddr_storage *rtcp_saddr=&session->rtcp.gs.rem_addr;
	socklen_t *rtcp_saddr_len=&session->rtcp.gs.rem_addrlen;
	OrtpAddress *aux_rtp=NULL,*aux_rtcp=NULL;

	if (is_aux){
		aux_rtp=ortp_malloc0(sizeof(OrtpAddress));
		rtp_saddr=&aux_rtp->addr;
		rtp_saddr_len=&aux_rtp->len;
		aux_rtcp=ortp_malloc0(sizeof(OrtpAddress));
		rtcp_saddr=&aux_rtcp->addr;
		rtcp_saddr_len=&aux_rtcp->len;
	}

	res0 = bctbx_name_to_addrinfo((session->rtp.gs.socket == -1) ? AF_UNSPEC : session->rtp.gs.sockfamily, SOCK_DGRAM, rtp_addr, rtp_port);
	if (res0 == NULL) {
		ortp_error("_rtp_session_set_remote_addr_full(): cannot set RTP destination to %s port %i.", rtp_addr, rtp_port);
		err=-1;
		goto end;
	} else {
		bctbx_addrinfo_to_printable_ip_address(res0, rtp_printable_addr, sizeof(rtp_printable_addr));
	}
	if (session->rtp.gs.socket == -1){
		/* the session has not its socket bound, do it */
		ortp_message ("Setting random local addresses.");
		/* bind to an address type that matches the destination address */
		if (res0->ai_addr->sa_family==AF_INET6)
			err = rtp_session_set_local_addr (session, "::", -1, -1);
		else err=rtp_session_set_local_addr (session, "0.0.0.0", -1, -1);
		if (err<0) {
			err=-1;
			goto end;
		}
	}

	err=-1;
	for (res = res0; res; res = res->ai_next) {
		/* set a destination address that has the same type as the local address */
		if (res->ai_family==session->rtp.gs.sockfamily ) {
			memcpy(rtp_saddr, res->ai_addr, res->ai_addrlen);
			*rtp_saddr_len=(socklen_t)res->ai_addrlen;
			err=0;
			break;
		}
	}
	bctbx_freeaddrinfo(res0);
	if (err) {
		ortp_warning("Could not set destination for RTP socket to %s:%i.",rtp_addr,rtp_port);
		goto end;
	}

	if ((rtcp_addr != NULL) && (rtcp_port > 0)) {
		res0 = bctbx_name_to_addrinfo((session->rtcp.gs.socket == -1) ? AF_UNSPEC : session->rtcp.gs.sockfamily, SOCK_DGRAM, rtcp_addr, rtcp_port);
		if (res0 == NULL) {
			ortp_error("_rtp_session_set_remote_addr_full(): cannot set RTCP destination to %s port %i.", rtcp_addr, rtcp_port);
			err=-1;
			goto end;
		} else {
			bctbx_addrinfo_to_printable_ip_address(res0, rtcp_printable_addr, sizeof(rtcp_printable_addr));
		}
		err=-1;
		for (res = res0; res; res = res->ai_next) {
			/* set a destination address that has the same type as the local address */
			if (res->ai_family==session->rtcp.gs.sockfamily ) {
				err=0;
				memcpy(rtcp_saddr, res->ai_addr, res->ai_addrlen);
				*rtcp_saddr_len=(socklen_t)res->ai_addrlen;
				break;
			}
		}
		bctbx_freeaddrinfo(res0);
		if (err) {
			ortp_warning("Could not set destination for RCTP socket to %s:%i.",rtcp_addr,rtcp_port);
			goto end;
		}

		if (can_connect(session)){
			if (try_connect(session->rtp.gs.socket,(struct sockaddr*)&session->rtp.gs.rem_addr,session->rtp.gs.rem_addrlen))
				session->flags|=RTP_SOCKET_CONNECTED;
			if (session->rtcp.gs.socket!=(ortp_socket_t)-1){
				if (try_connect(session->rtcp.gs.socket,(struct sockaddr*)&session->rtcp.gs.rem_addr,session->rtcp.gs.rem_addrlen))
					session->flags|=RTCP_SOCKET_CONNECTED;
			}
		}else if (session->flags & RTP_SOCKET_CONNECTED){
			/*must dissolve association done by connect().
			See connect(2) manpage*/
			struct sockaddr sa;
			sa.sa_family=AF_UNSPEC;
			if (connect(session->rtp.gs.socket,&sa,sizeof(sa))<0){
				ortp_error("Cannot dissolve connect() association for rtp socket: %s", getSocketError());
			}
			if (connect(session->rtcp.gs.socket,&sa,sizeof(sa))<0){
				ortp_error("Cannot dissolve connect() association for rtcp socket: %s", getSocketError());
			}
			session->flags&=~RTP_SOCKET_CONNECTED;
			session->flags&=~RTCP_SOCKET_CONNECTED;
		}

		ortp_message("RtpSession [%p] sending to rtp %s rtcp %s %s", session, rtp_printable_addr, rtcp_printable_addr, is_aux ? "as auxiliary destination" : "");
	} else {
		ortp_message("RtpSession [%p] sending to rtp %s %s", session, rtp_printable_addr, is_aux ? "as auxiliary destination" : "");
	}

end:
	if (is_aux){
		if (err==-1){
			ortp_free(aux_rtp);
			ortp_free(aux_rtcp);
		}else{
			session->rtp.gs.aux_destinations=o_list_append(session->rtp.gs.aux_destinations,aux_rtp);
			session->rtcp.gs.aux_destinations=o_list_append(session->rtcp.gs.aux_destinations,aux_rtcp);
		}
	}
	return err;
}

int rtp_session_set_remote_addr_and_port(RtpSession * session, const char * addr, int rtp_port, int rtcp_port){
	return rtp_session_set_remote_addr_full(session,addr,rtp_port,addr,rtcp_port);
}

/**
 *rtp_session_add_remote_aux_addr_full:
 *@param session:		a rtp session freshly created.
 *@param rtp_addr:		a local IP address in the xxx.xxx.xxx.xxx form.
 *@param rtp_port:		a local rtp port.
 *@param rtcp_addr:		a local IP address in the xxx.xxx.xxx.xxx form.
 *@param rtcp_port:		a local rtcp port.
 *
 *	Add an auxiliary remote address for the rtp session, ie a destination address where rtp packet
 *	are sent.
 *
 *	Returns: 0 on success.
**/

int
rtp_session_add_aux_remote_addr_full(RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port){
	return _rtp_session_set_remote_addr_full(session,rtp_addr,rtp_port,rtcp_addr,rtcp_port,TRUE);
}

void rtp_session_clear_aux_remote_addr(RtpSession * session){
	ortp_stream_clear_aux_addresses(&session->rtp.gs);
	ortp_stream_clear_aux_addresses(&session->rtcp.gs);
}

void rtp_session_set_sockets(RtpSession *session, int rtpfd, int rtcpfd){
	if (rtpfd!=-1) set_non_blocking_socket(rtpfd);
	if (rtcpfd!=-1) set_non_blocking_socket(rtcpfd);
	session->rtp.gs.socket=rtpfd;
	session->rtcp.gs.socket=rtcpfd;
	if (rtpfd!=-1 || rtcpfd!=-1 )
		session->flags|=(RTP_SESSION_USING_EXT_SOCKETS|RTP_SOCKET_CONNECTED|RTCP_SOCKET_CONNECTED);
	else session->flags&=~(RTP_SESSION_USING_EXT_SOCKETS|RTP_SOCKET_CONNECTED|RTCP_SOCKET_CONNECTED);
}

void rtp_session_set_transports(RtpSession *session, struct _RtpTransport *rtptr, struct _RtpTransport *rtcptr)
{
	session->rtp.gs.tr = rtptr;
	session->rtcp.gs.tr = rtcptr;
	if (rtptr)
		rtptr->session=session;
	if (rtcptr)
		rtcptr->session=session;

	if (rtptr || rtcptr )
		session->flags|=(RTP_SESSION_USING_TRANSPORT);
	else session->flags&=~(RTP_SESSION_USING_TRANSPORT);
}

void rtp_session_get_transports(const RtpSession *session, RtpTransport **rtptr, RtpTransport **rtcptr){
	if (rtptr) *rtptr=session->rtp.gs.tr;
	if (rtcptr) *rtcptr=session->rtcp.gs.tr;
}


/**
 *rtp_session_flush_sockets:
 *@param session: a rtp session
 *
 * Flushes the sockets for all pending incoming packets.
 * This can be usefull if you did not listen to the stream for a while
 * and wishes to start to receive again. During the time no receive is made
 * packets get bufferised into the internal kernel socket structure.
 *
**/
void rtp_session_flush_sockets(RtpSession *session){
	rtp_session_set_flag(session, RTP_SESSION_FLUSH);
	rtp_session_rtp_recv(session, 0);
	rtp_session_unset_flag(session, RTP_SESSION_FLUSH);
}


#ifdef USE_SENDMSG
#define MAX_IOV 30
static int rtp_sendmsg(int sock,mblk_t *m, const struct sockaddr *rem_addr, socklen_t addr_len){
	int error;
	struct msghdr msg;
	struct iovec iov[MAX_IOV];
	int iovlen;
	for(iovlen=0; iovlen<MAX_IOV && m!=NULL; m=m->b_cont,iovlen++){
		iov[iovlen].iov_base=m->b_rptr;
		iov[iovlen].iov_len=m->b_wptr-m->b_rptr;
	}
	if (iovlen==MAX_IOV){
		ortp_error("Too long msgb, didn't fit into iov, end discarded.");
	}
	msg.msg_name=(void*)rem_addr;
	msg.msg_namelen=addr_len;
	msg.msg_iov=&iov[0];
	msg.msg_iovlen=iovlen;
	msg.msg_control=NULL;
	msg.msg_controllen=0;
	msg.msg_flags=0;
	error=sendmsg(sock,&msg,0);
	return error;
}
#endif

ortp_socket_t rtp_session_get_socket(RtpSession *session, bool_t is_rtp){
	return is_rtp ? session->rtp.gs.socket : session->rtcp.gs.socket;
}

int _ortp_sendto(ortp_socket_t sockfd, mblk_t *m, int flags, const struct sockaddr *destaddr, socklen_t destlen){
	int error;
#ifdef USE_SENDMSG
	error=rtp_sendmsg(sockfd,m,destaddr,destlen);
#else
	if (m->b_cont!=NULL)
		msgpullup(m,-1);
	error = sendto (sockfd, (char*)m->b_rptr, (int) (m->b_wptr - m->b_rptr),
		0,destaddr,destlen);
#endif
	return error;
}

int rtp_session_sendto(RtpSession *session, bool_t is_rtp, mblk_t *m, int flags, const struct sockaddr *destaddr, socklen_t destlen){
	int ret;

	_rtp_session_check_socket_refresh(session);

	if (session->net_sim_ctx && (session->net_sim_ctx->params.mode==OrtpNetworkSimulatorOutbound
			|| session->net_sim_ctx->params.mode==OrtpNetworkSimulatorOutboundControlled)){
		ret=(int)msgdsize(m);
		m=dupmsg(m);
		memcpy(&m->net_addr,destaddr,destlen);
		m->net_addrlen=destlen;
		m->reserved1=is_rtp;
		ortp_mutex_lock(&session->net_sim_ctx->mutex);
		putq(&session->net_sim_ctx->send_q, m);
		ortp_mutex_unlock(&session->net_sim_ctx->mutex);
	}else{
		ortp_socket_t sockfd = rtp_session_get_socket(session, is_rtp);
		ret=_ortp_sendto(sockfd, m, flags, destaddr, destlen);
	}
	return ret;
}

static const ortp_recv_addr_t * lookup_recv_addr(RtpSession *session, struct sockaddr *from, socklen_t fromlen) {
	const ortp_recv_addr_t *result = NULL;
	bctbx_list_t *iterator = session->recv_addr_map;
	while (iterator != NULL) {
		ortp_recv_addr_map_t *item = (ortp_recv_addr_map_t *)bctbx_list_get_data(iterator);
		uint64_t curtime = ortp_get_cur_time_ms();
		if ((curtime - item->ts) > 2000) {
			bctbx_list_t *to_remove = iterator;
			iterator = bctbx_list_next(iterator);
			session->recv_addr_map = bctbx_list_erase_link(session->recv_addr_map, to_remove);
		} else {
			if (memcmp(&item->ss, from, fromlen) == 0) result = &item->recv_addr;
			iterator = bctbx_list_next(iterator);
		}
	}
	return result;
}

static const ortp_recv_addr_t * get_recv_addr(RtpSession *session, struct sockaddr *from, socklen_t fromlen) {
	char result[NI_MAXHOST] = { 0 };
	char dest[NI_MAXHOST] = { 0 };
	struct addrinfo *ai = NULL;
	int port = 0;
	int family = from->sa_family;
	int err;
	err = bctbx_sockaddr_to_ip_address(from, fromlen, dest, sizeof(dest), &port);
	if (err != 0) {
		ortp_error("bctbx_sockaddr_to_ip_address failed");
		return NULL;
	}
	err = bctbx_get_local_ip_for(family, dest, port, result, sizeof(result));
	if (err != 0) {
		ortp_error("bctbx_get_local_ip_for failed: dest=%s, port=%d", dest, port);
		return NULL;
	}
	ai = bctbx_ip_address_to_addrinfo(family, SOCK_DGRAM, result, port);
	if (ai == NULL) {
		ortp_error("bctbx_ip_address_to_addrinfo failed: result=%s, port=%d", result, port);
		return NULL;
	} else {
		ortp_recv_addr_map_t *item = bctbx_new0(ortp_recv_addr_map_t, 1);
		memcpy(&item->ss, from, fromlen);
		item->recv_addr.family = family;
		if (family == AF_INET) {
			memcpy(&item->recv_addr.addr.ipi_addr, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, sizeof(item->recv_addr.addr.ipi_addr));
		} else if (family == AF_INET6) {
			memcpy(&item->recv_addr.addr.ipi6_addr, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr, sizeof(item->recv_addr.addr.ipi6_addr));
		}
		bctbx_freeaddrinfo(ai);
		item->ts = ortp_get_cur_time_ms();
		session->recv_addr_map = bctbx_list_append(session->recv_addr_map, item);
		return &item->recv_addr;
	}
}

int rtp_session_recvfrom(RtpSession *session, bool_t is_rtp, mblk_t *m, int flags, struct sockaddr *from, socklen_t *fromlen) {
	int ret = rtp_session_rtp_recv_abstract(is_rtp ? session->rtp.gs.socket : session->rtcp.gs.socket, m, flags, from, fromlen);
	if ((ret >= 0) && (session->use_pktinfo == TRUE)) {
		if (m->recv_addr.family == AF_UNSPEC) {
			/* The receive address has not been filled, this typically happens on Mac OS X when receiving an IPv4 packet on
			 * a dual stack socket. Try to guess the address because it is mandatory for ICE. */
			const ortp_recv_addr_t *recv_addr = lookup_recv_addr(session, from, *fromlen);
			if (recv_addr == NULL) {
				recv_addr = get_recv_addr(session, from, *fromlen);
			}
			if (recv_addr != NULL) {
				memcpy(&m->recv_addr, recv_addr, sizeof(ortp_recv_addr_t));
			} else {
				ortp_error("Did not succeed to fill the receive address, this should not happen! [family=%d, len=%d]", from->sa_family, (int)*fromlen);
			}
		}
		/* Store the local port in the recv_addr of the mblk_t, the address is already filled in rtp_session_rtp_recv_abstract */
		m->recv_addr.port = htons(is_rtp ? session->rtp.gs.loc_port : session->rtcp.gs.loc_port);
	}
	return ret;
}

void update_sent_bytes(OrtpStream *os, int nbytes) {
	int overhead = ortp_stream_is_ipv6(os) ? IP6_UDP_OVERHEAD : IP_UDP_OVERHEAD;
	if ((os->sent_bytes == 0) && (os->send_bw_start.tv_sec == 0) && (os->send_bw_start.tv_usec == 0)) {
		/* Initialize bandwidth computing time when has not been started yet. */
		ortp_gettimeofday(&os->send_bw_start, NULL);
	}
	os->sent_bytes += nbytes + overhead;
}

static void update_recv_bytes(OrtpStream *os, int nbytes) {
	int overhead = ortp_stream_is_ipv6(os) ? IP6_UDP_OVERHEAD : IP_UDP_OVERHEAD;
	if ((os->recv_bytes == 0) && (os->recv_bw_start.tv_sec == 0) && (os->recv_bw_start.tv_usec == 0)) {
		ortp_gettimeofday(&os->recv_bw_start, NULL);
	}
	os->recv_bytes += nbytes + overhead;
}

static void log_send_error(RtpSession *session, const char *type, mblk_t *m, struct sockaddr *destaddr, socklen_t destlen){
	char printable_ip_address[65]={0};
	int errnum = getSocketErrorCode();
	const char *errstr = getSocketError();
	bctbx_sockaddr_to_printable_ip_address(destaddr, destlen, printable_ip_address, sizeof(printable_ip_address));
	ortp_error ("RtpSession [%p] error sending [%s] packet [%p] to %s: %s [%d]",
		session, type, m, printable_ip_address, errstr, errnum);
}

static int rtp_session_rtp_sendto(RtpSession * session, mblk_t * m, struct sockaddr *destaddr, socklen_t destlen, bool_t is_aux){
	int error;

	if (rtp_session_using_transport(session, rtp)){
		error = (session->rtp.gs.tr->t_sendto) (session->rtp.gs.tr,m,0,destaddr,destlen);
	}else{
		error=rtp_session_sendto(session, TRUE,m,0,destaddr,destlen);
	}
	if (!is_aux){
		/*errors to auxiliary destinations are not notified*/
		if (error < 0){
			if (session->on_network_error.count>0){
				rtp_signal_table_emit3(&session->on_network_error,"Error sending RTP packet",ORTP_INT_TO_POINTER(getSocketErrorCode()));
			}else log_send_error(session,"rtp",m,destaddr,destlen);
			session->rtp.send_errno=getSocketErrorCode();
		}else{
			update_sent_bytes(&session->rtp.gs, error);
		}
	}
	return error;
}

int rtp_session_rtp_send (RtpSession * session, mblk_t * m){
	int error=0;
	int i;
	rtp_header_t *hdr;
	struct sockaddr *destaddr=(struct sockaddr*)&session->rtp.gs.rem_addr;
	socklen_t destlen=session->rtp.gs.rem_addrlen;
	OList *elem=NULL;

	if (session->is_spliced) {
		freemsg(m);
		return 0;
	}

	hdr = (rtp_header_t *) m->b_rptr;
	if (hdr->version == 0) {
		/* We are probably trying to send a STUN packet so don't change its content. */
	} else {
		/* perform host to network conversions */
		hdr->ssrc = htonl (hdr->ssrc);
		hdr->timestamp = htonl (hdr->timestamp);
		hdr->seq_number = htons (hdr->seq_number);
		for (i = 0; i < hdr->cc; i++)
			hdr->csrc[i] = htonl (hdr->csrc[i]);
	}

	if (session->flags & RTP_SOCKET_CONNECTED) {
		destaddr=NULL;
		destlen=0;
	}
	/*first send to main destination*/
	error=rtp_session_rtp_sendto(session,m,destaddr,destlen,FALSE);
	/*then iterate over auxiliary destinations*/
	for(elem=session->rtp.gs.aux_destinations;elem!=NULL;elem=elem->next){
		OrtpAddress *addr=(OrtpAddress*)elem->data;
		rtp_session_rtp_sendto(session,m,(struct sockaddr*)&addr->addr,addr->len,TRUE);
	}
	freemsg(m);
	return error;
}

static int rtp_session_rtcp_sendto(RtpSession * session, mblk_t * m, struct sockaddr *destaddr, socklen_t destlen, bool_t is_aux){
	int error=0;

	/* Even in RTCP mux, we send through the RTCP RtpTransport, which will itself take in charge to do the sending of the packet
	 * through the RTP endpoint*/
	if (rtp_session_using_transport(session, rtcp)){
		error = (session->rtcp.gs.tr->t_sendto) (session->rtcp.gs.tr, m, 0, destaddr, destlen);
	}else{
		error=_ortp_sendto(rtp_session_get_socket(session, session->rtcp_mux),m,0,destaddr,destlen);
	}

	if (!is_aux){
		if (error < 0){
			if (session->on_network_error.count>0){
				rtp_signal_table_emit3(&session->on_network_error,"Error sending RTCP packet",ORTP_INT_TO_POINTER(getSocketErrorCode()));
			}else{
				log_send_error(session,"rtcp",m,destaddr,destlen);
			}
		} else {
			update_sent_bytes(&session->rtcp.gs, error);
			update_avg_rtcp_size(session, error);
		}
	}
	return error;
}

int
rtp_session_rtcp_send (RtpSession * session, mblk_t * m){
	int error=0;
	ortp_socket_t sockfd=session->rtcp.gs.socket;
	struct sockaddr *destaddr=session->rtcp_mux ? (struct sockaddr*)&session->rtp.gs.rem_addr : (struct sockaddr*)&session->rtcp.gs.rem_addr;
	socklen_t destlen=session->rtcp_mux ? session->rtp.gs.rem_addrlen : session->rtcp.gs.rem_addrlen;
	OList *elem=NULL;
	bool_t using_connected_socket=(session->flags & RTCP_SOCKET_CONNECTED)!=0;

	if (session->is_spliced) {
		freemsg(m);
		return 0;
	}
	if (using_connected_socket) {
		destaddr=NULL;
		destlen=0;
	}

	if (session->rtcp.enabled){
		if ( (sockfd!=(ortp_socket_t)-1 && (destlen>0 || using_connected_socket))
			|| rtp_session_using_transport(session, rtcp) ) {
			rtp_session_rtcp_sendto(session,m,destaddr,destlen,FALSE);
		}
		for(elem=session->rtcp.gs.aux_destinations;elem!=NULL;elem=elem->next){
			OrtpAddress *addr=(OrtpAddress*)elem->data;
			rtp_session_rtcp_sendto(session,m,(struct sockaddr*)&addr->addr,addr->len,TRUE);
		}
	}else ortp_message("Not sending rtcp report, rtcp disabled.");
	freemsg(m);
	return error;
}

int rtp_session_rtp_recv_abstract(ortp_socket_t socket, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) {
	int ret;
	int bufsz = (int) (msg->b_datap->db_lim - msg->b_datap->db_base);
#ifndef _WIN32
	struct iovec   iov;
	struct msghdr  msghdr;
	struct cmsghdr *cmsghdr;
	char control[512];
	memset(&msghdr, 0, sizeof(msghdr));
	memset(&iov, 0, sizeof(iov));
	iov.iov_base = msg->b_wptr;
	iov.iov_len  = bufsz;
	if(from != NULL && fromlen != NULL) {
		msghdr.msg_name = from;
		msghdr.msg_namelen = *fromlen;
	}
	msghdr.msg_iov     = &iov;
	msghdr.msg_iovlen  = 1;
	msghdr.msg_control = &control;
	msghdr.msg_controllen = sizeof(control);
	ret = recvmsg(socket, &msghdr, flags);
	if(fromlen != NULL)
		*fromlen = msghdr.msg_namelen;
	if(ret >= 0) {
#else
	char control[512];
	WSAMSG msghdr;
	WSACMSGHDR *cmsghdr;
	WSABUF data_buf;
	DWORD bytes_received;

	if (ortp_WSARecvMsg == NULL) {
		return recvfrom(socket, (char *)msg->b_wptr, bufsz, flags, from, fromlen);
	}

	memset(&msghdr, 0, sizeof(msghdr));
	memset(control, 0, sizeof(control));
	if(from != NULL && fromlen != NULL) {
		msghdr.name = from;
		msghdr.namelen = *fromlen;
	}
	data_buf.buf = (char *)msg->b_wptr;
	data_buf.len = bufsz;
	msghdr.lpBuffers = &data_buf;
	msghdr.dwBufferCount = 1;
	msghdr.Control.buf = control;
	msghdr.Control.len = sizeof(control);
	msghdr.dwFlags = flags;
	ret = ortp_WSARecvMsg(socket, &msghdr, &bytes_received, NULL, NULL);
	if(fromlen != NULL)
		*fromlen = msghdr.namelen;
	if(ret >= 0) {
		ret = bytes_received;
#endif
		for (cmsghdr = CMSG_FIRSTHDR(&msghdr); cmsghdr != NULL ; cmsghdr = CMSG_NXTHDR(&msghdr, cmsghdr)) {
#if defined(ORTP_TIMESTAMP)
			if (cmsghdr->cmsg_level == SOL_SOCKET && cmsghdr->cmsg_type == SO_TIMESTAMP) {
				memcpy(&msg->timestamp, (struct timeval *)CMSG_DATA(cmsghdr), sizeof(struct timeval));
			}
#endif
#ifdef IP_PKTINFO
			if ((cmsghdr->cmsg_level == IPPROTO_IP) && (cmsghdr->cmsg_type == IP_PKTINFO)) {
				struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsghdr);
				memcpy(&msg->recv_addr.addr.ipi_addr, &pi->ipi_addr, sizeof(msg->recv_addr.addr.ipi_addr));
				msg->recv_addr.family = AF_INET;
			}
#endif
#ifdef IPV6_PKTINFO
			if ((cmsghdr->cmsg_level == IPPROTO_IPV6) && (cmsghdr->cmsg_type == IPV6_PKTINFO)) {
				struct in6_pktinfo *pi = (struct in6_pktinfo *)CMSG_DATA(cmsghdr);
				memcpy(&msg->recv_addr.addr.ipi6_addr, &pi->ipi6_addr, sizeof(msg->recv_addr.addr.ipi6_addr));
				msg->recv_addr.family = AF_INET6;
			}
#endif
#ifdef IP_RECVDSTADDR
			if ((cmsghdr->cmsg_level == IPPROTO_IP) && (cmsghdr->cmsg_type == IP_RECVDSTADDR)) {
				struct in_addr *ia = (struct in_addr *)CMSG_DATA(cmsghdr);
				memcpy(&msg->recv_addr.addr.ipi_addr, ia, sizeof(msg->recv_addr.addr.ipi_addr));
				msg->recv_addr.family = AF_INET;
			}
#endif
#ifdef IPV6_RECVDSTADDR
			if ((cmsghdr->cmsg_level == IPPROTO_IPV6) && (cmsghdr->cmsg_type == IPV6_RECVDSTADDR)) {
				struct in6_addr *ia = (struct in6_addr *)CMSG_DATA(cmsghdr);
				memcpy(&msg->recv_addr.addr.ipi6_addr, ia, sizeof(msg->recv_addr.addr.ipi6_addr));
				msg->recv_addr.family = AF_INET6;
			}
#endif
#ifdef IP_RECVTTL
			if ((cmsghdr->cmsg_level == IPPROTO_IP) && (cmsghdr->cmsg_type == IP_TTL)) {
				uint32_t *ptr = (uint32_t *)CMSG_DATA(cmsghdr);
				msg->ttl_or_hl = (*ptr & 0xFF);
			}
#endif
#ifdef IPV6_RECVHOPLIMIT
			if ((cmsghdr->cmsg_level == IPPROTO_IPV6) && (cmsghdr->cmsg_type == IPV6_HOPLIMIT)) {
				uint32_t *ptr = (uint32_t *)CMSG_DATA(cmsghdr);
				msg->ttl_or_hl = (*ptr & 0xFF);
			}
#endif
		}
		/*store recv addr for use by modifiers*/
		if (from && fromlen) {
			memcpy(&msg->net_addr,from,*fromlen);
			msg->net_addrlen = *fromlen;
		}
	}
	return ret;
}

void rtp_session_notify_inc_rtcp(RtpSession *session, mblk_t *m, bool_t received_via_rtcp_mux){
	if (session->eventqs!=NULL){
		OrtpEvent *ev=ortp_event_new(ORTP_EVENT_RTCP_PACKET_RECEIVED);
		OrtpEventData *d=ortp_event_get_data(ev);
		d->packet=m;
		d->info.socket_type = received_via_rtcp_mux ? OrtpRTPSocket : OrtpRTCPSocket;
		rtp_session_dispatch_event(session,ev);
	}
	else freemsg(m);  /* avoid memory leak */
}

static void compute_rtt(RtpSession *session, const struct timeval *now, uint32_t lrr, uint32_t dlrr){
	uint64_t curntp=ortp_timeval_to_ntp(now);
	uint32_t approx_ntp=(curntp>>16) & 0xFFFFFFFF;
	/*ortp_message("rtt approx_ntp=%u, lrr=%u, dlrr=%u",approx_ntp,lrr,dlrr);*/
	if (lrr!=0 && dlrr!=0){
		/*we cast to int32_t to check for crazy RTT time (negative)*/
		double rtt_frac=(int32_t)(approx_ntp-lrr-dlrr);
		if (rtt_frac>=0){
			rtt_frac/=65536.0;

			session->rtt=(float)rtt_frac;
			/*ortp_message("rtt estimated to %f s",session->rtt);*/
		}else ortp_warning("Negative RTT computation, maybe due to clock adjustments.");
	}
}

static void compute_rtt_from_report_block(RtpSession *session, const struct timeval *now, const report_block_t *rb) {
	uint32_t last_sr_time = report_block_get_last_SR_time(rb);
	uint32_t sr_delay = report_block_get_last_SR_delay(rb);
	compute_rtt(session, now, last_sr_time, sr_delay);
	session->cum_loss = report_block_get_cum_packet_lost(rb);
}

static void compute_rtcp_xr_statistics(RtpSession *session, mblk_t *block, const struct timeval *now) {
	uint64_t ntp_timestamp;
	OrtpRtcpXrStats *stats = &session->rtcp_xr_stats;

	switch (rtcp_XR_get_block_type(block)) {
		case RTCP_XR_RCVR_RTT:
			ntp_timestamp = rtcp_XR_rcvr_rtt_get_ntp_timestamp(block);
			stats->last_rcvr_rtt_ts = (ntp_timestamp >> 16) & 0xffffffff;
			stats->last_rcvr_rtt_time.tv_sec = now->tv_sec;
			stats->last_rcvr_rtt_time.tv_usec = now->tv_usec;
			break;
		case RTCP_XR_DLRR:
			compute_rtt(session, now, rtcp_XR_dlrr_get_lrr(block), rtcp_XR_dlrr_get_dlrr(block));
			break;
		default:
			break;
	}
}

static void notify_tmmbr_received(RtpSession *session, mblk_t *m) {
	if (session->eventqs != NULL) {
		rtcp_fb_tmmbr_fci_t *fci = rtcp_RTPFB_tmmbr_get_fci(m);
		OrtpEvent *ev = ortp_event_new(ORTP_EVENT_TMMBR_RECEIVED);
		OrtpEventData *d = ortp_event_get_data(ev);
		d->packet = copymsg(m);
		d->info.tmmbr_mxtbr = rtcp_fb_tmmbr_fci_get_mxtbr_mantissa(fci) * (1 << rtcp_fb_tmmbr_fci_get_mxtbr_exp(fci));
		rtp_session_dispatch_event(session, ev);
	}
}

static void handle_rtcp_rtpfb_packet(RtpSession *session, mblk_t *block) {
	switch (rtcp_RTPFB_get_type(block)) {
		case RTCP_RTPFB_TMMBR:
			if (session->rtcp.tmmbr_info.received) freemsg(session->rtcp.tmmbr_info.received);
			session->rtcp.tmmbr_info.received = copymsg(block);
			rtp_session_send_rtcp_fb_tmmbn(session, rtcp_RTPFB_get_packet_sender_ssrc(block));
			notify_tmmbr_received(session, block);
			break;
		case RTCP_RTPFB_TMMBN:
			if (session->rtcp.tmmbr_info.sent) {
				rtcp_fb_tmmbr_fci_t *tmmbn_fci = rtcp_RTPFB_tmmbr_get_fci(block);
				rtcp_fb_tmmbr_fci_t *tmmbr_fci = rtcp_RTPFB_tmmbr_get_fci(session->rtcp.tmmbr_info.sent);
				if ((ntohl(tmmbn_fci->ssrc) == rtp_session_get_send_ssrc(session)) && (tmmbn_fci->value == tmmbr_fci->value)) {
					freemsg(session->rtcp.tmmbr_info.sent);
					session->rtcp.tmmbr_info.sent = NULL;
				}
			}
			break;
		default:
			break;
	}
}

/*
 * @brief : for SR packets, retrieves their timestamp, gets the date, and stores these information into the session descriptor. The date values may be used for setting some fields of the report block of the next RTCP packet to be sent.
 * @param session : the current session descriptor.
 * @param block : the block descriptor that may contain a SR RTCP message.
 * @return 0 if the packet is a real RTCP packet, -1 otherwise. 
 * @note a basic parsing is done on the block structure. However, if it fails, no error is returned, and the session descriptor is left as is, so it does not induce any change in the caller procedure behaviour.
 * @note the packet is freed or is taken ownership if -1 is returned
 */
static int process_rtcp_packet( RtpSession *session, mblk_t *block, struct sockaddr *addr, socklen_t addrlen ) {
	rtcp_common_header_t *rtcp;
	RtpStream * rtpstream = &session->rtp;

	int msgsize = (int) ( block->b_wptr - block->b_rptr );
	if ( msgsize < RTCP_COMMON_HEADER_SIZE ) {
		ortp_warning( "Receiving a too short RTCP packet" );
		freemsg(block);
		return -1;
	}

	rtcp = (rtcp_common_header_t *)block->b_rptr;

	if (rtcp->version != 2){
		/* try to see if it is a STUN packet */
		uint16_t stunlen = *((uint16_t *)(block->b_rptr + sizeof(uint16_t)));
		stunlen = ntohs(stunlen);
		if (stunlen + 20 == block->b_wptr - block->b_rptr) {
			/* this looks like a stun packet */
			if (session->eventqs != NULL) {
				OrtpEvent *ev = ortp_event_new(ORTP_EVENT_STUN_PACKET_RECEIVED);
				OrtpEventData *ed = ortp_event_get_data(ev);
				ed->packet = block;
				ed->source_addrlen=addrlen;
				memcpy(&ed->source_addr,addr,addrlen);
				ed->info.socket_type = OrtpRTCPSocket;
				rtp_session_dispatch_event(session, ev);
				return -1;
			}
		}else{
			ortp_warning("RtpSession [%p] receiving rtcp packet with version number != 2, discarded", session);
		}
		freemsg(block);
		return -1;
	}

	update_recv_bytes(&session->rtcp.gs, (int)(block->b_wptr - block->b_rptr));

	/* compound rtcp packet can be composed by more than one rtcp message */
	do{
		struct timeval reception_date;
		const report_block_t *rb;

		/* Getting the reception date from the main clock */
		ortp_gettimeofday( &reception_date, NULL );

		if (rtcp_is_SR(block) ) {
			rtcp_sr_t *sr = (rtcp_sr_t *) rtcp;

			/* The session descriptor values are reset in case there is an error in the SR block parsing */
			rtpstream->last_rcv_SR_ts = 0;
			rtpstream->last_rcv_SR_time.tv_usec = 0;
			rtpstream->last_rcv_SR_time.tv_sec = 0;

			if ( ntohl( sr->ssrc ) != session->rcv.ssrc ) {
				ortp_debug( "Receiving a RTCP SR packet from an unknown ssrc" );
				return 0;
			}

			if ( msgsize < RTCP_COMMON_HEADER_SIZE + RTCP_SSRC_FIELD_SIZE + RTCP_SENDER_INFO_SIZE + ( RTCP_REPORT_BLOCK_SIZE * sr->ch.rc ) ) {
				ortp_debug( "Receiving a too short RTCP SR packet" );
				return 0;
			}

			/* Saving the data to fill LSR and DLSR field in next RTCP report to be transmitted */
			/* This value will be the LSR field of the next RTCP report (only the central 32 bits are kept, as described in par.4 of RC3550) */
			rtpstream->last_rcv_SR_ts = ( ntohl( sr->si.ntp_timestamp_msw ) << 16 ) | ( ntohl( sr->si.ntp_timestamp_lsw ) >> 16 );
			/* This value will help in processing the DLSR of the next RTCP report ( see report_block_init() in rtcp.cc ) */
			rtpstream->last_rcv_SR_time.tv_usec = reception_date.tv_usec;
			rtpstream->last_rcv_SR_time.tv_sec = reception_date.tv_sec;
			rb=rtcp_SR_get_report_block(block,0);
			if (rb) compute_rtt_from_report_block(session,&reception_date,rb);
		}else if ( rtcp_is_RR(block)){
			rb=rtcp_RR_get_report_block(block,0);
			if (rb) compute_rtt_from_report_block(session,&reception_date,rb);
		} else if (rtcp_is_XR(block)) {
			compute_rtcp_xr_statistics(session, block, &reception_date);
		} else if (rtcp_is_RTPFB(block)) {
			handle_rtcp_rtpfb_packet(session, block);
		}
	}while (rtcp_next_packet(block));
	rtcp_rewind(block);
	return 0;
}

static void reply_to_collaborative_rtcp_xr_packet(RtpSession *session, mblk_t *block) {
	if (rtcp_is_XR(block) && (rtcp_XR_get_block_type(block) == RTCP_XR_RCVR_RTT)) {
		session->rtcp.rtcp_xr_dlrr_to_send = TRUE;
	}
}

static void rtp_process_incoming_packet(RtpSession * session, mblk_t * mp, bool_t is_rtp_packet, uint32_t user_ts, bool_t received_via_rtcp_mux) {
	bool_t sock_connected=(is_rtp_packet && !!(session->flags & RTP_SOCKET_CONNECTED))
		|| (!is_rtp_packet && !!(session->flags & RTCP_SOCKET_CONNECTED));

	struct sockaddr *remaddr = NULL;
	socklen_t addrlen;

	if (!mp){
		return;
	}
	remaddr = (struct sockaddr *)&mp->net_addr;
	addrlen = mp->net_addrlen;


	if (session->spliced_session){
		/*this will forward all traffic to the spliced session*/
		rtp_session_do_splice(session, mp, is_rtp_packet);
	}

	/* store the sender RTP address to do symmetric RTP at start mainly for stun packets.
	 * --For rtp packet symmetric RTP is handled in rtp_session_rtp_parse() after first valid rtp packet received.
	 * --For rtcp, only swicth if valid rtcp packet && first rtcp packet received*/
	rtp_session_update_remote_sock_addr(session,mp,is_rtp_packet,is_rtp_packet || (rtp_get_version(mp) != 2));

	if (is_rtp_packet){
		if (session->use_connect && session->symmetric_rtp && !sock_connected ){
			/* In the case where use_connect is false, */
			if (try_connect(session->rtp.gs.socket,remaddr,addrlen)) {
					session->flags|=RTP_SOCKET_CONNECTED;
			}
		}
		/* then parse the message and put on jitter buffer queue */
		update_recv_bytes(&session->rtp.gs, (int)(mp->b_wptr - mp->b_rptr));
		rtp_session_rtp_parse(session, mp, user_ts, remaddr,addrlen);
		/*for bandwidth measurements:*/
	}else {
		if (session->use_connect && session->symmetric_rtp && !sock_connected){
			if (try_connect(session->rtcp.gs.socket,remaddr,addrlen)) {
					session->flags|=RTCP_SOCKET_CONNECTED;
			}
		}
		if (process_rtcp_packet(session, mp, remaddr, addrlen) == 0){
			/* a copy is needed since rtp_session_notify_inc_rtcp will free the mp,
			and we don't want to send RTCP XR packet before notifying the application
			that a message has been received*/
			mblk_t * copy = copymsg(mp);
			session->stats.recv_rtcp_packets++;
			/* post an event to notify the application */
			rtp_session_notify_inc_rtcp(session, mp, received_via_rtcp_mux);
			/* reply to collaborative RTCP XR packets if needed. */
			if (session->rtcp.xr_conf.enabled == TRUE){
				reply_to_collaborative_rtcp_xr_packet(session, copy);
			}
			freemsg(copy);
		}
	}
}

void rtp_session_process_incoming(RtpSession * session, mblk_t *mp, bool_t is_rtp_packet, uint32_t ts, bool_t received_via_rtcp_mux) {
	if (session->net_sim_ctx && session->net_sim_ctx->params.mode == OrtpNetworkSimulatorInbound) {
		/*drain possible packets queued in the network simulator*/
		mp = rtp_session_network_simulate(session, mp, &is_rtp_packet);
		rtp_process_incoming_packet(session, mp, is_rtp_packet, ts, received_via_rtcp_mux); /*BUG here: received_via_rtcp_mux is not preserved by network simulator*/
	} else if (mp != NULL) {
		rtp_process_incoming_packet(session, mp, is_rtp_packet, ts, received_via_rtcp_mux);
	}
}

int rtp_session_rtp_recv (RtpSession * session, uint32_t user_ts) {
	int error;
	struct sockaddr_storage remaddr;
	socklen_t addrlen = sizeof (remaddr);
	mblk_t *mp;

	if ((session->rtp.gs.socket==(ortp_socket_t)-1) && !rtp_session_using_transport(session, rtp)) return -1;  /*session has no sockets for the moment*/

	while (1)
	{
		bool_t sock_connected=!!(session->flags & RTP_SOCKET_CONNECTED);

		mp = msgb_allocator_alloc(&session->rtp.gs.allocator, session->recv_buf_size);
		mp->reserved1 = user_ts;

		if (sock_connected){
			error=rtp_session_recvfrom(session, TRUE, mp, 0, NULL, NULL);
		}else if (rtp_session_using_transport(session, rtp)) {
			error = (session->rtp.gs.tr->t_recvfrom)(session->rtp.gs.tr, mp, 0, (struct sockaddr *) &remaddr, &addrlen);
		} else {
			error = rtp_session_recvfrom(session, TRUE, mp, 0, (struct sockaddr *) &remaddr, &addrlen);
		}
		if (error > 0){
			mp->b_wptr+=error;
			rtp_session_process_incoming(session, mp, TRUE, user_ts, FALSE);
		}
		else
		{
			int errnum;
			if (error==-1 && !is_would_block_error((errnum=getSocketErrorCode())) )
			{
				if (session->on_network_error.count>0){
					rtp_signal_table_emit3(&session->on_network_error,"Error receiving RTP packet",ORTP_INT_TO_POINTER(getSocketErrorCode()));
				}else ortp_warning("Error receiving RTP packet: %s, err num  [%i],error [%i]",getSocketError(),errnum,error);
#ifdef __ios
				/*hack for iOS and non-working socket because of background mode*/
				if (errnum==ENOTCONN){
					/*re-create new sockets */
					rtp_session_set_local_addr(session,session->rtp.gs.sockfamily==AF_INET ? "0.0.0.0" : "::0",session->rtp.gs.loc_port,session->rtcp.gs.loc_port);
				}
#endif
			}else{
				/*EWOULDBLOCK errors or transports returning 0 are ignored.*/
				rtp_session_process_incoming(session, NULL, TRUE, user_ts, FALSE);
			}
			freemsg(mp);
			return -1;
		}
	}
	return error;
}

int rtp_session_rtcp_recv (RtpSession * session) {
	int error;
	struct sockaddr_storage remaddr;

	socklen_t addrlen = sizeof (remaddr);
	mblk_t *mp;

	if (session->rtcp.gs.socket==(ortp_socket_t)-1 && !rtp_session_using_transport(session, rtcp)) return -1;  /*session has no RTCP sockets for the moment*/


	while (1)
	{
		bool_t sock_connected=!!(session->flags & RTCP_SOCKET_CONNECTED);

		mp = msgb_allocator_alloc(&session->rtp.gs.allocator, RTCP_MAX_RECV_BUFSIZE);
		mp->reserved1 = session->rtp.rcv_last_app_ts;

		if (sock_connected){
			error=rtp_session_recvfrom(session, FALSE, mp, 0, NULL, NULL);
		}else{
			addrlen=sizeof (remaddr);

			if (rtp_session_using_transport(session, rtcp)){
				error=(session->rtcp.gs.tr->t_recvfrom)(session->rtcp.gs.tr, mp, 0,
					(struct sockaddr *) &remaddr,
					&addrlen);
			}else{
				error=rtp_session_recvfrom(session, FALSE, mp, 0,
					(struct sockaddr *) &remaddr,
					&addrlen);
			}
		}
		if (error > 0)
		{
			mp->b_wptr += error;
			rtp_session_process_incoming(session, mp, FALSE, session->rtp.rcv_last_app_ts, FALSE);
		}
		else
		{
			int errnum;
			if (error==-1 && !is_would_block_error((errnum=getSocketErrorCode())) )
			{
				if (session->on_network_error.count>0){
					rtp_signal_table_emit3(&session->on_network_error,"Error receiving RTCP packet",ORTP_INT_TO_POINTER(getSocketErrorCode()));
				}else ortp_warning("Error receiving RTCP packet: %s, err num  [%i],error [%i]",getSocketError(),errnum,error);
#ifdef __ios
				/*hack for iOS and non-working socket because of background mode*/
				if (errnum==ENOTCONN){
					/*re-create new sockets */
					rtp_session_set_local_addr(session,session->rtcp.gs.sockfamily==AF_INET ? "0.0.0.0" : "::0",session->rtcp.gs.loc_port,session->rtcp.gs.loc_port);
				}
#endif
				session->rtp.recv_errno=errnum;
			}else{
				/*EWOULDBLOCK errors or transports returning 0 are ignored.*/
				rtp_session_process_incoming(session, NULL, FALSE, session->rtp.rcv_last_app_ts, FALSE);
			}

			freemsg(mp);
			return -1; /* avoids an infinite loop ! */
		}
	}
	return error;
}

int  rtp_session_update_remote_sock_addr(RtpSession * session, mblk_t * mp, bool_t is_rtp,bool_t only_at_start) {
	struct sockaddr_storage * rem_addr = NULL;
	socklen_t *rem_addrlen;
	const char* socket_type;
	bool_t sock_connected;
	bool_t do_address_change = /*(rtp_get_version(mp) == 2 && */ !only_at_start;

	if (!rtp_session_get_symmetric_rtp(session))
		return -1; /*nothing to try if not rtp symetric*/

	if (is_rtp) {
		rem_addr = &session->rtp.gs.rem_addr;
		rem_addrlen = &session->rtp.gs.rem_addrlen;
		socket_type = "rtp";
		sock_connected = session->flags & RTP_SOCKET_CONNECTED;
		do_address_change =  session->rtp.gs.socket != (ortp_socket_t)-1  && ( do_address_change || rtp_session_get_stats(session)->packet_recv == 0);
	} else {
		rem_addr = &session->rtcp.gs.rem_addr;
		rem_addrlen = &session->rtcp.gs.rem_addrlen;
		sock_connected = session->flags & RTCP_SOCKET_CONNECTED;
		socket_type = "rtcp";
		do_address_change = session->rtcp.gs.socket != (ortp_socket_t)-1  && (do_address_change || rtp_session_get_stats(session)->recv_rtcp_packets == 0);
	}

	if (do_address_change
		&& rem_addr
		&& !sock_connected
		&& !ortp_is_multicast_addr((const struct sockaddr*)rem_addr)
		&& memcmp(rem_addr,&mp->net_addr,mp->net_addrlen) !=0) {
		char current_ip_address[64]={0};
		char new_ip_address[64]={0};

		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)rem_addr, *rem_addrlen, current_ip_address, sizeof(current_ip_address));
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&mp->net_addr, mp->net_addrlen, new_ip_address, sizeof(new_ip_address));
		ortp_message("Switching %s destination from %s to %s for session [%p]"
			   , socket_type
			   , current_ip_address
			   , new_ip_address
			   , session);

		memcpy(rem_addr,&mp->net_addr,mp->net_addrlen);
		*rem_addrlen = mp->net_addrlen;
		return 0;
	}
	return -1;
}
