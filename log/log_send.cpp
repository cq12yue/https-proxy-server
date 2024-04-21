#include "log_send.h"
#include "log_type.h"
#include "../ipc/fifo_tls.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/uio.h>
#include <stdarg.h>
using namespace ipc;

#define LOG_XXX_IMPL(level,fmt) \
	do{\
		va_list ap;\
		va_start(ap, fmt);\
		bool ret = log_send(level,fmt,ap);\
		va_end(ap);\
		return ret;\
	}while(0)

#define MAX_MSG_LEN 65536

static bool log_send(int level,const char* fmt,va_list vl)
{
	char buf[MAX_MSG_LEN];
	int ret = vsnprintf(buf,sizeof(buf),fmt,vl);
	if(ret <= 0)
		return false;

	log_header hdr;
	hdr.level = level;
	gettimeofday(&hdr.now,NULL);
	hdr.tid = (unsigned long)pthread_self();

	size_t len = sizeof(hdr)+ret;
	
	struct iovec iov[3];
	iov[0].iov_base = &len; iov[0].iov_len = sizeof(len);
	iov[1].iov_base = &hdr; iov[1].iov_len = sizeof(hdr);
	iov[2].iov_base = buf;  iov[2].iov_len = ret;

	return tls_fifo_writevn(iov,3);
}

bool log_init(const char* srv_fifo_path)
{
	return tls_fifo_init(srv_fifo_path);
}

bool log_trace(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_TRACE,fmt);
}

bool log_info(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_INFO,fmt);
}

bool log_debug(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_DEBUG,fmt);
}

bool log_warn(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_WARN,fmt);
}

bool log_error(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_ERROR,fmt);
}

bool log_fatal(const char* fmt,...)
{
	LOG_XXX_IMPL(LOG_LEVEL_FATAL,fmt);
}
