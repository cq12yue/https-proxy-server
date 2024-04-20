#include "connector.h"
#include "global.h"
#include "p2p_proxy.h"
#include "../log/log_send.h"
#include "../net/function.h"
#if DEBUG
#include "../base/timeval.h"
using namespace base;
#endif
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <cassert>
#ifdef __x86_64__
#include <unistd.h>
#endif

/***************** ChangeLog **************************************************************************
2012-8-23	Use edge-triggered instead of level-triggered,combine ev_et flag with ev_read or ev_write,
			otherwise write event will be triggered ceaselessly,cause high cpu usage. 

2012-11-8	add include unistd header file to support 64 bit compile
2013-6-8	optimize pipe notice to send data
2013-6-18	add event_base param for open,open_sock and open_pipe methods 
 *****************************************************************************************************/
static const int KEEPALIVE_IDLE = 60;
static const int KEEPALIVE_INTV = 5;
static const int KEEPALIVE_CNT  = 3;

inline const char* get_state_text(connector::state s)
{
	assert(connector::disconnected <= s && s <= connector::connected);
	static const char* text[] ={"disconnected","connecting","connected"};
	return text[s];
}

connector::connector()
:state_(disconnected)
,pipe_tran_(0)
{
}

int connector::open(const netcomm::net_addr& addr,event_base* base)
{
	addr_ = addr;
	int af = addr_.family();

	if(AF_INET!=af && AF_INET6!=af)	{		
		log_error("connector::open only support AF_INET and AF_INET6 address family");			
		return failed;
	}
	return open(base);
}

bool connector::async_send(const void* data,size_t len)
{
	assert(sizeof(intptr_t)==sizeof(buffer*));

	buffer* buf = new (len) buffer(data,len);
	intptr_t val = reinterpret_cast<intptr_t>(buf);

	ssize_t ret = write(fd_[1],&val,sizeof(val));
	if(ret == sizeof(val))
		return true;

	delete buf;
	log_fatal("connector::async_send write pipe fail: ret=%d,errno=%d",ret,errno);
	return false;
}

void connector::send(const void* data,size_t len)
{
	buffer* buf = new (len) buffer(data,len);
	out_queue_.push(buf);
	async_send();
}

void connector::close()
{
	close_sock();
	close_pipe();
}

//////////////////////////////////////////////////////////////////////////
int connector::open_sock(event_base* base)
{
	assert(disconnected==state_);
	assert(base);
	
	sock_ = socket(addr_.family(),SOCK_STREAM,0);
	if(-1==sock_) {
		log_fatal("connector::open_sock socket fail: errno = %d",errno);		
		return state_ = disconnected;
	}
	
	unsigned char mask = 0;
	int ret;
	struct timeval tv;

	if(!netcomm::set_block_attr(sock_,false)){
		log_fatal("connector::open_sock set_block_attr fail: errno = %d",errno);	
		goto fail;
	}
	if(!netcomm::enable_keepalive(sock_,KEEPALIVE_IDLE,KEEPALIVE_INTV,KEEPALIVE_CNT)){
		log_fatal("connector::open_sock enable_keepalive fail: errno = %d",errno);	
		goto fail;
	}
		
	ret = connect(sock_,addr_,addr_.length());
	if(-1==ret && errno != EINPROGRESS){
		log_fatal("connector::open_sock connect fail: errno = %d",errno);		
		goto fail;
	}
	if(event_assign(&ev_read_,base,sock_,EV_READ|EV_PERSIST|EV_ET,handle_read,this)){
		log_error("connector::open_sock event_assign ev_read_ fail");		
		goto fail;
	}
	if(event_add(&ev_read_,NULL)){
		log_error("connector::open_sock event_add ev_read_ fail");	
		goto fail;
	}
	mask |= 0x01;

	if(event_assign(&ev_write_,base,sock_,EV_WRITE|EV_PERSIST|EV_ET,handle_write,this)){
		log_error("connector::open_sock event_assign ev_write_ fail");		
		goto fail;
	}
	if(event_add(&ev_write_,NULL)){
		log_error("connector::open_sock event_add ev_write_ fail");		
		goto fail;
	}
	mask |= 0x02;

	if(0==ret) return state_ = connected;

	if(evtimer_assign(&ev_timer_,base,handle_connect_timeout,this)){
		log_error("connector::open_sock event_assign ev_timer_ fail");		
		goto fail;
	}
	tv.tv_sec = g_conf.timeout_respond_, tv.tv_usec = 0;
	if(evtimer_add(&ev_timer_,&tv)){
		log_error("connector::open_sock event_add ev_timer_ fail");		
		goto fail;
	}
	return state_ = connecting;

fail:
	if(mask & 0x02)
		event_del(&ev_write_);
	if(mask & 0x01)
		event_del(&ev_read_);
	::close(sock_);

	return state_ = disconnected;
}

bool connector::open_pipe(event_base* base)
{
	assert(base);

	if(::pipe(fd_)) return false;

	if(!netcomm::set_block_attr(fd_[0],false)){
		log_fatal("connector::open_pipe set_block_attr fail: errno = %d",errno);	
		goto fail;
	}
	if(event_assign(&ev_notify_,base,fd_[0],EV_READ|EV_PERSIST|EV_ET,handle_notify,this)){
		log_error("connector::open_pipe event_assign fail");		
		goto fail;
	}
	if(event_add(&ev_notify_,NULL)){
		log_error("connector::open_pipe event_add fail");		
		goto fail;
	}
	return true;

fail:
	::close(fd_[0]); ::close(fd_[1]);
	return false;
}

int connector::open(event_base* base)
{
	if(!open_pipe(base)) 
		return failed;
	return open_sock(base);
}

void connector::reopen_sock()
{
	close_sock();	
	if(!out_queue_.empty())
		open_sock(event_get_base(&ev_notify_));
}

void connector::close_pipe()
{
	event_del(&ev_notify_);
	::close(fd_[0]); ::close(fd_[1]);
}

void connector::close_sock()
{
	if(disconnected==state_) 
		return;
		
	event_del(&ev_read_);
	event_del(&ev_write_);
	if(connecting==state_)
		event_del(&ev_timer_);
	
	::close(sock_);
	
	state_ = disconnected;
}

void connector::async_send()
{
	ssize_t ret; buffer* buf;

#if DEBUG
	static __thread struct timeval tv_beg;
	struct timeval tv_end, tv_diff;
#endif

	while(!out_queue_.empty()) {
		buf = out_queue_.top();
#if DEBUG
		if(0==buf->dtran_)
			gettimeofday(&tv_beg,NULL);
#endif
		ret = ::send(sock_,buf->data_+buf->dtran_,buf->dlen_-buf->dtran_,0);

		if(ret > 0) {
			buf->dtran_ += ret;
			if(buf->dtran_ == buf->dlen_) {
				log_debug("connector::async_send finish");
				out_queue_.pop();
				delete buf;
#if DEBUG
				gettimeofday(&tv_end,NULL);
				tv_diff = tv_end - tv_beg;
				log_debug("connector send one packet use %0.3f ms",tv_diff.tv_sec*1000+tv_diff.tv_usec/1000.0);
#endif
			}
		}else if(ret < 0 && (errno == EAGAIN||errno == EWOULDBLOCK)){
			break;
		}else{ //the connection had occurred error or been closed
			log_fatal("connector::async_send fail: peer %s,errno=%d",ret==0?"close":"error",errno);
		
			if(buf->dtran_) {
				out_queue_.pop(); delete buf;		
			}
			reopen_sock();
			log_info("connector::async_send state is %s",get_state_text(state_));
			break;
		}
	}
}

void connector::handle_notify()
{
	ssize_t ret;
	for (;;) {
		ret = read(fd_[0], pipe_buf_+pipe_tran_,PIPE_MSG_SIZE-pipe_tran_);
		if(ret > 0){
			pipe_tran_ += ret;
			if(pipe_tran_==PIPE_MSG_SIZE){
				pipe_tran_ = 0;
				buffer* buf = reinterpret_cast<buffer*>(*((intptr_t*)pipe_buf_));
				out_queue_.push(buf);
				log_debug("connector::handle_notify new");

				if(connector::disconnected==state_){
					open_sock(event_get_base(&ev_notify_));
					log_info("connector::handle_notify: state is %s",get_state_text(state_));
				}
				if(connector::connected==state_)
					async_send();	
			}
		}else if(ret < 0 && (errno == EAGAIN||errno == EWOULDBLOCK)){
			break;	
		}else
			break;
	}	
}

void connector::handle_notify(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_READ)) return;
	static_cast<connector*>(arg)->handle_notify();	
}

void connector::handle_connect_timeout(evutil_socket_t fd, short ev, void *arg)
{
	connector* ctor = static_cast<connector*>(arg);
	assert(connector::connecting==ctor->state_);
	log_warn("connector connect timeout");

	ctor->out_queue_.clear();
	ctor->reopen_sock();
}

void connector::handle_read()
{
	char buf[1]; 
	ssize_t ret = recv(sock_,buf,sizeof(buf),0);
	if(0==ret || ret < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)){
		log_fatal("connector::handle_read fail: peer %s,errno=%d",ret==0?"close":"error",errno);
		reopen_sock();	
		log_info("connector::handle_read: state is %s",get_state_text(state_));
	}
}

void connector::handle_read(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_READ)) return;
	static_cast<connector*>(arg)->handle_read();	
}

void connector::handle_write()
{
	if(connector::connecting==state_){
		int error, len = sizeof(error);
		if (getsockopt(sock_,SOL_SOCKET,SO_ERROR,&error,(socklen_t*)&len) < 0 || error != 0)
			reopen_sock();
		else{
			state_ = connector::connected;
			event_del(&ev_timer_);
		}
		log_info("connector::handle_write: state is %s",get_state_text(state_));
	}
	if(connector::connected==state_)
		async_send();
}

void connector::handle_write(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_WRITE)) return;
	static_cast<connector*>(arg)->handle_write();
}
