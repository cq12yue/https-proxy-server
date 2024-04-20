#include "flow_grab.h"
#include "../log/log_send.h"
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#define __USE_BSD
#include <netinet/ip.h>
#define __FAVOR_BSD
#include <netinet/tcp.h>
#include <arpa/inet.h>

flow_grab::flow_grab()
:obytes_(0)
,pd_(NULL)
{
}

bool flow_grab::start(event_base *base,const char* dev/*=NULL*/,const char* filter/*=NULL*/)
{
	assert(NULL==pd_);

	char ebuf[PCAP_ERRBUF_SIZE];
	if(NULL==dev){
		dev = pcap_lookupdev(ebuf);
		if (NULL==dev){
			log_error("flow_grab start pcap_lookupdev fail: %s",ebuf);
			return false;
		}
	}
	pd_ = pcap_open_live(dev,65535,0,0,ebuf);//no-promiscuous and wait forever
	*ebuf = '\0';
	if (NULL==pd_){
		log_error("flow_grab start pcap_open_live fail: %s", ebuf);
		return false;
	}else if(*ebuf)
		log_warn("flow_grab start pcap_open_live warn: %s", ebuf);
	
	bpf_u_int32 localnet,netmask;
	if (pcap_lookupnet(dev,&localnet,&netmask,ebuf) < 0) {
		localnet = netmask = 0;
		log_warn("flow_grab start pcap_lookupnet fail: %s", ebuf);
	}
	if(filter){
		struct bpf_program fcode;
		if (pcap_compile(pd_,&fcode,filter,1,netmask) < 0){
			log_error("flow_grab start pcap_compile fail: %s", pcap_geterr(pd_));
			goto fail;
		}
		if (pcap_setfilter(pd_,&fcode) < 0){
			log_error("flow_grab start pcap_setfilter fail: %s", pcap_geterr(pd_));
			goto fail;
		}
	}
	if (-1==pcap_get_selectable_fd(pd_)){
		log_error("flow_grab start pcap_get_selectable_fd fails");
		goto fail;
	}

	if (-1==pcap_setnonblock(pd_,1,ebuf)){
		log_error("flow_grab start pcap_setnonblock failed: %s", ebuf);
		goto fail;
	}

	if(event_assign(&ev_read_,base,pcap_get_selectable_fd(pd_),EV_READ|EV_PERSIST|EV_ET,handle_read,this)){
		log_error("flow grab event_assign fail");		
		goto fail;
	}
	if(event_add(&ev_read_,NULL)){
		log_error("flow grab event add fail");
		goto fail;
	}
	return true;

fail:
	if(pd_) {
		pcap_close(pd_); pd_ = NULL;
	}
	return false;
}

void flow_grab::stop()
{
	if(NULL==pd_) return;

	event_del(&ev_read_);

	pcap_close(pd_); pd_ = NULL;
}

void flow_grab::handle_read(evutil_socket_t fd,short ev,void *arg)
{
	static_cast<flow_grab*>(arg)->handle_read();
}

void flow_grab::handle_read()
{
	for(;;){
		if(pcap_dispatch(pd_,-1,handle_packet,(u_char*)this) <= 0){
			break;
		}
	}
}

void flow_grab::handle_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	flow_grab* fg = reinterpret_cast<flow_grab*>(user);
	struct ether_header* ehdr = (struct ether_header*)sp;

	if(ETHERTYPE_IP==ntohs(ehdr->ether_type)){
		struct ip* iph = (struct ip*)(ehdr+1);
		if(IPPROTO_TCP==iph->ip_p){
			u_int16_t len = ntohs(iph->ip_len);
			__sync_add_and_fetch(&fg->obytes_,len);
		}
	}
}
