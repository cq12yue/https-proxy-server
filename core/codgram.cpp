#include "codgram.h"
#include "server.h"
#include "global.h"
#include "connection.h"
#include "flow_grab.h"
#include "../log/log_send.h"
#include "../base/proc_stat.h"
#include "../net/function.h"
#include <limits.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
using namespace base;

/************************************************************************************************
2013-6-13 in handle_timeout function
			1) use inet_ntop instead of inet_ntoa to avoid multi-thread bug
			2) use thread local variable to optimize access ip.

2013-6-18	add event_base param for open methods
2013-9-29	improve handle_timeout,only when limit flow,send left_flow data
2013-10-12	modify the length of report packet which is sent to dispatch server,now it includes
			the character '}'
*************************************************************************************************/
codgram::codgram()
:sock_(-1)
{
}

bool codgram::open(const netcomm::net_addr& remote_addr,event_base* base)
{
	assert(base);
	
	int af = remote_addr.family();

	if(AF_INET!=af && AF_INET6!=af)	{		
		log_error("codgram::open only support AF_INET and AF_INET6 address family");			
		return false;
	}
	sock_ = socket(af,SOCK_DGRAM,0);
	if(-1==sock_) {
		log_fatal("codgram::open socket fail: errno=%d",errno);		
		return false;
	}
	unsigned char mask = 0;
	struct timeval tv;

	if(-1==connect(sock_,remote_addr,remote_addr.length())){
		log_fatal("codgram::open connect fail: errno=%d",errno);	
		goto fail;
	}
	if(event_assign(&ev_read_,base,sock_,EV_READ|EV_PERSIST|EV_ET,handle_read,this)){
		log_error("codgram::open event_assign ev_read_ fail");		
		goto fail;
	}
	if(event_add(&ev_read_,NULL)){
		log_error("codgram::open event_add ev_read_ fail");	
		goto fail;
	}
	mask |= 0x01;

	if(event_assign(&ev_timer_,base,-1,EV_PERSIST,handle_timeout,this)){
		log_error("connector::open event_assign ev_timer_ fail");		
		goto fail;
	}
	tv.tv_sec = g_conf.interval_report_, tv.tv_usec = 0;
	if(evtimer_add(&ev_timer_,&tv)){
		log_error("codgram::open event_add ev_timer_ fail");		
		goto fail;
	}
	return true;

fail:
	if(mask&0x01)
		event_del(&ev_read_);
	::close(sock_); sock_ = -1;
	return false;
}

void codgram::close()
{
	if(-1!=sock_){
		event_del(&ev_read_);
		event_del(&ev_timer_);
		::close(sock_); sock_ = -1;
	}
}

bool codgram::send(const void* data,size_t len)
{
	ssize_t ret;
	if(-1==(ret=::send(sock_,data,len,0))){
		log_fatal("codgram::send fail: errno=%d",errno);
		return false;
	}
	assert(ret==len);
	return true;
}

void codgram::handle_read(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_READ)) return;

	char buf[1024]; 
	ssize_t ret = recv(fd,buf,sizeof(buf),0);
	if(0==ret || ret < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)){
		log_fatal("codgram::handle_read recv fail: peer %s, errno=%d",ret==0?"close":"error",errno);
	}
}

void codgram::handle_timeout(evutil_socket_t fd, short ev, void *arg)
{
	static_cast<codgram*>(arg)->handle_timeout();
}

static float get_left_flow_ratio(uint64_t now_flow,long limit_flow)
{
	uint64_t flow_gb = now_flow >> 30;	
	float ratio = 1.0f;

	if(limit_flow > 0){		
		flow_gb = flow_gb >= limit_flow ? 0 : (limit_flow-flow_gb);
		ratio = flow_gb/(limit_flow*1.0f);
	}

	return ratio;
}

static float get_left_rate_ratio(uint64_t last_flow,uint64_t now_flow,long limit_rate,int time_span)
{
	assert(time_span > 0);

	uint64_t rate;
	rate = last_flow <= now_flow ? now_flow-last_flow : ULLONG_MAX-(last_flow-now_flow);	
	rate /= time_span*1024; //KB/s

	log_debug("codgram out flow limit_rate=%ld,last_flow=%lu,now_flow=%lu,rate=%lu KB/s",limit_rate,
		 last_flow,now_flow,rate);

	float ratio = 1.0f;
	if(limit_rate > 0)
		ratio = rate < limit_rate ? (limit_rate-rate)/(limit_rate*1.0f) : 0.0f;
	
	return ratio;
}

void codgram::handle_timeout()
{
	float cpu = 0.0f, mem = 0.0f;
	size_t conns = client_conn_table::instance()->size() + server_conn_table::instance()->size();
	base::get_process_usage(getpid(),cpu,mem);

	uint64_t now_out_flow = singleton<flow_grab>::instance()->get_out_flow();
	float ratio_flow = get_left_flow_ratio(now_out_flow,g_conf.limit_flow_);
	float ratio_rate = get_left_rate_ratio(last_out_flow_,now_out_flow,g_conf.limit_rate_,g_conf.interval_report_);
	last_out_flow_ = now_out_flow;

	server* s = server::instance();
	if(-0.000001f <= ratio_rate && ratio_rate <= 0.000001f)
		s->enable_accept(false);
	else
		s->enable_accept(true);	

	char buf[1024];	
	int ret; 
#if USE_SSL
	ret = snprintf(buf,sizeof(buf)-1,
		"{\"ip\":\"%s\",\"port\":%d,\"ssl_port\":%d,\"conns\":%lu,\"cpu\":%0.2f,\"mem\":%0.2f,\"left_flow\":%0.2f,\"left_rate\":%0.2f}",
			 g_conf.pub_addr_.c_str(),g_conf.port_,g_conf.ssl_.port_,conns,cpu,mem,ratio_flow,ratio_rate);
#else
	ret = snprintf(buf,sizeof(buf)-1,
		"{\"ip\":\"%s\",\"port\":%d,\"conns\":%lu,\"cpu\":%0.2f,\"mem\":%0.2f,\"left_flow\":%0.2f,\"left_rate\":%0.2f}",
			g_conf.pub_addr_.c_str(),g_conf.port_,conns,cpu,mem,ratio_flow,ratio_rate);
#endif
	buf[ret] = '\0';

	send(buf,ret);

	log_info("codgram report: %s",buf);
}
