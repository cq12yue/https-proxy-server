#include "log_file.h"
#include "log_type.h"
#include "../ipc/ipc_io_ext.h"
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

/**************************************************************************************************
2013-11-6	modify return type of log write function from bool to ssize_t,
		add two kinds of log_write function with level,now and tid parameter,
		add log_trace,log_info,log_debug,log_warn,log_error and log_fatal functions.
***************************************************************************************************/

static const int MAX_LOG_SIZE	= 65536;
static const int FILE_FLAG_BASE = O_WRONLY|O_APPEND;
static const mode_t DEFAULT_FILE_MODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
static const size_t DEFAULT_RBSIZE = 1024*1024;

struct log_s
{
	int fd;
	char *name;
	size_t rbsize; //rollback size
	int lrange; //level range belong to interval [0,6]
};

static int reopen(log_t log)
{
	struct stat st;
	if(-1==fstat(log->fd,&st))
		return 0;

	if(st.st_size > log->rbsize){
		close(log->fd);
		log->fd = open(log->name,FILE_FLAG_BASE|O_TRUNC,DEFAULT_FILE_MODE);
	}

	return -1 != log->fd;
}

static ssize_t log_writev(log_t log,const struct iovec *iov,int cnt)
{
	if(log->name && !reopen(log))
		return -1;

	ssize_t tran;
	ipc_writevn(log->fd,iov,cnt,&tran);
	return tran;
}

static ssize_t log_write(log_t log,int level,const struct timeval *tv,unsigned int pid,unsigned long tid,const void *data,size_t size)
{
	char buf[256];
	struct tm t;
	localtime_r(&tv->tv_sec,&t);	
	size_t len = strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&t);
	int ret = snprintf(buf+len,sizeof(buf)-len,",%0.3f [%u-%lu] %s -> ", tv->tv_usec/1000.0,pid,tid,log_level_text[level].str);
	if(ret <= 0)
		return ret;
	len += ret;

	struct iovec iov[3];
	iov[0].iov_base = buf;		iov[0].iov_len = len;
	iov[1].iov_base = (char*)data;	iov[1].iov_len = size;
	iov[2].iov_base = (char*)"\n";  iov[2].iov_len = 1;

	return log_writev(log,iov,3);
}

#define LOG_XXX_IMPL(log,level,fmt)\
	do{\
		assert(log);\
		if(level<(log->lrange&0x000000FF) || level>((log->lrange>>8)&0x000000FF))\
			return 0;\
		char buf[MAX_LOG_SIZE];\
		struct timeval now;\
		gettimeofday(&now,NULL);\
		va_list vl;\
		va_start(vl,fmt);\
		int ret = vsnprintf(buf,sizeof(buf),fmt,vl);\
		va_end(vl);\
		if(ret <= 0)\
			return ret;\
		return log_write(log,level,&now,getpid(),pthread_self(),buf,ret);\
	}while(0)

///////////////////////////////////////////////////////////////////////////////////////////////////
log_t log_open(const char *name)
{
	log_t log = (log_t)malloc(sizeof(struct log_s));
	if(log){
		if(NULL==name){
			log->fd = STDOUT_FILENO;
			log->name = NULL;
			log->rbsize = 0;
		}else{
			log->fd = open(name,FILE_FLAG_BASE|O_CREAT,DEFAULT_FILE_MODE);
			if(-1==log->fd)
				goto fail;
	
			log->name = (char*)malloc(strlen(name)+1);
			if(NULL==log->name){
				close(log->fd);		
				goto fail;
			}
	
			strcpy(log->name,name);
			log->rbsize = DEFAULT_RBSIZE;
		}
		//default all level
		log->lrange = (LOG_LEVEL_FATAL << 8) | LOG_LEVEL_TRACE; 
	}
	
	return log;

fail:
	free(log);
	return NULL;
}

void log_set_rbsize(log_t log,size_t size)
{
	assert(log);
	if(log->name)
		log->rbsize = size;
}

void log_set_level_range(log_t log,int lrange)
{
	assert(log);
	log->lrange = lrange;
}

void log_close(log_t log)
{
	assert(log);

	if(log->fd!=-1)
		close(log->fd);
	if(log->name) 
		free(log->name);
		
	free(log);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
ssize_t log_trace(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_TRACE,fmt);
}

ssize_t log_info(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_INFO,fmt);
}

ssize_t log_debug(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_DEBUG,fmt);
}

ssize_t log_warn(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_WARN,fmt);
}

ssize_t log_error(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_ERROR,fmt);
}

ssize_t log_fatal(log_t log,const char *fmt,...)
{
	LOG_XXX_IMPL(log,LOG_LEVEL_FATAL,fmt);
}
