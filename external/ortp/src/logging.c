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
#include "utils.h"
#include <time.h>


typedef struct{
	char *domain;
	unsigned int logmask;
}OrtpLogDomain;

static void ortp_log_domain_destroy(OrtpLogDomain *obj){
	if (obj->domain) ortp_free(obj->domain);
	ortp_free(obj);
}

typedef struct _OrtpLogger{
	OrtpLogFunc logv_out;
	unsigned int log_mask; /*the default log mask, if no per-domain settings are found*/
	FILE *log_file;
	unsigned long log_thread_id;
	OList *log_stored_messages_list;
	OList *log_domains;
	ortp_mutex_t log_stored_messages_mutex;
	ortp_mutex_t domains_mutex;
}OrtpLogger;


static OrtpLogger __ortp_logger = { &ortp_logv_out, ORTP_WARNING|ORTP_ERROR|ORTP_FATAL, 0};

void ortp_init_logger(void){
	ortp_mutex_init(&__ortp_logger.domains_mutex, NULL);
}

void ortp_uninit_logger(void){
	ortp_mutex_destroy(&__ortp_logger.domains_mutex);
	__ortp_logger.log_domains = o_list_free_with_data(__ortp_logger.log_domains, (void (*)(void*))ortp_log_domain_destroy);
}

/**
*@param file a FILE pointer where to output the ortp logs.
*
**/
void ortp_set_log_file(FILE *file)
{
	__ortp_logger.log_file=file;
}

/**
*@param func: your logging function, compatible with the OrtpLogFunc prototype.
*
**/
void ortp_set_log_handler(OrtpLogFunc func){
	__ortp_logger.logv_out=func;
}

OrtpLogFunc ortp_get_log_handler(void){
	return __ortp_logger.logv_out;
}

static OrtpLogDomain * get_log_domain(const char *domain){
	OList *it;

	if (domain == NULL) return NULL;
	for (it = __ortp_logger.log_domains; it != NULL; it = it->next){
		OrtpLogDomain *ld = (OrtpLogDomain*)it->data;
		if (ld->domain && strcmp(ld->domain, domain) == 0 ){
			return ld;
		}
	}
	return NULL;
}

static OrtpLogDomain *get_log_domain_rw(const char *domain){
	OrtpLogDomain *ret;

	if (domain == NULL) return NULL;
	ret = get_log_domain(domain);
	if (ret) return ret;
	/*it does not exist, hence create it by taking the mutex*/
	ortp_mutex_lock(&__ortp_logger.domains_mutex);
	ret = get_log_domain(domain);
	if (!ret){
		ret = ortp_new0(OrtpLogDomain,1);
		ret->domain = ortp_strdup(domain);
		ret->logmask = __ortp_logger.log_mask;
		__ortp_logger.log_domains = o_list_prepend(__ortp_logger.log_domains, ret);
	}
	ortp_mutex_unlock(&__ortp_logger.domains_mutex);
	return ret;
}

/**
* @ param levelmask a mask of ORTP_DEBUG, ORTP_MESSAGE, ORTP_WARNING, ORTP_ERROR
* ORTP_FATAL .
**/
void ortp_set_log_level_mask(const char *domain, int levelmask){
	if (domain == NULL) __ortp_logger.log_mask=levelmask;
	else get_log_domain_rw(domain)->logmask = levelmask;
}


void ortp_set_log_level(const char *domain, OrtpLogLevel level){
	int levelmask = ORTP_FATAL;
	if (level<=ORTP_ERROR){
		levelmask |= ORTP_ERROR;
	}
	if (level<=ORTP_WARNING){
		levelmask |= ORTP_WARNING;
	}
	if (level<=ORTP_MESSAGE){
		levelmask |= ORTP_MESSAGE;
	}
	if (level<=ORTP_TRACE){
		levelmask |= ORTP_TRACE;
	}
	if (level<=ORTP_DEBUG){
		levelmask |= ORTP_DEBUG;
	}
	ortp_set_log_level_mask(domain, levelmask);
}

unsigned int ortp_get_log_level_mask(const char *domain) {
	OrtpLogDomain *ld;
	if (domain == NULL || (ld = get_log_domain(domain)) == NULL) return __ortp_logger.log_mask;
	else return ld->logmask;
}

void ortp_set_log_thread_id(unsigned long thread_id) {
	if (thread_id == 0) {
		ortp_logv_flush();
		ortp_mutex_destroy(&__ortp_logger.log_stored_messages_mutex);
	} else {
		ortp_mutex_init(&__ortp_logger.log_stored_messages_mutex, NULL);
	}
	__ortp_logger.log_thread_id = thread_id;
}

char * ortp_strdup_vprintf(const char *fmt, va_list ap)
{
/* Guess we need no more than 100 bytes. */
	int n, size = 200;
	char *p,*np;
#ifndef _WIN32
	va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
	if ((p = (char *) ortp_malloc (size)) == NULL)
		return NULL;
	while (1){
/* Try to print in the allocated space. */
#ifndef _WIN32
		va_copy(cap,ap);
		n = vsnprintf (p, size, fmt, cap);
		va_end(cap);
#else
		/*this works on 32 bits, luckily*/
		n = vsnprintf (p, size, fmt, ap);
#endif
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
//printf("Reallocing space.\n");
		/* Else try again with more space. */
		if (n > -1)	/* glibc 2.1 */
			size = n + 1;	/* precisely what is needed */
		else		/* glibc 2.0 */
			size *= 2;	/* twice the old size */
		if ((np = (char *) ortp_realloc (p, size)) == NULL)
		{
			free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}

char *ortp_strdup_printf(const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=ortp_strdup_vprintf(fmt, args);
	va_end (args);
	return ret;
}

char * ortp_strcat_vprintf(char* dst, const char *fmt, va_list ap){
	char *ret;
	size_t dstlen, retlen;

	ret=ortp_strdup_vprintf(fmt, ap);
	if (!dst) return ret;

	dstlen = strlen(dst);
	retlen = strlen(ret);

	if ((dst = ortp_realloc(dst, dstlen+retlen+1)) != NULL){
		strncat(dst,ret,retlen);
		dst[dstlen+retlen] = '\0';
		ortp_free(ret);
		return dst;
	} else {
		ortp_free(ret);
		return NULL;
	}
}

char *ortp_strcat_printf(char* dst, const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=ortp_strcat_vprintf(dst, fmt, args);
	va_end (args);
	return ret;
}

#if	defined(_WIN32) || defined(_WIN32_WCE)
#define ENDLINE "\r\n"
#else
#define ENDLINE "\n"
#endif

typedef struct {
	int level;
	char *msg;
	char *domain;
} ortp_stored_log_t;

void _ortp_logv_flush(int dummy, ...) {
	OList *elem;
	OList *msglist;
	va_list empty_va_list;
	va_start(empty_va_list, dummy);
	ortp_mutex_lock(&__ortp_logger.log_stored_messages_mutex);
	msglist = __ortp_logger.log_stored_messages_list;
	__ortp_logger.log_stored_messages_list = NULL;
	ortp_mutex_unlock(&__ortp_logger.log_stored_messages_mutex);
	for (elem = msglist; elem != NULL; elem = o_list_next(elem)) {
		ortp_stored_log_t *l = (ortp_stored_log_t *)elem->data;
#ifdef _WIN32
		__ortp_logger.logv_out(l->domain, l->level, l->msg, empty_va_list);
#else
		va_list cap;
		va_copy(cap, empty_va_list);
		__ortp_logger.logv_out(l->domain, l->level, l->msg, cap);
		va_end(cap);
#endif
		if (l->domain) ortp_free(l->domain);
		ortp_free(l->msg);
		ortp_free(l);
	}
	o_list_free(msglist);
	va_end(empty_va_list);
}

void ortp_logv_flush(void) {
	_ortp_logv_flush(0);
}

void ortp_logv(const char *domain, OrtpLogLevel level, const char *fmt, va_list args) {
	if ((__ortp_logger.logv_out != NULL) && ortp_log_level_enabled(domain, level)) {
		if (__ortp_logger.log_thread_id == 0) {
			__ortp_logger.logv_out(domain, level, fmt, args);
		} else if (__ortp_logger.log_thread_id == ortp_thread_self()) {
			ortp_logv_flush();
			__ortp_logger.logv_out(domain, level, fmt, args);
		} else {
			ortp_stored_log_t *l = ortp_new(ortp_stored_log_t, 1);
			l->domain = domain ? ortp_strdup(domain) : NULL;
			l->level = level;
			l->msg = ortp_strdup_vprintf(fmt, args);
			ortp_mutex_lock(&__ortp_logger.log_stored_messages_mutex);
			__ortp_logger.log_stored_messages_list = o_list_append(__ortp_logger.log_stored_messages_list, l);
			ortp_mutex_unlock(&__ortp_logger.log_stored_messages_mutex);
		}
	}
#if !defined(_WIN32_WCE)
	if (level == ORTP_FATAL) {
		ortp_logv_flush();
		abort();
	}
#endif
}

/*This function does the default formatting and output to file*/
void ortp_logv_out(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args){
	const char *lname="undef";
	char *msg;
	struct timeval tp;
	struct tm *lt;
#ifndef _WIN32
	struct tm tmbuf;
#endif
	time_t tt;
	ortp_gettimeofday(&tp,NULL);
	tt = (time_t)tp.tv_sec;

#ifdef _WIN32
	lt = localtime(&tt);
#else
	lt = localtime_r(&tt,&tmbuf);
#endif

	if (__ortp_logger.log_file==NULL) __ortp_logger.log_file=stderr;
	switch(lev){
		case ORTP_DEBUG:
			lname = "debug";
		break;
		case ORTP_MESSAGE:
			lname = "message";
		break;
		case ORTP_WARNING:
			lname = "warning";
		break;
		case ORTP_ERROR:
			lname = "error";
		break;
		case ORTP_FATAL:
			lname = "fatal";
		break;
		default:
			lname = "badlevel";
	}

	msg=ortp_strdup_vprintf(fmt,args);
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#ifndef _UNICODE
	OutputDebugStringA(msg);
	OutputDebugStringA("\r\n");
#else
	{
		size_t len=strlen(msg);
		wchar_t *tmp=(wchar_t*)ortp_malloc0((len+1)*sizeof(wchar_t));
		mbstowcs(tmp,msg,len);
		OutputDebugStringW(tmp);
		OutputDebugStringW(L"\r\n");
		ortp_free(tmp);
	}
#endif
#endif
	fprintf(__ortp_logger.log_file,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i ortp-%s-%s" ENDLINE
		,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
		,(int)(tp.tv_usec/1000), lname, msg);
	fflush(__ortp_logger.log_file);

	// fatal messages should go to stderr too, even if we are using a file since application will abort right after
	if (__ortp_logger.log_file != stderr && lev == ORTP_FATAL) {
		fprintf(stderr,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i ortp-%s-%s" ENDLINE
			,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
			,(int)(tp.tv_usec/1000), lname, msg);
		fflush(stderr);
	}

	ortp_free(msg);
}


#ifdef __QNX__
#include <slog2.h>

static bool_t slog2_registered = FALSE;
static slog2_buffer_set_config_t slog2_buffer_config;
static slog2_buffer_t slog2_buffer_handle[2];

void ortp_qnx_log_handler(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args) {
	uint8_t severity = SLOG2_DEBUG1;
	uint8_t buffer_idx = 1;
	char* msg;

	if (slog2_registered != TRUE) {
		slog2_buffer_config.buffer_set_name = domain;
		slog2_buffer_config.num_buffers = 2;
		slog2_buffer_config.verbosity_level = SLOG2_DEBUG2;
		slog2_buffer_config.buffer_config[0].buffer_name = "hi_rate";
		slog2_buffer_config.buffer_config[0].num_pages = 6;
		slog2_buffer_config.buffer_config[1].buffer_name = "lo_rate";
		slog2_buffer_config.buffer_config[1].num_pages = 2;
		if (slog2_register(&slog2_buffer_config, slog2_buffer_handle, 0) == 0) {
			slog2_registered = TRUE;
		} else {
			fprintf(stderr, "Error registering slogger2 buffer!\n");
			return;
		}
	}

	switch(lev){
		case ORTP_DEBUG:
			severity = SLOG2_DEBUG1;
		break;
		case ORTP_MESSAGE:
			severity = SLOG2_INFO;
		break;
		case ORTP_WARNING:
			severity = SLOG2_WARNING;
		break;
		case ORTP_ERROR:
			severity = SLOG2_ERROR;
		break;
		case ORTP_FATAL:
			severity = SLOG2_CRITICAL;
		break;
		default:
			severity = SLOG2_CRITICAL;
	}

	msg = ortp_strdup_vprintf(fmt,args);
	slog2c(slog2_buffer_handle[buffer_idx], 0, severity, msg);
}
#endif /* __QNX__ */
