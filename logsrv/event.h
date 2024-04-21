#ifndef _EVENT_H
#define _EVENT_H

static const unsigned char io_read  = 0x01;
static const unsigned char io_write = 0x02;

typedef void (*func_type)(int,int,void*);

struct event
{
	int fd_;
	int flag_;
	void* arg_;
	func_type fun_;
	
	void set(int fd,int flag,func_type fun,void* arg)
	{
	   fd_  = fd, flag_ = flag;	 
	   fun_ = fun;
	   arg_ = arg;
	}
};

#endif
