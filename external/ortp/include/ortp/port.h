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
/* this file is responsible of the portability of the stack */

#ifndef ORTP_PORT_H
#define ORTP_PORT_H


#if !defined(_WIN32) && !defined(_WIN32_WCE)
/********************************/
/* definitions for UNIX flavour */
/********************************/

#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __linux
#include <stdint.h>
#endif


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if defined(_XOPEN_SOURCE_EXTENDED) || !defined(__hpux)
#include <arpa/inet.h>
#endif



#include <sys/time.h>

#include <netdb.h>

typedef int ortp_socket_t;
typedef pthread_t ortp_thread_t;
typedef pthread_mutex_t ortp_mutex_t;
typedef pthread_cond_t ortp_cond_t;

#ifdef __INTEL_COMPILER
#pragma warning(disable : 111)		// statement is unreachable
#pragma warning(disable : 181)		// argument is incompatible with corresponding format string conversion
#pragma warning(disable : 188)		// enumerated type mixed with another type
#pragma warning(disable : 593)		// variable "xxx" was set but never used
#pragma warning(disable : 810)		// conversion from "int" to "unsigned short" may lose significant bits
#pragma warning(disable : 869)		// parameter "xxx" was never referenced
#pragma warning(disable : 981)		// operands are evaluated in unspecified order
#pragma warning(disable : 1418)		// external function definition with no prior declaration
#pragma warning(disable : 1419)		// external declaration in primary source file
#pragma warning(disable : 1469)		// "cc" clobber ignored
#endif

#define ORTP_PUBLIC
#define ORTP_INLINE			inline

#ifdef __cplusplus
extern "C"
{
#endif

int __ortp_thread_join(ortp_thread_t thread, void **ptr);
int __ortp_thread_create(ortp_thread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg);
unsigned long __ortp_thread_self(void);

#ifdef __cplusplus
}
#endif

#define ortp_thread_create	__ortp_thread_create
#define ortp_thread_join	__ortp_thread_join
#define ortp_thread_self	__ortp_thread_self
#define ortp_thread_exit	pthread_exit
#define ortp_mutex_init		pthread_mutex_init
#define ortp_mutex_lock		pthread_mutex_lock
#define ortp_mutex_unlock	pthread_mutex_unlock
#define ortp_mutex_destroy	pthread_mutex_destroy
#define ortp_cond_init		pthread_cond_init
#define ortp_cond_signal	pthread_cond_signal
#define ortp_cond_broadcast	pthread_cond_broadcast
#define ortp_cond_wait		pthread_cond_wait
#define ortp_cond_destroy	pthread_cond_destroy

#define SOCKET_OPTION_VALUE	void *
#define SOCKET_BUFFER		void *

#define getSocketError() strerror(errno)
#define getSocketErrorCode() (errno)
#define ortp_gettimeofday(tv,tz) gettimeofday(tv,tz)
#define ortp_log10f(x)	log10f(x)


#else
/*********************************/
/* definitions for WIN32 flavour */
/*********************************/

#include <stdio.h>
#define _CRT_RAND_S
#include <stdlib.h>
#include <stdarg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#include <io.h>
#endif

#if defined(__MINGW32__) || !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_DESKTOP)
#define ORTP_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define ORTP_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define ORTP_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define ORTP_WINDOWS_UNIVERSAL 1
#endif
#endif

#ifdef _MSC_VER
#ifdef ORTP_STATIC
#define ORTP_PUBLIC
#else
#ifdef ORTP_EXPORTS
#define ORTP_PUBLIC	__declspec(dllexport)
#else
#define ORTP_PUBLIC	__declspec(dllimport)
#endif
#endif
#pragma push_macro("_WINSOCKAPI_")
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

typedef  unsigned __int64 uint64_t;
typedef  __int64 int64_t;
typedef  unsigned short uint16_t;
typedef  unsigned int uint32_t;
typedef  int int32_t;
typedef  unsigned char uint8_t;
typedef __int16 int16_t;
#else
#include <stdint.h> /*provided by mingw32*/
#include <io.h>
#define ORTP_PUBLIC
ORTP_PUBLIC char* strtok_r(char *str, const char *delim, char **nextp);
#endif

#define vsnprintf	_vsnprintf

typedef SOCKET ortp_socket_t;
#ifdef ORTP_WINDOWS_DESKTOP
typedef HANDLE ortp_cond_t;
typedef HANDLE ortp_mutex_t;
#else
typedef CONDITION_VARIABLE ortp_cond_t;
typedef SRWLOCK ortp_mutex_t;
#endif
typedef HANDLE ortp_thread_t;

#define ortp_thread_create	WIN_thread_create
#define ortp_thread_join	WIN_thread_join
#define ortp_thread_self	WIN_thread_self
#define ortp_thread_exit(arg)
#define ortp_mutex_init		WIN_mutex_init
#define ortp_mutex_lock		WIN_mutex_lock
#define ortp_mutex_unlock	WIN_mutex_unlock
#define ortp_mutex_destroy	WIN_mutex_destroy
#define ortp_cond_init		WIN_cond_init
#define ortp_cond_signal	WIN_cond_signal
#define ortp_cond_broadcast	WIN_cond_broadcast
#define ortp_cond_wait		WIN_cond_wait
#define ortp_cond_destroy	WIN_cond_destroy


#ifdef __cplusplus
extern "C"
{
#endif

ORTP_PUBLIC int WIN_mutex_init(ortp_mutex_t *m, void *attr_unused);
ORTP_PUBLIC int WIN_mutex_lock(ortp_mutex_t *mutex);
ORTP_PUBLIC int WIN_mutex_unlock(ortp_mutex_t *mutex);
ORTP_PUBLIC int WIN_mutex_destroy(ortp_mutex_t *mutex);
ORTP_PUBLIC int WIN_thread_create(ortp_thread_t *t, void *attr_unused, void *(*func)(void*), void *arg);
ORTP_PUBLIC int WIN_thread_join(ortp_thread_t thread, void **unused);
ORTP_PUBLIC unsigned long WIN_thread_self(void);
ORTP_PUBLIC int WIN_cond_init(ortp_cond_t *cond, void *attr_unused);
ORTP_PUBLIC int WIN_cond_wait(ortp_cond_t * cond, ortp_mutex_t * mutex);
ORTP_PUBLIC int WIN_cond_signal(ortp_cond_t * cond);
ORTP_PUBLIC int WIN_cond_broadcast(ortp_cond_t * cond);
ORTP_PUBLIC int WIN_cond_destroy(ortp_cond_t * cond);

#ifdef __cplusplus
}
#endif

#define SOCKET_OPTION_VALUE	char *
#define ORTP_INLINE			__inline

#if defined(_WIN32_WCE)

#define ortp_log10f(x)		(float)log10 ((double)x)

#ifdef assert
	#undef assert
#endif /*assert*/
#define assert(exp)	((void)0)

#ifdef errno
	#undef errno
#endif /*errno*/
#define  errno GetLastError()
#ifdef strerror
		#undef strerror
#endif /*strerror*/
const char * ortp_strerror(DWORD value);
#define strerror ortp_strerror


#else /*_WIN32_WCE*/

#define ortp_log10f(x)	log10f(x)

#endif

ORTP_PUBLIC const char *getWinSocketError(int error);
#ifndef getSocketErrorCode
#define getSocketErrorCode() WSAGetLastError()
#endif
#ifndef getSocketError
#define getSocketError() getWinSocketError(WSAGetLastError())
#endif

#ifndef F_OK
#define F_OK 00 /* Visual Studio does not define F_OK */
#endif


#ifdef __cplusplus
extern "C"{
#endif
ORTP_PUBLIC int ortp_gettimeofday (struct timeval *tv, void* tz);
#ifdef _WORKAROUND_MINGW32_BUGS
char * WSAAPI gai_strerror(int errnum);
#endif
#ifdef __cplusplus
}
#endif

#endif

#ifndef _BOOL_T_
#define _BOOL_T_
typedef unsigned char bool_t;
#endif /* _BOOL_T_ */
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef struct _OList OList;

typedef struct ortpTimeSpec{
	int64_t tv_sec;
	int64_t tv_nsec;
}ortpTimeSpec;

#ifdef __cplusplus
extern "C"{
#endif

ORTP_PUBLIC void* ortp_malloc(size_t sz);
ORTP_PUBLIC void ortp_free(void *ptr);
ORTP_PUBLIC void* ortp_realloc(void *ptr, size_t sz);
ORTP_PUBLIC void* ortp_malloc0(size_t sz);
ORTP_PUBLIC char * ortp_strdup(const char *tmp);

/*override the allocator with this method, to be called BEFORE ortp_init()*/
typedef struct _OrtpMemoryFunctions{
	void *(*malloc_fun)(size_t sz);
	void *(*realloc_fun)(void *ptr, size_t sz);
	void (*free_fun)(void *ptr);
}OrtpMemoryFunctions;

void ortp_set_memory_functions(OrtpMemoryFunctions *functions);

#define ortp_new(type,count)	(type*)ortp_malloc(sizeof(type)*(count))
#define ortp_new0(type,count)	(type*)ortp_malloc0(sizeof(type)*(count))

ORTP_PUBLIC int close_socket(ortp_socket_t sock);
ORTP_PUBLIC int set_non_blocking_socket(ortp_socket_t sock);

ORTP_PUBLIC char *ortp_strndup(const char *str,int n);
ORTP_PUBLIC char *ortp_strdup_printf(const char *fmt,...);
ORTP_PUBLIC char *ortp_strdup_vprintf(const char *fmt, va_list ap);
ORTP_PUBLIC char *ortp_strcat_printf(char *dst, const char *fmt,...);
ORTP_PUBLIC char *ortp_strcat_vprintf(char *dst, const char *fmt, va_list ap);

ORTP_PUBLIC int ortp_file_exist(const char *pathname);

ORTP_PUBLIC void ortp_get_cur_time(ortpTimeSpec *ret);
void _ortp_get_cur_time(ortpTimeSpec *ret, bool_t realtime);
ORTP_PUBLIC uint64_t ortp_get_cur_time_ms(void);
ORTP_PUBLIC void ortp_sleep_ms(int ms);
ORTP_PUBLIC void ortp_sleep_until(const ortpTimeSpec *ts);
ORTP_PUBLIC int ortp_timespec_compare(const ortpTimeSpec *s1, const ortpTimeSpec *s2);
ORTP_PUBLIC unsigned int ortp_random(void);

/* portable named pipes  and shared memory*/
#if !defined(_WIN32_WCE)
#ifdef _WIN32
typedef HANDLE ortp_pipe_t;
#define ORTP_PIPE_INVALID INVALID_HANDLE_VALUE
#else
typedef int ortp_pipe_t;
#define ORTP_PIPE_INVALID (-1)
#endif

ORTP_PUBLIC ortp_pipe_t ortp_server_pipe_create(const char *name);
/*
 * warning: on win32 ortp_server_pipe_accept_client() might return INVALID_HANDLE_VALUE without
 * any specific error, this happens when ortp_server_pipe_close() is called on another pipe.
 * This pipe api is not thread-safe.
*/
ORTP_PUBLIC ortp_pipe_t ortp_server_pipe_accept_client(ortp_pipe_t server);
ORTP_PUBLIC int ortp_server_pipe_close(ortp_pipe_t spipe);
ORTP_PUBLIC int ortp_server_pipe_close_client(ortp_pipe_t client);

ORTP_PUBLIC ortp_pipe_t ortp_client_pipe_connect(const char *name);
ORTP_PUBLIC int ortp_client_pipe_close(ortp_pipe_t sock);

ORTP_PUBLIC int ortp_pipe_read(ortp_pipe_t p, uint8_t *buf, int len);
ORTP_PUBLIC int ortp_pipe_write(ortp_pipe_t p, const uint8_t *buf, int len);

ORTP_PUBLIC void *ortp_shm_open(unsigned int keyid, int size, int create);
ORTP_PUBLIC void ortp_shm_close(void *memory);

ORTP_PUBLIC	bool_t ortp_is_multicast_addr(const struct sockaddr *addr);
	
	
#endif

#ifdef __cplusplus
}

#endif


#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(ORTP_STATIC)
#ifdef ORTP_EXPORTS
   #define ORTP_VAR_PUBLIC    extern __declspec(dllexport)
#else
   #define ORTP_VAR_PUBLIC    __declspec(dllimport)
#endif
#else
   #define ORTP_VAR_PUBLIC    extern
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(i)	(((uint8_t *) (i))[0] == 0xff)
#endif

/*define __ios when we are compiling for ios.
 The TARGET_OS_IPHONE macro is stupid, it is defined to 0 when compiling on mac os x.
*/
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE==1
#define __ios 1
#endif

#endif


