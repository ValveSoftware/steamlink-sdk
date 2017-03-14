/***************************************************************************
* config.h.cmake
* Copyright (C) 2014  Belledonne Communications, Grenoble France
*
****************************************************************************
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
****************************************************************************/

#define ORTP_MAJOR_VERSION ${ORTP_MAJOR_VERSION}
#define ORTP_MINOR_VERSION ${ORTP_MINOR_VERSION}
#define ORTP_MICRO_VERSION ${ORTP_MICRO_VERSION}
#define ORTP_VERSION "${ORTP_VERSION}"

#cmakedefine HAVE_SYS_UIO_H 1
#cmakedefine HAVE_SYS_AUDIO_H 1
#cmakedefine HAVE_SYS_SHM_H 1
#cmakedefine HAVE_STDATOMIC_H 1
#cmakedefine HAVE_ARC4RANDOM 1

#cmakedefine ORTP_BIGENDIAN

#cmakedefine PERF
#cmakedefine ORTP_STATIC
#cmakedefine ORTP_TIMESTAMP
#cmakedefine ORTP_DEBUG_MODE
#cmakedefine ORTP_DEFAULT_THREAD_STACK_SIZE ${ORTP_DEFAULT_THREAD_STACK_SIZE}
#cmakedefine POSIXTIMER_INTERVAL ${POSIXTIMER_INTERVAL}
#cmakedefine __APPLE_USE_RFC_3542
