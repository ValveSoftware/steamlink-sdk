/**
 * \file socket.c
 * \brief Socket helper routines
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2003
 */
/*
 *  Socket helper routines
 *  Copyright (c) 2003 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
  
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include "local.h"

#ifndef DOC_HIDDEN
int snd_send_fd(int sock, void *data, size_t len, int fd)
{
	int ret;
	size_t cmsg_len = CMSG_LEN(sizeof(int));
	struct cmsghdr *cmsg = alloca(cmsg_len);
	int *fds = (int *) CMSG_DATA(cmsg);
	struct msghdr msghdr;
	struct iovec vec;

	vec.iov_base = (void *)&data;
	vec.iov_len = len;

	cmsg->cmsg_len = cmsg_len;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*fds = fd;

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &vec;
 	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsg;
	msghdr.msg_controllen = cmsg_len;
	msghdr.msg_flags = 0;

	ret = sendmsg(sock, &msghdr, 0 );
	if (ret < 0) {
		SYSERR("sendmsg failed");
		return -errno;
	}
	return ret;
}

int snd_receive_fd(int sock, void *data, size_t len, int *fd)
{
	int ret;
	size_t cmsg_len = CMSG_LEN(sizeof(int));
	struct cmsghdr *cmsg = alloca(cmsg_len);
	int *fds = (int *) CMSG_DATA(cmsg);
	struct msghdr msghdr;
	struct iovec vec;

	vec.iov_base = (void *)&data;
	vec.iov_len = len;

	cmsg->cmsg_len = cmsg_len;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*fds = -1;

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &vec;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsg;
	msghdr.msg_controllen = cmsg_len;
	msghdr.msg_flags = 0;

	ret = recvmsg(sock, &msghdr, 0);
	if (ret < 0) {
		SYSERR("recvmsg failed");
		return -errno;
	}
	*fd = *fds;
	return ret;
}
#endif
