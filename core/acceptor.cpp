#include "acceptor.h"
#include "server.h"
#include "worker.h"
#include "../log/log_send.h"
#include "../net/function.h"
#include <stdexcept>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __x86_64__
#include <unistd.h>
#endif
using namespace netcomm;
using namespace std;

/************************************** ChangeLog ******************************
2012-8-23	Use edge-triggered instead of level-triggered,combine ev_et flag with ev_read or ev_write,otherwise write event will be triggered ceaselessly,casue high cpu usage. 

2012-11-8	add include unistd.h header file to support 64 bit compile
2013-6-18	add event_base param for constructor and open methods
 ******************************************************************************/

acceptor::acceptor(const net_addr& addr,event_base* base,bool is_on_ssl_port)
:sock_(-1)
,is_accept_(true)
,is_on_ssl_port_(is_on_ssl_port)
{
	if(!open(addr,base))
		throw runtime_error("");
}

acceptor::acceptor(bool is_on_ssl_port)
:sock_(-1)
,is_accept_(true)
,is_on_ssl_port_(is_on_ssl_port)
{
}

acceptor::~acceptor()
{
	close();
}

bool acceptor::open(const net_addr& addr,event_base* base)
{
	int af = addr.family();
	if (AF_INET != af && AF_INET6 != af)
		return false;

	sock_ = ::socket(af,SOCK_STREAM,0);
	if (-1==sock_){
		log_fatal("acceptor socket fail: errno = %d",errno);	
		return false;
	}
	int flag = 1;
	net_addr final; 
	socklen_t len = sizeof(final);
	char text[INET6_ADDRSTRLEN];

	if(!netcomm::set_block_attr(sock_,false)){
		log_fatal("acceptor set_block_attr fail: errno = %d",errno);	
		goto fail;
	}
	setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (void*)&flag, sizeof(flag));

	if(-1==::bind(sock_,addr,addr.length())){
		if(AF_INET==addr.family())	{
			inet_ntop(AF_INET,addr.ip4_addr(),text,sizeof(text)); 
			log_fatal("acceptor bind on inet ip=%s,port=%u fail: errno=%d",text,ntohs(*addr.ip4_port()),errno);
		}else{
			inet_ntop(AF_INET6,addr.ip6_addr(),text,sizeof(text));
			log_fatal("acceptor bind on inet6 ip=%s,port=%u fail: errno=%d",text,ntohs(*addr.ip6_port()),errno);
		}
		goto fail;
	}

	int slen,rlen;
	len = sizeof(int);
	if(!getsockopt(sock_,SOL_SOCKET,SO_SNDBUF,(char*)&slen,&len) && !getsockopt(sock_,SOL_SOCKET,SO_RCVBUF,(char*)&rlen,&len))
		log_info("listen sock=%d,sndbuf=%d,rcvbuf=%d",sock_,slen,rlen);

#if 0
	if(slen < 32768){
		slen = 16384;
		if(!setsockopt(sock_,SOL_SOCKET,SO_SNDBUF,(const char*)&slen,sizeof(int)))
			log_info("listen set sndbuf size 32K successfully");
		else
			log_info("listen set sndbuf size 32K failed: errno = %d",errno);
	}
	rlen = 16384;
	if(!setsockopt(sock_,SOL_SOCKET,SO_RCVBUF,(const char*)&rlen,sizeof(int)))
		log_info("listen set rcvbuf size 32K successfully");
	else
		log_info("listen set rcvbuf size 32K failed: errno = %d",errno);
#endif   

	if(-1==::listen(sock_,SOMAXCONN)){
		if(AF_INET==addr.family())	{
			inet_ntop(AF_INET,addr.ip4_addr(),text,sizeof(text)); 
			log_fatal("acceptor listen on inet ip=%s,port=%u fail: errno=%d",text,ntohs(*addr.ip4_port()),errno);
		}else{
			inet_ntop(AF_INET6,addr.ip6_addr(),text,sizeof(text));
			log_fatal("acceptor listen on inet6 ip=%s,port=%u fail: errno=%d",text,ntohs(*addr.ip6_port()),errno);
		}
		goto fail;
	}

	len = sizeof(final);
	if(0==getsockname(sock_,final,&len)) {
		if(AF_INET==final.family())	{
			inet_ntop(AF_INET,final.ip4_addr(),text,sizeof(text)); 
			log_info("acceptor listen on inet ip=%s,port=%u",text,ntohs(*final.ip4_port()));
		}else{
			inet_ntop(AF_INET6,final.ip6_addr(),text,sizeof(text));
			log_info("acceptor listen on inet6 ip=%s,port=%u",text,ntohs(*final.ip6_port()));
		}
	}

	assert(base);
	if(event_assign(&ev_accept_,base,sock_,EV_READ|EV_PERSIST|EV_ET,handle_accept,this)){
		log_error("acceptor::open event_assign fail");	
		goto fail;
	}
	if(event_add(&ev_accept_,NULL)){
		log_error("acceptor::open event_add fail");	
		goto fail;
	}

	addr_ = addr;
	return true;

fail:
	SAFE_CLOSE_FD(sock_);
	return false;
}

void acceptor::close()
{
	if(-1!=sock_){
		event_del(&ev_accept_);
		SAFE_CLOSE_FD(sock_);
	}
}

bool acceptor::dispatch_conn(int sock,const char* ip,uint16_t port)
{
	server* s = server::instance();
	int &index = s->last_worker_;
	index = (index + 1)%s->workers_.size();
	
	if(!s->workers_[index]->notify_add_conn(sock,ip,port,is_on_ssl_port_)){
		log_fatal("acceptor dispatch sock %d to worker %d fail",sock,index);
		return false;
	}
	return true;
}

void acceptor::handle_accept()
{
	net_addr addr;  socklen_t len; 
	int sock;
	char text[INET6_ADDRSTRLEN];
	uint16_t port;

	for (;;) {
		len = sizeof(addr);
		sock = ::accept(sock_,addr,&len);
		if(-1==sock){
			if(errno != EAGAIN && errno != EWOULDBLOCK)
				log_fatal("acceptor::handle_accept accept fail: errno=%d",errno);
			break;
		}
		if(AF_INET==addr.family()){
			inet_ntop(AF_INET,addr.ip4_addr(),text,sizeof(text)); 
			port = ntohs(*addr.ip4_port());
		}else{
			inet_ntop(AF_INET6,addr.ip6_addr(),text,sizeof(text));
			port = ntohs(*addr.ip6_port());
		}
		if(addr.is_any()||addr.is_multicast()||addr.is_all_broadcast()){
			::close(sock);
			log_error("acceptor: %s is not unique cast address",text);
			continue;
		}
		if(!is_accept_){
			::close(sock);
			continue;
		}
		if(!netcomm::set_block_attr(sock,false)){
			log_fatal("acceptor::handle_accept set sock %d no-blocking fail",sock);
			::close(sock); break;
		}
		if(!dispatch_conn(sock,text,port)){
			::close(sock); break;
		}
	}
}

void acceptor::handle_accept(evutil_socket_t fd,short ev,void *arg)
{
	static_cast<acceptor*>(arg)->handle_accept();
}
