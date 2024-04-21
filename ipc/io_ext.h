#ifndef _IPC_IO_EXT_H
#define _IPC_IO_EXT_H

#include <unistd.h>

struct iovec;

namespace ipc
{
	bool readn(int fd,void* buf,size_t cnt,ssize_t* tran=NULL);
	bool readvn(int fd,const struct iovec* iov,int iovcnt,ssize_t* tran=NULL);
	
	bool writen(int fd,const void* buf,size_t cnt,ssize_t* tran=NULL);
	bool writevn(int fd,const struct iovec* iov,int iovcnt,ssize_t* tran=NULL);
}

#endif
