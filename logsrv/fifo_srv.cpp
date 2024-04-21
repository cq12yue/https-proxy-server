#include "fifo_srv.h"
#include "select_reactor.h"
#include "../log/log_file.h"
#include "../log/log_type.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <time.h>
#include <stdexcept>
using namespace std;
using namespace ipc;

static const mode_t DEFAULT_FILE_MODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

extern logfile* g_lf;

fifo_base::fifo_base(select_reactor* sr)
:fio_(NULL)
,sr_(sr)
{
	fm_.data = NULL;
	to_head();
}

fifo_base::~fifo_base()
{
	if(fio_) fifo_close(fio_);
}

void fifo_base::async_read_msg()
{
	assert(fio_);

	for(;;){
		ssize_t num = read(fio_->fd,fm_.data+tran_,total_-tran_);
		if(num > 0) {
			tran_ += num;
			if(tran_==total_){
				switch(s_){
					case fifo_base::head:
						to_body();
						break;					

					case fifo_base::body:
						on_message(fm_.data,fm_.len);
						to_head();
						break;
				}
			}

		}else if(0==num||(errno!=EAGAIN&&errno!=EWOULDBLOCK)){
			on_error(0==num);
			break;
		}else
			break;
	}
}

void fifo_base::on_message(const void* data,size_t size)
{
}

void fifo_base::on_error(bool is_close)
{
}

void fifo_base::handle_read(int fd,int flag,void* arg)
{
	try{
		static_cast<fifo_base*>(arg)->async_read_msg();
	}catch(std::exception& se){
		log_printf(g_lf,"fifo base handle read: %s\n",se.what());
	}catch(...){
		log_printf(g_lf,"fifo base unknow exception\n");
	}
}

void fifo_base::to_head()
{ 
	if(fm_.data && fm_.data!=(char*)&fm_.len && fm_.data!=fast_buf_)
		delete []fm_.data;
	total_ = sizeof(fm_.len),tran_ = 0;
	fm_.data = (char*)&fm_.len;
	s_ = fifo_base::head;
}

void fifo_base::to_body()
{
	total_ = fm_.len,tran_ = 0;
	fm_.data = (total_ <= FAST_BUF_SIZE ? fast_buf_ : new char[total_]);
	s_ = fifo_base::body;
}

///////////////////////////////////////////////////////////////////////
//class client_list implement
client_list::client_list()
{
}

client_list::~client_list()
{
	for(set<fifo_client*>::iterator it = c_.begin();it!=c_.end();){
		delete *it;
		c_.erase(it++);
	}
}

void client_list::add(fifo_client* c)
{
	c_.insert(c);	
}

void client_list::remove(fifo_client* c)
{
	set<fifo_client*>::iterator it=c_.find(c);
	if(it!=c_.end())
		c_.erase(it);	
}

////////////////////////////////////////////////////////////////////
//class fifo_acceptor implement
fifo_acceptor::fifo_acceptor(select_reactor* sr)
:fifo_base(sr)
{
}

void fifo_acceptor::start(const char* name)
{
	if(!fifo_open(fio_,name,O_CREAT|O_RDWR|O_NONBLOCK,DEFAULT_FILE_MODE)){
		char buf[1024];	
		snprintf(buf,sizeof(buf),"file_acceptor start %s fail: errno=%d\n",name,errno);
		throw runtime_error(buf);
	}
	ev_.set(fio_->fd,io_read,handle_read,this);
	sr_->add_event(ev_);
}

void fifo_acceptor::on_message(const void* data,size_t size)
{
	printf("fifo_acceptor msg fd=%d,data=%s,size=%lu\n",fio_->fd,(const char*)data,size);	
	clients_.add(new fifo_client((const char*)data,this));
}

void fifo_acceptor::on_error(bool is_close)
{
	printf("fifo_acceptor %s\n",is_close ? "close" : "error");	
}

//////////////////////////////////////////////////////////////////////////////////
//class fifo_client implement	
fifo_client::fifo_client(const char* name,fifo_acceptor* own)
:fifo_base(own->sr_)
,own_(own)
{
	if(!fifo_open(fio_,name,O_RDONLY|O_NONBLOCK,0))	{
		char buf[1024];	
		snprintf(buf,sizeof(buf),"file_client constructor %s fail: errno=%d\n",name,errno);		 
		throw runtime_error(buf);
	}

	ev_.set(fio_->fd,io_read,handle_read,this);
	sr_->add_event(ev_);
}

fifo_client::~fifo_client()
{
	sr_->del_event(ev_);	
}

void fifo_client::on_message(const void* data,size_t size)
{
	printf("fifo_client::on_message fd=%d,size=%lu\n",fio_->fd,size);

	if(size >= sizeof(log_msg)){
		const log_msg* lm = reinterpret_cast<const log_msg*>(data);
		const log_header& hdr = lm->hdr;
		log_write(g_lf,hdr.level,hdr.now,hdr.tid,lm->data,size-sizeof(hdr));
	}
}

void fifo_client::on_error(bool is_close)
{
	if(is_close)
		printf("fifo_client fd=%d closed\n",fio_->fd);	
	else
		printf("fifo_client fd=%d errno=%d\n",fio_->fd,errno);

	own_->clients_.remove(this);
	delete this;
}

