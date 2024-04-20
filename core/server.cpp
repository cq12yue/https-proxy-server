#include "server.h"
#include "connector.h"
#include "acceptor.h"
#include "worker.h"
#include "codgram.h"
#include "flow_grab.h"
#include "global.h"
#include "../log/log_send.h"
#include "../loki/ScopeGuard.h"
#include "../base/cstring.h"
#include "../base/ini_file.h"
#include <signal.h>
#include <string.h> //for strtok,memset
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <execinfo.h>
#include <sys/stat.h>
#include <unistd.h>
//#define __USE_GNU
#define _GNU_SOURCE
#include <sched.h>
#include <ucontext.h>
#define _FILE_OFFSET_BITS 64
#include <sys/resource.h>

using namespace netcomm;
using namespace base;
using namespace std;

/***************************************ChangeLog***********************************************
2012-11-8	add include unistd.h header file to support 64 bit compile
2013-5-20	set thread cpu affinity with server(main thread)
2013-8-27	add start_flow_grab and stop_flow_grab functions
2013-9-29	change config variable limit_flow into signed long data type
2013-11-25	add SIGHUP signal handling to support reload external configuration file
************************************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////////
server::server()
:ev_base_(NULL)
,last_worker_(-1)
{
}

server::~server()
{
}

bool server::init(bool is_ipv4)
{
	is_ipv4_ = is_ipv4;

	ev_base_ = event_base_new();
	if (NULL==ev_base_){
		log_error("server::init event_base_new fail");
		return false;
	}
	unsigned char mask = 0x01;

	if(g_conf.p2p_enable_){
		if(!start_connector())
			goto fail;
		mask |= 0x02;
	}

	if(!start_workers())
		goto fail;
	mask |= 0x04;

	if(!start_acceptors())
		goto fail;
	mask |= 0x08;

	if(g_conf.disp_enable_){
		if(!start_codgram())
			goto fail;
		mask |= 0x10;
	}

	if((g_conf.limit_flow_>0 || g_conf.limit_rate_>0) && !start_flow_grab())
		goto fail;

	return true;

fail:
	if(mask & 0x10)
		stop_codgram();
	if(mask & 0x08)
		stop_acceptors();
	if(mask & 0x04)
		stop_workers();
	if(mask & 0x02)
		stop_connector();
	if(mask & 0x01)
		event_base_free(ev_base_);
	return false;
}

int server::run()
{
	log_info("main thread is running...");

	assert(ev_base_);

	struct rlimit rm;
	rm.rlim_cur = 65535,rm.rlim_max = 65536;
	if(-1==setrlimit(RLIMIT_NOFILE,&rm))
		log_fatal("server prlimit fail error=%d",errno);
	
	int ret;

	cpu_set_t cpu;
	CPU_ZERO(&cpu); 
	CPU_SET(0,&cpu); 

	pthread_t id = pthread_self();
	
	if(!(ret=pthread_setaffinity_np(id, sizeof(cpu), &cpu))){
		CPU_ZERO(&cpu);
		if(!(ret=pthread_getaffinity_np(id, sizeof(cpu), &cpu))){
			if (CPU_ISSET(0, &cpu))
				log_info("server %lu is running in processor 0",id);
		}else
			log_warn("server %lu get affinity fail: error=%d",id,ret);
	}else
		log_warn("server %lu set affinity fail: error=%d",id,ret); 

	ret = 0;
	if(-1==event_base_dispatch(ev_base_)){
		ret = 1;	
		log_fatal("server::run event_base_dispatch fail");
	}

	stop_connector();
	stop_acceptors();
	stop_workers(); 
	stop_codgram();
	stop_flow_grab();
	event_base_free(ev_base_);
	
	log_info("main thread is exit...");

	return ret;
}

void server::enable_accept(bool is_accept)
{
	for(vector<acceptor*>::iterator it=acceptors_.begin();it!=acceptors_.end();++it){
		(*it)->enable(is_accept);
	}
}

bool server::start_connector()
{
	addrinfo hints,*res;
	memset(&hints,0,sizeof(hints));
	hints.ai_family = (is_ipv4_ ? AF_INET : AF_INET6);
	hints.ai_socktype = SOCK_STREAM;

	char buf[32];
	snprintf(buf, sizeof(buf), "%d", g_conf.p2p_port_);
	int ret;
	if(ret=getaddrinfo(g_conf.p2p_addr_.c_str(),buf,&hints,&res)){
		log_fatal("start_connector getaddrinfo fail:%s-%d,%s",g_conf.p2p_addr_.c_str(),g_conf.p2p_port_,
				gai_strerror(ret));
		return false;
	}

	LOKI_ON_BLOCK_EXIT(freeaddrinfo,res);

	switch(singleton<connector>::instance()->open(net_addr(res->ai_addr,res->ai_addrlen),ev_base_)) {
		case connector::disconnected:
			log_info("connector is disconnected");
			break;
		case connector::connecting:
			log_info("connector is connecting");
			break;
		case connector::connected:
			log_info("connector is connected");
			break;
		case connector::failed:
			log_error("connector open fail");
			return false;
	}
	return true;
}

void server::stop_connector()
{
	singleton<connector>::instance()->close();
}

bool server::start_codgram()
{
	addrinfo hints,*res;
	memset(&hints,0,sizeof(hints));
	hints.ai_family = (is_ipv4_ ? AF_INET : AF_INET6);
	hints.ai_socktype = SOCK_DGRAM;

	char buf[32];
	snprintf(buf,sizeof(buf),"%d",g_conf.disp_port_);
	int ret;
	if(ret=getaddrinfo(g_conf.disp_addr_.c_str(),buf,&hints,&res)){
		log_fatal("start_codgram getaddrinfo fail:%s-%d,%s",g_conf.disp_addr_.c_str(),g_conf.disp_addr_.c_str(),
					gai_strerror(ret));
		return false;
	}

	LOKI_ON_BLOCK_EXIT(freeaddrinfo,res);
	if(!codgram::instance()->open(net_addr(res->ai_addr,res->ai_addrlen),ev_base_)){
		log_fatal("start_codgram open fail:%s-%d",g_conf.disp_addr_.c_str(),g_conf.disp_port_);	
		return false;
	}
	return true;
}

void server::stop_codgram()
{
	codgram::instance()->close();
}

bool server::start_workers()
{
	int num = sysconf(_SC_NPROCESSORS_ONLN);
	log_info("system has %d online processors",num);

	if(num > 0)
		num -= 1;
	if(num <= 0)
		num = 1;
	
	for (int i=1;i<=num;++i){
		try {
			worker* p = new worker(i);
			try { 
				workers_.push_back(p);
			}catch(...){
			  delete p;
			}
		}catch(...){

		}
	}
	log_info("started %d workers",workers_.size());
	return !workers_.empty();
}

void server::stop_workers()
{
	vector<worker*>::iterator it;
	for (it = workers_.begin();it!=workers_.end();)	{
		delete *it; it = workers_.erase(it);
	}
}

bool server::start_acceptors()
{
	char old,*b,*s,*beg=&g_conf.address_[0],*end=beg+g_conf.address_.size();

	for (s = strtok_r(beg,";,",&b); s; s = strtok_r(NULL,";,",&b)) {
		if(b!=end) {
			old = *--b, *b = '\0';
		}
		
		if (0==strcmp(s, "*")) 
			s = NULL;
		if(g_conf.port_)
			start_per_acceptor(s,g_conf.port_,false);
#if USE_SSL
		if(g_conf.ssl_.port_)
			start_per_acceptor(s,g_conf.ssl_.port_,true);
#endif

		if(b!=end) *b++ = old;
	}

	log_info("started %d acceptor",acceptors_.size());
	return !acceptors_.empty();
}

bool server::start_per_acceptor(const char* address,uint16_t port,bool is_ssl_port)
{
	addrinfo hints, *res;
	memset(&hints,0,sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = (is_ipv4_ ? AF_INET : AF_INET6);
	hints.ai_socktype = SOCK_STREAM;

	char buf[32];
	snprintf(buf, sizeof(buf), "%d", port);
	int ret;
	if(ret=getaddrinfo(address,buf,&hints,&res)){
		log_fatal("start_per_acceptor getaddrinfo fail:%s-%d,%s",address,port,gai_strerror(ret));
		return false;
	}

	LOKI_ON_BLOCK_EXIT(freeaddrinfo,res);

	struct addrinfo* ai;
	for (ai=res;ai;ai=ai->ai_next) {
		try {
			acceptor *p = new acceptor(netcomm::net_addr(ai->ai_addr,ai->ai_addrlen),ev_base_,is_ssl_port);
			try{
				acceptors_.push_back(p);
				break; //if successful,then exit the loop
			}catch(...){
				delete p;
			}
		}catch(...){

		}
	}
	return NULL!=ai;
}

void server::stop_acceptors()
{
	vector<acceptor*>::iterator it;
	for (it = acceptors_.begin();it!=acceptors_.end();) {
		delete *it; it = acceptors_.erase(it);
	}
}

bool server::start_flow_grab()
{
	char buf[256];

	int ret = snprintf(buf,sizeof(buf)-1,"tcp src port %d",g_conf.port_);
#if USE_SSL
	ret += snprintf(buf+ret,sizeof(buf)-1-ret," or %d",g_conf.ssl_.port_);
#endif
	buf[ret] = '\0';

	log_info("flow grab filter: %s",buf);

	return singleton<flow_grab>::instance()->start(ev_base_,NULL,buf);
}

void server::stop_flow_grab()
{
	if(g_conf.limit_flow_>0||g_conf.limit_rate_>0)
		singleton<flow_grab>::instance()->stop();
}

void server::stop()
{
	event_base_loopbreak(ev_base_);
}
