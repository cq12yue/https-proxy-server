#include "log_file.h"
#include "log_type.h"
#include "../ipc/io_ext.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <pthread.h>
using namespace ipc;

/**************************************************************************************************
2013-11-6	modify return type of log write function from bool to ssize_t,
		add two kinds of log_write function with level,now and tid parameter,
		add log_trace,log_info,log_debug,log_warn,log_error and log_fatal functions.
***************************************************************************************************/

static const int MAX_LOG_SIZE	= 65536;
static const int FILE_FLAG_BASE = O_WRONLY|O_APPEND;
static const mode_t DEFAULT_FILE_MODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
static const size_t DEFAULT_RBSIZE = 1024*1024;

struct logfile
{
	int fd;
	char* name;
	size_t rbsize; //rollback size
};

#define LOG_XXX_IMPL(lf,level,fmt)\
	do{\
		struct timeval now;\
		gettimeofday(&now,NULL);\
		va_list vl;\
		va_start(vl,fmt);\
		ssize_t ret = __log_write(lf,level,now,pthread_self(),fmt,vl);\
		va_end(vl);\
		return ret;\
	}while(0)

static bool reopen(logfile *lf)
{
	struct stat st;
	if(-1==fstat(lf->fd,&st))
		return false;

	if(st.st_size > lf->rbsize){
		close(lf->fd);
		lf->fd = open(lf->name,FILE_FLAG_BASE|O_TRUNC,DEFAULT_FILE_MODE);
	}

	return -1 != lf->fd;
}

static ssize_t __log_write(logfile *lf,int level,const struct timeval &tv,unsigned long tid,const char *fmt,va_list vl)
{
	char buf[MAX_LOG_SIZE];
	int ret = vsnprintf(buf,sizeof(buf),fmt,vl);
	if(ret <= 0)
		return ret;
	return log_write(lf,level,tv,tid,buf,ret);
}

//////////////////////////////////////////////////////////////////////////
logfile* log_open(const char *name)
{
	logfile* lf = (logfile*)malloc(sizeof(logfile));
	if(lf){
		lf->fd = open(name,FILE_FLAG_BASE|O_CREAT,DEFAULT_FILE_MODE);
		if(-1==lf->fd)
			goto fail;

		lf->name = (char*)malloc(strlen(name)+1);
		if(NULL==lf->name){
			close(lf->fd);		
			goto fail;
		}

		strcpy(lf->name,name);
		lf->rbsize = DEFAULT_RBSIZE;
	}
	return lf;

fail:
	free(lf);
	return NULL;
}

void log_set_rbsize(logfile *lf,size_t size)
{
	assert(lf);
	lf->rbsize = size;
}

ssize_t log_write(logfile *lf,const void *buf,size_t len)
{
	assert(lf);

	if(!reopen(lf))
		return -1;

	ssize_t tran;
	writen(lf->fd,buf,len,&tran);
	return tran;
}

ssize_t log_writev(logfile *lf,const struct iovec *iov,int cnt)
{
	assert(lf);

	if(!reopen(lf))
		return -1;

	ssize_t tran;
	writevn(lf->fd,iov,cnt,&tran);
	return tran;
}

ssize_t log_printf(logfile *lf,const char *fmt,...)
{
	assert(lf);

	if(!reopen(lf))
		return -1;

	char buf[MAX_LOG_SIZE];
	va_list vl;
	va_start(vl,fmt);
	int ret = vsnprintf(buf,sizeof(buf),fmt,vl);
	va_end(vl);
	
	if(ret <= 0)  
		return ret;	

	ssize_t tran;
	writen(lf->fd,buf,ret,&tran);
	return tran;
}

ssize_t log_write(logfile *lf,int level,const struct timeval &tv,unsigned long tid,const void *data,size_t size)
{
	assert(LOG_LEVEL_TRACE<=level && level<=LOG_LEVEL_FATAL);

	char buf[256];
	struct tm* t = localtime(&tv.tv_sec);	
	size_t len = strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",t);
	int ret = snprintf(buf+len,sizeof(buf)-len,",%0.3f [%lu] %s -> ",
	 		   tv.tv_usec/1000.0,tid,log_level_text[level].str);
	if(ret <= 0)
		return ret;
	len += ret;

	struct iovec iov[3];
	iov[0].iov_base = buf;					iov[0].iov_len = len;
	iov[1].iov_base = (char*)data;	iov[1].iov_len = size;
	iov[2].iov_base = (char*)"\n";  iov[2].iov_len = 1;

	return log_writev(lf,iov,3);
}

ssize_t log_printf(logfile *lf,int level,const struct timeval &tv,unsigned long tid,const char *fmt,...)
{
	va_list vl;
	va_start(vl,fmt);
	ssize_t ret = __log_write(lf,level,tv,tid,fmt,vl);
	va_end(vl);
	return ret;
}

ssize_t log_trace(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_TRACE,fmt);
}

ssize_t log_info(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_INFO,fmt);
}

ssize_t log_debug(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_DEBUG,fmt);
}

ssize_t log_warn(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_WARN,fmt);
}

ssize_t log_error(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_ERROR,fmt);
}

ssize_t log_fatal(logfile *lf,const char *fmt,...)
{
	LOG_XXX_IMPL(lf,LOG_LEVEL_FATAL,fmt);
}

void log_close(logfile *lf)
{
	assert(lf);

	if(lf->fd!=-1)
		close(lf->fd);
	if(lf->name) 
		free(lf->name);
	free(lf);
}
