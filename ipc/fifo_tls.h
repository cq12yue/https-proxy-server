#ifndef _IPC_FIFO_TLS_H
#define _IPC_FIFO_TLS_H

#include <stddef.h>
#include <unistd.h>

struct iovec;

namespace ipc
{
	struct fifo;

	bool tls_fifo_init(const char* srv_fifo_path);

	fifo* tls_fifo_get();
	ssize_t tls_fifo_write(const void* buf,size_t size);
	ssize_t tls_fifo_fwrite(const char* fmt,...);

	bool tls_fifo_writevn(const struct iovec* iov,int iovcnt,ssize_t* tran = NULL);
	bool tls_fifo_write_msg(const void* buf,size_t size);
	bool tls_fifo_fwrite_msg(const char* fmt,...);
}

#endif 
