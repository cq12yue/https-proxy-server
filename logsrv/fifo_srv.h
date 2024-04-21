#ifndef _FIFO_SRV_H
#define _FIFO_SRV_H

#include "../ipc/fifo_io.h"
#include "event.h"
#include <set>

class select_reactor;

class fifo_base
{
	enum { FAST_BUF_SIZE = 64*1024 }; 
protected:
	fifo_base(select_reactor* sr);
	virtual ~fifo_base();
	
	void async_read_msg();
	virtual void on_message(const void* data,size_t size);
	virtual void on_error(bool is_close);
	
	static void handle_read(int fd,int flag,void* arg);		
	
protected:
	ipc::fifo* fio_;
	event ev_;
	select_reactor* sr_;
	
private:
	void to_head();
	void to_body();
		
private:	
	enum {head,body} s_;
	size_t tran_,total_;
	ipc::fifo_msg fm_;
	char fast_buf_[FAST_BUF_SIZE];
};

class fifo_client;

class client_list
{
public:
	client_list();
	~client_list();
	
	void add(fifo_client* c);
	void remove(fifo_client* c);
	
private:
	std::set<fifo_client*> c_;		
};

class fifo_acceptor : public fifo_base
{
	friend class fifo_client;
public:
	fifo_acceptor(select_reactor* sr);
	
	void start(const char* name);	

protected:
	void on_message(const void* data,size_t size);
	void on_error(bool is_close);

private:
	client_list clients_;
};

class fifo_client : public fifo_base
{
public:
	fifo_client(const char* name,fifo_acceptor* own);
	~fifo_client();	

protected:
	void on_message(const void* data,size_t size);
	void on_error(bool is_close);

private:
	fifo_acceptor* own_;
};

#endif
