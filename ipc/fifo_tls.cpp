#include "fifo_tls.h"
#include "fifo_io.h"
#include "io_ext.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <errno.h>
using namespace ipc;

static const int max_buf_size  = 65536;
static const mode_t default_file_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static pthread_key_t s_key;
static int s_errno;

static void delete_key(void* ptr)
{
	fifo_close(static_cast<fifo*>(ptr));
}

static void create_key()
{
	s_errno = pthread_key_create(&s_key,delete_key);
}

//////////////////////////////////////////////////////////////////////////////////
static char s_srv_fifo_path[PATH_MAX];

static fifo* tls_fifo_create()
{
	fifo* f = NULL;
	if(!fifo_open(f,s_srv_fifo_path,O_WRONLY|O_NONBLOCK,0))
		return NULL;

	fifo* fio = NULL;
	int ret = fcntl(f->fd,F_GETFL);
	if (ret < 0)
		goto fail;

	ret &= ~O_NONBLOCK;
	if(-1 == fcntl(f->fd,F_SETFL,ret))
		goto fail;

	if(-1==mkdir("/tmp/fifo",0) && EEXIST!=errno)
		goto fail;

	char path[PATH_MAX];	
	ret = snprintf(path,sizeof(path),"/tmp/fifo/%u-%lu",(unsigned int)getpid(),(unsigned long)pthread_self());
	fio = fifo_make(path,default_file_mode);
	if(NULL==fio)
		goto fail;

	if(!fifo_write_msg(f,path,ret+1))
		goto fail;

	if(!fifo_open(fio,path,O_WRONLY,0))
		goto fail;

	if(pthread_setspecific(s_key,fio))
		goto fail;

	goto success;

fail:
	if(fio) {
		fifo_close(fio); 
		fio = NULL;
	}

success:
	fifo_close(f);	
	return fio;
}

static void tls_fifo_destroy(fifo* f)
{
	fifo_close(f);
	pthread_setspecific(s_key,NULL);
}

//////////////////////////////////////////////////////////////////////////
bool ipc::tls_fifo_init(const char* srv_fifo_path)
{
	pthread_once(&key_once,create_key);
	if(s_errno) return false;

	size_t len = strlen(srv_fifo_path);
	if(len >= PATH_MAX)
		return false;

	strcpy(s_srv_fifo_path,srv_fifo_path);
	return true;
}

fifo* ipc::tls_fifo_get()
{
	fifo* fio = static_cast<fifo*>(pthread_getspecific(s_key));
	if(NULL==fio)
		fio = tls_fifo_create();
	return fio;
}

ssize_t ipc::tls_fifo_write(const void* buf,size_t size)
{
	fifo* fio = tls_fifo_get();
	if(NULL==fio) 
		return false;

	ssize_t ret = write(fio->fd,buf,size);
	if(-1==ret && EPIPE==errno)
		tls_fifo_destroy(fio);
	return ret;
}

ssize_t ipc::tls_fifo_fwrite(const char* fmt,...)
{
	char buf[max_buf_size];

	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf,sizeof(buf),fmt,ap);
	va_end(ap);
	if(ret < 0)  
		return -1;	
	return tls_fifo_write(buf,ret);
}

bool ipc::tls_fifo_writevn(const struct iovec* iov,int iovcnt,ssize_t* tran/* = NULL*/)
{
	fifo* fio = tls_fifo_get();
	if(NULL==fio)
		return false;

	bool ret = writevn(fio->fd,iov,iovcnt,tran);
	if(!ret && EPIPE==errno)
		tls_fifo_destroy(fio);
	return ret;
}

bool ipc::tls_fifo_write_msg(const void* buf,size_t size)
{
	struct iovec iov[2];
	iov[0].iov_base = (char*)&size; iov[0].iov_len = sizeof(size);
	iov[1].iov_base = (char*)buf;   iov[1].iov_len = size;
	
	return tls_fifo_writevn(iov,2);
}

bool ipc::tls_fifo_fwrite_msg(const char* fmt,...)
{
	char buf[max_buf_size];

	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf,sizeof(buf),fmt,ap);
	va_end(ap);
	if(ret < 0)  
		return false;	
	
	return tls_fifo_write_msg(buf,ret);
}
