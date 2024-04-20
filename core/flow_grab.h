#ifndef _CORE_FLOW_GRAB_H
#define _CORE_FLOW_GRAB_H

#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../mem/new_policy.h"
#include <pthread.h>
#include <pcap/pcap.h>
#include <event.h>
#include <stdint.h>

class flow_grab : public base::if_then_else<_USE_MEM_POOL,
					memory::tls_spec_new_policy<flow_grab>,
					base::null_type
					>::type
{
public:
	flow_grab();

	bool start(event_base *base,const char* dev=NULL,const char* filter=NULL);
	void stop();
	
	uint64_t get_out_flow() const 
	{ return __sync_val_compare_and_swap(const_cast<volatile uint64_t*>(&obytes_),0,0); }

protected:
	static void handle_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp);
	static void handle_read(evutil_socket_t fd,short ev,void *arg);
	void handle_read();
private:
	event ev_read_;
	pcap_t* pd_;
	volatile uint64_t obytes_;
};

#endif
