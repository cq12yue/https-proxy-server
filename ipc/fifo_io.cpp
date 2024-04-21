#include "fifo_io.h"
#include "io_ext.h"
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
using namespace ipc;

fifo* ipc::fifo_make(const char* path,mode_t mode)
{
	int ret,mask = 0;
	
	char* p = strrchr((char*)path,'/'),*dir = NULL;
	fifo* f = NULL;
		
	if(p){
		size_t n = p-path+1;
		dir = (char*)malloc(n+1);
		if(NULL==dir) return NULL; //fast return,need not goto fail
	
		strncpy(dir,path,n);
		dir[n] = '\0';
		ret = mkdir(dir,0);
		if(0==ret)
			mask |= 0x01; //mkdir success
		else if(EEXIST!=errno)
			goto fail;
	}
	ret = mkfifo(path,mode);
	if(0==ret)
		mask |= 0x02;	//mkfifo success
	else if(EEXIST!=errno)
		goto fail;
			
	f = (fifo*)malloc(sizeof(fifo));
	if(NULL==f)
		goto fail;
		
	f->name = (char*)malloc(strlen(path)+1);
	if(NULL==f->name)
		goto fail;
	
	strcpy(f->name,path); f->fd = -1;
	goto success;
	
fail:
	if(mask&0x02) 
		unlink(path);
		
	if(mask&0x01)
		rmdir(dir);
	
	if(f){
		free(f); f = NULL;
	}
		
success:
	if(dir) free(dir);
	return f;
}

/**
 	If fail,the function should not change the first parameter f and close the fifo which been created by itself.
 */
bool ipc::fifo_open(fifo*& f,const char* path,int flag,mode_t mode)
{
	fifo* fio = NULL;
	bool is_creat = flag&O_CREAT;
	
	if(is_creat){
		fio = fifo_make(path,mode);
		if(NULL==fio)
			return false;
		flag &= ~O_CREAT;
		
	}else if(NULL==f){
		fio = (fifo*)malloc(sizeof(fifo));
		if(NULL==fio)
			return false;
		fio->name = NULL;
		fio->fd = -1;
	}
	
	int fd = open(path,flag,0);
	if(fd < 0){
		if(fio) fifo_close(fio);
		return false;
	}
	if(fio) f = fio;
	f->fd = fd;
	return true;
}

/**
	Only for block write(f->fd & O_NONBLOCK must be zero),if FIFO has no sufficient space for writing,
	then write will be block.
 */
bool ipc::fifo_write_msg(fifo* f,const fifo_msg* fm,ssize_t* tran/*=NULL*/)
{
	assert(fm && f);	
	return fifo_write_msg(f,fm->data,fm->len);
}

bool ipc::fifo_write_msg(fifo* f,const void* data,size_t size,ssize_t* tran/*=NULL*/)
{
	assert(f);

	struct iovec iov[2];
	iov[0].iov_base = (char*)&size;
	iov[0].iov_len = sizeof(size);
	iov[1].iov_base = (char*)data;
	iov[1].iov_len = size;

	return writevn(f->fd,iov,2,tran);	
}

/**
	Only for block read(f->fd & O_NONBLOCK must be zero)
 */
bool ipc::fifo_read_msg(fifo* f,fifo_msg* fm)
{
	assert(f&&fm);

	if(!readn(f->fd,&fm->len,sizeof(fm->len)))
		return false;

	fm->data = (char*)malloc(fm->len);
	if(NULL==fm->data)
		return false;
	
	if(!readn(f->fd,fm->data,fm->len)){
		free(fm->data);
		return false;
	}

	return true;
}

void ipc::fifo_close(fifo* f)
{
	assert(f);

	if(f->name){
		unlink(f->name);
		free(f->name);
	}
	if(f->fd!=-1)
		close(f->fd);
	free(f);
}
