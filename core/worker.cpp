#include "worker.h"
#if USE_SSL
#include "ssl_conn.h"
#else
#include "connection.h"
#endif
#include "global.h"
#include "../log/log_send.h"
#include "../base/util.h"
#include "../net/function.h"
#include <stdexcept>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/uio.h>
#ifdef __x86_64__
#include <unistd.h>
#endif
#define _GNU_SOURCE
#include <sched.h>

using namespace std;

/***************** ChangeLog *******************************************************************************
2012-8-23	use edge-triggered instead of level-triggered,combine ev_et flag with ev_read or ev_write, 
			otherwise write event will be triggered ceaselessly,resulting in high cpu usage. 

2012-11-8	add include unistd.h header file to support 64 bit compile

2012-12-24	add timing wheel to kill idle connection that have not read and written data within a certain time
2013-5-20	set thread cpu affinity with worker
************************************************************************************************************/

void conn_bucket::clear()
{
	connection *conn, *other;
	conn_bucket::iterator it,itt;

	for(it=begin();it!=end();){
		conn = *it++, other = conn->get_other();
		conn->close(IDLE_TIMEOUT);

		if(other && other->is_client()){
			itt = find(other);
			if(itt!=end() && itt==it)
				it = ++itt;
			other->close(IDLE_TIMEOUT);
		}
	}
}

conn_bucket& conn_bucket::operator=(const conn_bucket& other)
{
	if(this != &other){
		clear();
		base::operator=(other);
	}
	return *this;
}
///////////////////////////////////////////////////////////////////////////////
worker::worker(int cpu_id,bool is_start/*=true*/)
:ev_base_(NULL)
,pipe_tran_(0)
,pipe_size_(1)
,s_(prepare)
,cpu_id_(cpu_id)
,cb_(g_conf.timeout_idle_)
{
   fd_[0] = fd_[1] = -1;   
   cb_.resize(g_conf.timeout_idle_);

#if USE_SSL
   if(!init_ssl_ctx(g_conf.ssl_,&ssl_ctx_))
	   throw runtime_error("worker init_ssl_ctx fail");
#endif

   if(is_start&&!start())
	   throw runtime_error("worker start fail");
}

worker::~worker()
{
   stop();
}

//if start fail,then call reset to restore initial status
bool worker::start()
{
	if(ev_base_)    return true;

	if(::pipe(fd_)){
		log_fatal("worker::start pipe fail: errno = %d",errno);	
		return false;
	}
	
	int ret;
	unsigned char mask = 0x01;
	if(!netcomm::set_block_attr(fd_[0],false)){
		log_fatal("worker::start set_block_attr fail: errno = %d",errno);	
		goto fail;
	}
	
	ev_base_ = event_base_new();
	if(NULL==ev_base_){ 
		log_error("worker::start event_base_new fail");		
		goto fail;
	}
	mask |= 0x02;

	if(event_assign(&ev_notify_,ev_base_,fd_[0],EV_READ|EV_PERSIST|EV_ET,handle_notify,this)){
		log_error("worker::start event_assign fail");	
		goto fail;
	}
	if(event_add(&ev_notify_,NULL)){
		log_error("worker::start event_add fail");		
		goto fail;
	}
	mask |= 0x04;

	if(event_assign(&ev_timer_,ev_base_,-1,EV_PERSIST,handle_timeout,this)){
		log_error("woker::start evtimer_assign fail");
		goto fail;
	}
	struct timeval tv;
	tv.tv_sec = 1, tv.tv_usec = 0;
	if(evtimer_add(&ev_timer_,&tv)){
		log_error("worker::start evtimer_add fail");
		goto fail;
	}
	mask |= 0x08;

	if(ret = pthread_create(&id_,NULL,thread_entry,this)){
		log_fatal("worker::start pthread_create fail errno = %d",ret);		
		goto fail;
	}

	return true;

fail:
	if(mask&0x08)
		event_del(&ev_timer_);
	if(mask&0x04)
		event_del(&ev_notify_);
	if(mask&0x02){ 
		event_base_free(ev_base_);
		ev_base_ = NULL;
	}
	if(mask & 0x01)
		SAFE_CLOSE_PIPE(fd_);
	return false;
}

/**
 *	if ev_base_ is not null,then id_ and attr_ must be valid,
 *	so only check ev_base_ whether null or not,rather than all status.
 */
bool worker::stop()
{
	if(NULL==ev_base_) return true;

	notify_exit(); 
	if(pthread_join(id_,NULL))
		return false;
	
	event_del(&ev_notify_);
	event_base_free(ev_base_);
	ev_base_ = NULL;
	SAFE_CLOSE_PIPE(fd_);

	return true;
}

bool worker::notify_exit()
{
	char type = EXIT_WORKER;
	if(write(fd_[1],&type,1)!=1){
		log_fatal("worker::notify_exit fail: errno=%d",errno);
		return false;
	}
	return true;
}

bool worker::send_iovec(char type,void* msg,size_t len)
{
	struct iovec iov[2];
	iov[0].iov_base = &type;
	iov[0].iov_len  = 1;
	iov[1].iov_base = msg;
	iov[1].iov_len  = len;

	return writev(fd_[1],iov,NUM_ELEMENTS(iov))==1+len;
}

bool worker::notify_add_conn(int sock,const char* ip,uint16_t port,bool is_ssl)
{
	pipe_msg_add_conn msg;
	msg.sock = sock;
	strcpy(msg.ip,ip);
	msg.port = port;
	msg.ssl = is_ssl;

	if(!send_iovec(ADD_SOCK,&msg,sizeof(msg))){
		log_fatal("worker::notify_add_conn fail: errno=%d",errno);	
		return false;
	}
	return true;

}
/**
 * 	If the bytes of buf to be written is less than or equal to PIPE_BUF,
 *	write operation is atomically,whether the O_NONBLOCK flag is specified or not;
 *	otherwise,atomic operation is not guaranteed.
 */
bool worker::notify_match_conn(connection* src_conn,int dst_sock,bool flag)
{
	pipe_msg_match_conn msg;
	msg.src_conn = src_conn;
	msg.dst_sock = dst_sock;
	msg.flag     = flag;
	
	if(!send_iovec(MATCH_SOCK,&msg,sizeof(msg))){
		log_fatal("worker::notify_match_conn fail: errno=%d",errno);	
		return false;
	}
	return true;
}

void worker::state_machine()
{
	connection* conn = NULL;
	int sock;

	try{
		switch(s_) {
		case prepare:
			if(ADD_SOCK==pipe_buf_[0]){
				pipe_tran_ = 0,	pipe_size_ = sizeof(pipe_msg_add_conn);
				s_ = add_conn;
			}else if(MATCH_SOCK==pipe_buf_[0]){
				pipe_tran_ = 0, pipe_size_ = sizeof(pipe_msg_match_conn);
				s_ = match_conn;
			}else if(EXIT_WORKER==pipe_buf_[0]){
				event_base_loopbreak(ev_base_);
				return;
			}else 
				assert(false);
			break;

		case add_conn:
			if(pipe_tran_ == pipe_size_){	
				pipe_tran_ = 0, pipe_size_ = 1;		
				s_ = prepare;	

				pipe_msg_add_conn* msg = reinterpret_cast<pipe_msg_add_conn*>(pipe_buf_);	
			#if USE_SSL
				if(msg->ssl)
					conn = new ssl_conn_t(&ssl_ctx_,msg->sock,msg->ip,msg->port);
				else
					conn = new connection(msg->sock,msg->ip,msg->port);
			#else
				conn = new connection(msg->sock,msg->ip,msg->port);
			#endif
				conn->attach(this);	
			}
			break;

		case match_conn:
			if(pipe_tran_==pipe_size_){
				pipe_tran_ = 0, pipe_size_ = 1;		
				s_ = prepare;

				pipe_msg_match_conn* msg = reinterpret_cast<pipe_msg_match_conn*>(pipe_buf_);		
				conn = msg->src_conn;
				assert(conn);
				conn->attach(this);
				conn->match(msg->dst_sock,msg->flag);
			}
			break;
		}
	}catch(http_exception& he){
		conn->send_error(he.code_);
	}catch(io_exception& ie){
		conn->close(ie.flag_);
	}catch(std::exception& se){
		if(NULL==conn) 
			::close(sock);
		else
			delete conn;
		log_fatal(se.what());
	}catch(...){
		if(NULL==conn) 
			::close(sock);
		else
			delete conn;
		log_fatal("worker::state_machine unknown exception");
	}
}

void worker::handle_notify()
{
	//the number of bytes read, there is the number of connections
	ssize_t ret;
	for (;;) {
		ret = read(fd_[0], pipe_buf_+pipe_tran_, pipe_size_-pipe_tran_);
		if(ret > 0){
			pipe_tran_ += ret; 
			state_machine();
		}else if(ret < 0 && (errno==EAGAIN || errno==EWOULDBLOCK)){
			//the pipe has no available data 		
			break;
		}else if(0==ret){
			//some thread or process has the pipe open for writing close it
			break;
		}else{
			log_fatal("worker::handle_notify read fail: errno = %d",errno);
			break;
		}
	}	
}

void worker::handle_timeout()
{
	cb_.push_back(conn_bucket());
}

void worker::run()
{
	log_info("worker %lu is running...",id_);

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGINT);
	sigaddset(&mask,SIGTERM);
	sigaddset(&mask,SIGHUP);
	pthread_sigmask(SIG_BLOCK,&mask,NULL);

	cpu_set_t cpu;
	CPU_ZERO(&cpu);
	CPU_SET(cpu_id_, &cpu);

	int ret;
	if (!(ret=pthread_setaffinity_np(id_,sizeof(cpu), &cpu))){
		CPU_ZERO(&cpu);
		if(!(ret=pthread_getaffinity_np(id_, sizeof(cpu), &cpu))){
			if (CPU_ISSET(cpu_id_, &cpu))
				log_info("worker %lu is running in processor %d",id_,cpu_id_);
		}else
			log_warn("worker %lu get affinity fail: cpu=%d,error=%d",id_,cpu_id_,ret);
	}else
		log_warn("worker %lu set affinity fail: cpu=%d,error=%d",id_,cpu_id_,ret);
	
	if(event_base_dispatch(ev_base_))
		log_fatal("worker %lu run error",id_);

	log_info("worker %lu is exit...",id_);
}

void worker::handle_notify(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_READ))
		return;
    static_cast<worker*>(arg)->handle_notify();
}

void worker::handle_timeout(evutil_socket_t fd, short ev, void *arg)
{
	static_cast<worker*>(arg)->handle_timeout();
}

void* worker::thread_entry(void* param)
{
	static_cast<worker*>(param)->run();
	return NULL;
}
