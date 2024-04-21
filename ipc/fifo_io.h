#ifndef _IPC_FIFO_IO_H
#define _IPC_FIFO_IO_H

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

struct iovec;

namespace ipc 
{
	struct fifo
	{
		int   fd;
		char* name;
	};

	struct fifo_msg
	{
		size_t len;
		size_t tran;
		char* data;
	};

	fifo* fifo_make(const char* path,mode_t mode);

	bool fifo_open(fifo*& f,const char* path,int flag,mode_t mode);

	bool fifo_write_msg(fifo* f,const fifo_msg* fm,ssize_t* tran=NULL);

	bool fifo_write_msg(fifo* f,const void* data,size_t size,ssize_t* tran=NULL);

	bool fifo_read_msg(fifo* f,fifo_msg* fm);

	void fifo_close(fifo* f);
}

#endif
