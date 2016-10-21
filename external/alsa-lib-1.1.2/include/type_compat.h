/*
 *  ALSA lib - compatibility header to be included by local.h
 *  Copyright (c) 2016 by  Thomas Klausner <wiz@NetBSD.org>
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

#ifndef __TYPE_COMPAT_H
#define __TYPE_COMPAT_H

#ifndef EBADFD
#define EBADFD EBADF
#endif
#ifndef ESTRPIPE
#define ESTRPIPE EPIPE
#endif

#ifndef __u16
#define __u16	uint16_t
#endif
#ifndef __u32
#define __u32	uint32_t
#endif
#ifndef __u64
#define __u64	uint64_t
#endif
#ifndef __le16
#define __le16	uint16_t
#endif
#ifndef __le32
#define __le32	uint32_t
#endif
#ifndef __le64
#define __le64	uint64_t
#endif
#ifndef u_int8_t
#define u_int8_t	uint8_t
#endif
#ifndef u_int16_t
#define u_int16_t	uint16_t
#endif
#ifndef u_int32_t
#define u_int32_t	uint32_t
#endif
#ifndef u_int32_t
#define u_int32_t	uint64_t
#endif
#ifndef __kernel_pid_t
#define __kernel_pid_t	pid_t
#endif
#ifndef __kernel_off_t
#define __kernel_off_t	off_t
#endif

#ifndef __bitwise
#define __bitwise
#endif

#endif
