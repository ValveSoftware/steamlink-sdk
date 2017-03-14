
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


#ifdef HAVE_CONFIG_H
#include "ortp-config.h"
#endif
#include "ortp/logging.h"
#include "ortp/port.h"
#include "ortp/str_utils.h"
#include "utils.h"
#include <bctoolbox/port.h>

#if	defined(_WIN32) && !defined(_WIN32_WCE)
#include <process.h>
#endif

#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

static void *ortp_libc_malloc(size_t sz){
	return malloc(sz);
}

static void *ortp_libc_realloc(void *ptr, size_t sz){
	return realloc(ptr,sz);
}

static void ortp_libc_free(void*ptr){
	free(ptr);
}

static bool_t allocator_used=FALSE;

static OrtpMemoryFunctions ortp_allocator={
	ortp_libc_malloc,
	ortp_libc_realloc,
	ortp_libc_free
};

void ortp_set_memory_functions(OrtpMemoryFunctions *functions){
	if (allocator_used){
		ortp_fatal("ortp_set_memory_functions() must be called before "
		"first use of ortp_malloc or ortp_realloc");
		return;
	}
	ortp_allocator=*functions;
}

void* ortp_malloc(size_t sz){
	allocator_used=TRUE;
	return ortp_allocator.malloc_fun(sz);
}

void* ortp_realloc(void *ptr, size_t sz){
	allocator_used=TRUE;
	return ortp_allocator.realloc_fun(ptr,sz);
}

void ortp_free(void* ptr){
	ortp_allocator.free_fun(ptr);
}

void * ortp_malloc0(size_t size){
	void *ptr=ortp_malloc(size);
	memset(ptr,0,size);
	return ptr;
}

char * ortp_strdup(const char *tmp){
	size_t sz;
	char *ret;
	if (tmp==NULL)
	  return NULL;
	sz=strlen(tmp)+1;
	ret=(char*)ortp_malloc(sz);
	strcpy(ret,tmp);
	ret[sz-1]='\0';
	return ret;
}

/*
 * this method is an utility method that calls fnctl() on UNIX or
 * ioctlsocket on Win32.
 * int retrun the result of the system method
 */
int set_non_blocking_socket (ortp_socket_t sock){
#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	return fcntl (sock, F_SETFL, O_NONBLOCK);
#else
	unsigned long nonBlock = 1;
	return ioctlsocket(sock, FIONBIO , &nonBlock);
#endif
}


/*
 * this method is an utility method that calls close() on UNIX or
 * closesocket on Win32.
 * int retrun the result of the system method
 */
int close_socket(ortp_socket_t sock){
#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	return close (sock);
#else
	return closesocket(sock);
#endif
}

#if defined (_WIN32_WCE) || defined(_MSC_VER)
int ortp_file_exist(const char *pathname) {
	FILE* fd;
	if (pathname==NULL) return -1;
	fd=fopen(pathname,"r");
	if (fd==NULL) {
		return -1;
	} else {
		fclose(fd);
		return 0;
	}
}
#else
int ortp_file_exist(const char *pathname) {
	return access(pathname,F_OK);
}
#endif /*_WIN32_WCE*/

#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	/* Use UNIX inet_aton method */
#else
	int inet_aton (const char * cp, struct in_addr * addr)
	{
		unsigned long retval;

		retval = inet_addr (cp);

		if (retval == INADDR_NONE)
		{
			return -1;
		}
		else
		{
			addr->S_un.S_addr = retval;
			return 1;
		}
	}
#endif

char *ortp_strndup(const char *str,int n){
	int min=MIN((int)strlen(str),n)+1;
	char *ret=(char*)ortp_malloc(min);
	strncpy(ret,str,min);
	ret[min-1]='\0';
	return ret;
}

#if	!defined(_WIN32) && !defined(_WIN32_WCE)
int __ortp_thread_join(ortp_thread_t thread, void **ptr){
	int err=pthread_join(thread,ptr);
	if (err!=0) {
		ortp_error("pthread_join error: %s",strerror(err));
	}
	return err;
}

int __ortp_thread_create(ortp_thread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg){
	pthread_attr_t my_attr;
	pthread_attr_init(&my_attr);
	if (attr)
		my_attr = *attr;
#ifdef ORTP_DEFAULT_THREAD_STACK_SIZE
	if (ORTP_DEFAULT_THREAD_STACK_SIZE!=0)
		pthread_attr_setstacksize(&my_attr, ORTP_DEFAULT_THREAD_STACK_SIZE);
#endif
	return pthread_create(thread, &my_attr, routine, arg);
}

unsigned long __ortp_thread_self(void) {
	return (unsigned long)pthread_self();
}

#endif
#if	defined(_WIN32) || defined(_WIN32_WCE)

int WIN_mutex_init(ortp_mutex_t *mutex, void *attr)
{
#ifdef ORTP_WINDOWS_DESKTOP
	*mutex=CreateMutex(NULL, FALSE, NULL);
#else
	InitializeSRWLock(mutex);
#endif
	return 0;
}

int WIN_mutex_lock(ortp_mutex_t * hMutex)
{
#ifdef ORTP_WINDOWS_DESKTOP
	WaitForSingleObject(*hMutex, INFINITE); /* == WAIT_TIMEOUT; */
#else
	AcquireSRWLockExclusive(hMutex);
#endif
	return 0;
}

int WIN_mutex_unlock(ortp_mutex_t * hMutex)
{
#ifdef ORTP_WINDOWS_DESKTOP
	ReleaseMutex(*hMutex);
#else
	ReleaseSRWLockExclusive(hMutex);
#endif
	return 0;
}

int WIN_mutex_destroy(ortp_mutex_t * hMutex)
{
#ifdef ORTP_WINDOWS_DESKTOP
	CloseHandle(*hMutex);
#endif
	return 0;
}

typedef struct thread_param{
	void * (*func)(void *);
	void * arg;
}thread_param_t;

static unsigned WINAPI thread_starter(void *data){
	thread_param_t *params=(thread_param_t*)data;
	params->func(params->arg);
	ortp_free(data);
	return 0;
}

#if defined _WIN32_WCE
#    define _beginthreadex	CreateThread
#    define	_endthreadex	ExitThread
#endif

int WIN_thread_create(ortp_thread_t *th, void *attr, void * (*func)(void *), void *data)
{
	thread_param_t *params=ortp_new(thread_param_t,1);
	params->func=func;
	params->arg=data;
	*th=(HANDLE)_beginthreadex( NULL, 0, thread_starter, params, 0, NULL);
	return 0;
}

int WIN_thread_join(ortp_thread_t thread_h, void **unused)
{
	if (thread_h!=NULL)
	{
		WaitForSingleObjectEx(thread_h, INFINITE, FALSE);
		CloseHandle(thread_h);
	}
	return 0;
}

unsigned long WIN_thread_self(void) {
	return (unsigned long)GetCurrentThreadId();
}

int WIN_cond_init(ortp_cond_t *cond, void *attr)
{
#ifdef ORTP_WINDOWS_DESKTOP
	*cond=CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	InitializeConditionVariable(cond);
#endif
	return 0;
}

int WIN_cond_wait(ortp_cond_t* hCond, ortp_mutex_t * hMutex)
{
#ifdef ORTP_WINDOWS_DESKTOP
	//gulp: this is not very atomic ! bug here ?
	WIN_mutex_unlock(hMutex);
	WaitForSingleObject(*hCond, INFINITE);
	WIN_mutex_lock(hMutex);
#else
	SleepConditionVariableSRW(hCond, hMutex, INFINITE, 0);
#endif
	return 0;
}

int WIN_cond_signal(ortp_cond_t * hCond)
{
#ifdef ORTP_WINDOWS_DESKTOP
	SetEvent(*hCond);
#else
	WakeConditionVariable(hCond);
#endif
	return 0;
}

int WIN_cond_broadcast(ortp_cond_t * hCond)
{
	WIN_cond_signal(hCond);
	return 0;
}

int WIN_cond_destroy(ortp_cond_t * hCond)
{
#ifdef ORTP_WINDOWS_DESKTOP
	CloseHandle(*hCond);
#endif
	return 0;
}

#if defined(_WIN32_WCE)
#include <time.h>

const char * ortp_strerror(DWORD value) {
	static TCHAR msgBuf[256];
	FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			value,
			0, // Default language
			(LPTSTR) &msgBuf,
			0,
			NULL
	);
	return (const char *)msgBuf;
}

int
gettimeofday (struct timeval *tv, void *tz)
{
  DWORD timemillis = GetTickCount();
  tv->tv_sec  = timemillis/1000;
  tv->tv_usec = (timemillis - (tv->tv_sec*1000)) * 1000;
  return 0;
}

#else

int ortp_gettimeofday (struct timeval *tv, void* tz)
{
	union
	{
		__int64 ns100; /*time since 1 Jan 1601 in 100ns units */
		FILETIME fileTime;
	} now;

	GetSystemTimeAsFileTime (&now.fileTime);
	tv->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
	tv->tv_sec = (long) ((now.ns100 - 116444736000000000LL) / 10000000LL);
	return (0);
}

#endif

const char *getWinSocketError(int error)
{
	static char buf[80];

	switch (error)
	{
		case WSANOTINITIALISED: return "Windows sockets not initialized : call WSAStartup";
		case WSAEADDRINUSE:		return "Local Address already in use";
		case WSAEADDRNOTAVAIL:	return "The specified address is not a valid address for this machine";
		case WSAEINVAL:			return "The socket is already bound to an address.";
		case WSAENOBUFS:		return "Not enough buffers available, too many connections.";
		case WSAENOTSOCK:		return "The descriptor is not a socket.";
		case WSAECONNRESET:		return "Connection reset by peer";

		default :
			sprintf(buf, "Error code : %d", error);
			return buf;
		break;
	}

	return buf;
}

#ifdef _WORKAROUND_MINGW32_BUGS
char * WSAAPI gai_strerror(int errnum){
	 return (char*)getWinSocketError(errnum);
}
#endif

#endif

#ifndef _WIN32

#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/stat.h>

static char *make_pipe_name(const char *name){
	return ortp_strdup_printf("/tmp/%s",name);
}

/* portable named pipes */
ortp_socket_t ortp_server_pipe_create(const char *name){
	struct sockaddr_un sa;
	char *pipename=make_pipe_name(name);
	ortp_socket_t sock;
	sock=socket(AF_UNIX,SOCK_STREAM,0);
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,pipename,sizeof(sa.sun_path)-1);
	unlink(pipename);/*in case we didn't finished properly previous time */
	ortp_free(pipename);
	fchmod(sock,S_IRUSR|S_IWUSR);
	if (bind(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){
		ortp_error("Failed to bind command unix socket: %s",strerror(errno));
		return -1;
	}
	listen(sock,1);
	return sock;
}

ortp_socket_t ortp_server_pipe_accept_client(ortp_socket_t server){
	struct sockaddr_un su;
	socklen_t ssize=sizeof(su);
	ortp_socket_t client_sock=accept(server,(struct sockaddr*)&su,&ssize);
	return client_sock;
}

int ortp_server_pipe_close_client(ortp_socket_t client){
	return close(client);
}

int ortp_server_pipe_close(ortp_socket_t spipe){
	struct sockaddr_un sa;
	socklen_t len=sizeof(sa);
	int err;
	/*this is to retrieve the name of the pipe, in order to unlink the file*/
	err=getsockname(spipe,(struct sockaddr*)&sa,&len);
	if (err==0){
		unlink(sa.sun_path);
	}else ortp_error("getsockname(): %s",strerror(errno));
	return close(spipe);
}

ortp_socket_t ortp_client_pipe_connect(const char *name){
	ortp_socket_t sock = -1;
	struct sockaddr_un sa;
	struct stat fstats;
	char *pipename=make_pipe_name(name);
	uid_t uid = getuid();

	// check that the creator of the pipe is us
	if( (stat(name, &fstats) == 0) && (fstats.st_uid != uid) ){
		ortp_error("UID of file %s (%lu) differs from ours (%lu)", pipename, (unsigned long)fstats.st_uid, (unsigned long)uid);
		return -1;
	}

	sock = socket(AF_UNIX,SOCK_STREAM,0);
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,pipename,sizeof(sa.sun_path)-1);
	ortp_free(pipename);
	if (connect(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){
		close(sock);
		return -1;
	}
	return sock;
}

int ortp_pipe_read(ortp_socket_t p, uint8_t *buf, int len){
	return read(p,buf,len);
}

int ortp_pipe_write(ortp_socket_t p, const uint8_t *buf, int len){
	return write(p,buf,len);
}

int ortp_client_pipe_close(ortp_socket_t sock){
	return close(sock);
}

#ifdef HAVE_SYS_SHM_H

void *ortp_shm_open(unsigned int keyid, int size, int create){
	key_t key=keyid;
	void *mem;
	int perms=S_IRUSR|S_IWUSR;
	int fd=shmget(key,size,create ? (IPC_CREAT | perms ) : perms);
	if (fd==-1){
		printf("shmget failed: %s\n",strerror(errno));
		return NULL;
	}
	mem=shmat(fd,NULL,0);
	if (mem==(void*)-1){
		printf("shmat() failed: %s", strerror(errno));
		return NULL;
	}
	return mem;
}

void ortp_shm_close(void *mem){
	shmdt(mem);
}

#endif

#elif defined(_WIN32) && !defined(_WIN32_WCE)

static char *make_pipe_name(const char *name){
	return ortp_strdup_printf("\\\\.\\pipe\\%s",name);
}

static HANDLE event=NULL;

/* portable named pipes */
ortp_pipe_t ortp_server_pipe_create(const char *name){
#ifdef ORTP_WINDOWS_DESKTOP
	ortp_pipe_t h;
	char *pipename=make_pipe_name(name);
	h=CreateNamedPipe(pipename,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,PIPE_TYPE_MESSAGE|PIPE_WAIT,1,
						32768,32768,0,NULL);
	ortp_free(pipename);
	if (h==INVALID_HANDLE_VALUE){
		ortp_error("Fail to create named pipe %s",pipename);
	}
	if (event==NULL) event=CreateEvent(NULL,TRUE,FALSE,NULL);
	return h;
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return INVALID_HANDLE_VALUE;
#endif
}


/*this function is a bit complex because we need to wakeup someday
even if nobody connects to the pipe.
ortp_server_pipe_close() makes this function to exit.
*/
ortp_pipe_t ortp_server_pipe_accept_client(ortp_pipe_t server){
#ifdef ORTP_WINDOWS_DESKTOP
	OVERLAPPED ol;
	DWORD undef;
	HANDLE handles[2];
	memset(&ol,0,sizeof(ol));
	ol.hEvent=CreateEvent(NULL,TRUE,FALSE,NULL);
	ConnectNamedPipe(server,&ol);
	handles[0]=ol.hEvent;
	handles[1]=event;
	WaitForMultipleObjects(2,handles,FALSE,INFINITE);
	if (GetOverlappedResult(server,&ol,&undef,FALSE)){
		CloseHandle(ol.hEvent);
		return server;
	}
	CloseHandle(ol.hEvent);
	return INVALID_HANDLE_VALUE;
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return INVALID_HANDLE_VALUE;
#endif
}

int ortp_server_pipe_close_client(ortp_pipe_t server){
#ifdef ORTP_WINDOWS_DESKTOP
	return DisconnectNamedPipe(server)==TRUE ? 0 : -1;
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return -1;
#endif
}

int ortp_server_pipe_close(ortp_pipe_t spipe){
#ifdef ORTP_WINDOWS_DESKTOP
	SetEvent(event);
	//CancelIoEx(spipe,NULL); /*vista only*/
	return CloseHandle(spipe);
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return -1;
#endif
}

ortp_pipe_t ortp_client_pipe_connect(const char *name){
#ifdef ORTP_WINDOWS_DESKTOP
	char *pipename=make_pipe_name(name);
	ortp_pipe_t hpipe = CreateFile(
		 pipename,   // pipe name
		 GENERIC_READ |  // read and write access
		 GENERIC_WRITE,
		 0,              // no sharing
		 NULL,           // default security attributes
		 OPEN_EXISTING,  // opens existing pipe
		 0,              // default attributes
		 NULL);          // no template file
	ortp_free(pipename);
	return hpipe;
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return INVALID_HANDLE_VALUE;
#endif
}

int ortp_pipe_read(ortp_pipe_t p, uint8_t *buf, int len){
	DWORD ret=0;
	if (ReadFile(p,buf,len,&ret,NULL))
		return ret;
	/*ortp_error("Could not read from pipe: %s",strerror(GetLastError()));*/
	return -1;
}

int ortp_pipe_write(ortp_pipe_t p, const uint8_t *buf, int len){
	DWORD ret=0;
	if (WriteFile(p,buf,len,&ret,NULL))
		return ret;
	/*ortp_error("Could not write to pipe: %s",strerror(GetLastError()));*/
	return -1;
}


int ortp_client_pipe_close(ortp_pipe_t sock){
	return CloseHandle(sock);
}


typedef struct MapInfo{
	HANDLE h;
	void *mem;
}MapInfo;

static OList *maplist=NULL;

void *ortp_shm_open(unsigned int keyid, int size, int create){
#ifdef ORTP_WINDOWS_DESKTOP
	HANDLE h;
	char name[64];
	void *buf;

	snprintf(name,sizeof(name),"%x",keyid);
	if (create){
		h = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			size,                // maximum object size (low-order DWORD)
			name);                 // name of mapping object
	}else{
		h = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			name);               // name of mapping object
	}
	if (h==(HANDLE)-1) {
		ortp_error("Fail to open file mapping (create=%i)",create);
		return NULL;
	}
	buf = (LPTSTR) MapViewOfFile(h, // handle to map object
		FILE_MAP_ALL_ACCESS,  // read/write permission
		0,
		0,
		size);
	if (buf!=NULL){
		MapInfo *i=(MapInfo*)ortp_new(MapInfo,1);
		i->h=h;
		i->mem=buf;
		maplist=o_list_append(maplist,i);
	}else{
		CloseHandle(h);
		ortp_error("MapViewOfFile failed");
	}
	return buf;
#else
	ortp_error("%s not supported!", __FUNCTION__);
	return NULL;
#endif
}

void ortp_shm_close(void *mem){
#ifdef ORTP_WINDOWS_DESKTOP
	OList *elem;
	for(elem=maplist;elem!=NULL;elem=elem->next){
		MapInfo *i=(MapInfo*)elem->data;
		if (i->mem==mem){
			CloseHandle(i->h);
			UnmapViewOfFile(mem);
			ortp_free(i);
			maplist=o_list_remove_link(maplist,elem);
			return;
		}
	}
	ortp_error("No shared memory at %p was found.",mem);
#else
	ortp_error("%s not supported!", __FUNCTION__);
#endif
}


#endif


#ifdef __MACH__
#include <sys/types.h>
#include <sys/timeb.h>
#endif

void _ortp_get_cur_time(ortpTimeSpec *ret, bool_t realtime){
#if defined(_WIN32_WCE) || defined(_WIN32)
#ifdef ORTP_WINDOWS_DESKTOP
	DWORD timemillis;
#	if defined(_WIN32_WCE)
	timemillis=GetTickCount();
#	else
	timemillis=timeGetTime();
#	endif
	ret->tv_sec=timemillis/1000;
	ret->tv_nsec=(timemillis%1000)*1000000LL;
#else
	ULONGLONG timemillis = GetTickCount64();
	ret->tv_sec = timemillis / 1000;
	ret->tv_nsec = (timemillis % 1000) * 1000000LL;
#endif
#elif defined(__MACH__) && defined(__GNUC__) && (__GNUC__ >= 3)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ret->tv_sec=tv.tv_sec;
	ret->tv_nsec=tv.tv_usec*1000LL;
#elif defined(__MACH__)
	struct timeb time_val;

	ftime (&time_val);
	ret->tv_sec = time_val.time;
	ret->tv_nsec = time_val.millitm * 1000000LL;
#else
	struct timespec ts;
	if (clock_gettime(realtime ? CLOCK_REALTIME : CLOCK_MONOTONIC,&ts)<0){
		ortp_fatal("clock_gettime() doesn't work: %s",strerror(errno));
	}
	ret->tv_sec=ts.tv_sec;
	ret->tv_nsec=ts.tv_nsec;
#endif
}

void ortp_get_cur_time(ortpTimeSpec *ret){
	_ortp_get_cur_time(ret, FALSE);
}


uint64_t ortp_get_cur_time_ms(void) {
	ortpTimeSpec ts;
	ortp_get_cur_time(&ts);
	return (ts.tv_sec * 1000LL) + ((ts.tv_nsec + 500000LL) / 1000000LL);
}

void ortp_sleep_ms(int ms){
#ifdef _WIN32
#ifdef ORTP_WINDOWS_DESKTOP
	Sleep(ms);
#else
	HANDLE sleepEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	if (!sleepEvent) return;
	WaitForSingleObjectEx(sleepEvent, ms, FALSE);
	CloseHandle(sleepEvent);
#endif
#else
	struct timespec ts;
	ts.tv_sec=ms/1000;
	ts.tv_nsec=(ms%1000)*1000000LL;
	nanosleep(&ts,NULL);
#endif
}

void ortp_sleep_until(const ortpTimeSpec *ts){
#ifdef __linux
	struct timespec rq;
	rq.tv_sec=ts->tv_sec;
	rq.tv_nsec=ts->tv_nsec;
	while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &rq, NULL)==-1 && errno==EINTR){
	}
#else
	ortpTimeSpec current;
	ortpTimeSpec diff;
	_ortp_get_cur_time(&current, TRUE);
	diff.tv_sec=ts->tv_sec-current.tv_sec;
	diff.tv_nsec=ts->tv_nsec-current.tv_nsec;
	if (diff.tv_nsec<0){
		diff.tv_nsec+=1000000000LL;
		diff.tv_sec-=1;
	}
#ifdef _WIN32
		ortp_sleep_ms((int)((diff.tv_sec * 1000LL) + (diff.tv_nsec/1000000LL)));
#else
	{
		struct timespec dur,rem;
		dur.tv_sec=diff.tv_sec;
		dur.tv_nsec=diff.tv_nsec;
		while (nanosleep(&dur,&rem)==-1 && errno==EINTR){
			dur=rem;
		};
	}
#endif
#endif
}

#if defined(_WIN32) && !defined(_MSC_VER)
char* strtok_r(char *str, const char *delim, char **nextp){
	char *ret;

	if (str == NULL){
		str = *nextp;
	}
	str += strspn(str, delim);
	if (*str == '\0'){
		return NULL;
	}
	ret = str;
	str += strcspn(str, delim);
	if (*str){
		*str++ = '\0';
	}
	*nextp = str;
	return ret;
}
#endif


#if defined(_WIN32)

#if !defined(_MSC_VER)
#include <wincrypt.h>
static int ortp_wincrypto_random(unsigned int *rand_number){
	static HCRYPTPROV hProv=(HCRYPTPROV)-1;
	static int initd=0;

	if (!initd){
		if (!CryptAcquireContext(&hProv,NULL,NULL,PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)){
			ortp_error("ortp_wincrypto_random(): Could not acquire a windows crypto context");
			return -1;
		}
		initd=TRUE;
	}
	if (hProv==(HCRYPTPROV)-1)
		return -1;

	if (!CryptGenRandom(hProv,4,(BYTE*)rand_number)){
		ortp_error("ortp_wincrypto_random(): CryptGenRandom() failed.");
		return -1;
	}
	return 0;
}
#endif
#elif defined(__QNXNTO__) || ((defined(__linux) || defined(__APPLE__)) && !defined(HAVE_ARC4RANDOM))

static unsigned int ortp_urandom(void) {
	static int fd=-1;
	if (fd==-1) fd=open("/dev/urandom",O_RDONLY);
	if (fd!=-1){
		unsigned int tmp;
		if (read(fd,&tmp,4)!=4){
			ortp_error("Reading /dev/urandom failed.");
		}else return tmp;
	} else ortp_error("Could not open /dev/urandom");
	return (unsigned int) random();
}

#endif




unsigned int ortp_random(void){
#ifdef HAVE_ARC4RANDOM
#if defined(__QNXNTO__) // There is a false positive with blackberry build
	return ortp_urandom();
#else
	return arc4random();
#endif
#elif defined(__linux) || defined(__APPLE__)
	return ortp_urandom();
#elif defined(_WIN32)
	static int initd=0;
	unsigned int ret;
#ifdef _MSC_VER
	/*rand_s() is pretty nice and simple function but is not wrapped by mingw.*/

	if (rand_s(&ret)==0){
		return ret;
	}
#else
	if (ortp_wincrypto_random(&ret)==0){
		return ret;
	}
#endif
	/* Windows's rand() is unsecure but is used as a fallback*/
	if (!initd) {
		struct timeval tv;
		ortp_gettimeofday(&tv,NULL);
		srand((unsigned int)tv.tv_sec+tv.tv_usec);
		initd=1;
		ortp_warning("ortp: Random generator is using rand(), this is unsecure !");
	}
	return rand()<<16 | rand();
#endif
}

bool_t ortp_is_multicast_addr(const struct sockaddr *addr) {

	switch (addr->sa_family) {
		case AF_INET:
			return IN_MULTICAST(ntohl(((struct sockaddr_in *) addr)->sin_addr.s_addr));
		case AF_INET6:
			return IN6_IS_ADDR_MULTICAST(&(((struct sockaddr_in6 *) addr)->sin6_addr));
		default:
			return FALSE;
	}

}
