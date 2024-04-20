#ifndef _CORE_CODGRAM_H
#define _CORE_CODGRAM_H

#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../mem/new_policy.h"
#include "../net/net_addr.h"
#include <event.h>

class codgram : public base::if_then_else<_USE_MEM_POOL,
					memory::tls_spec_new_policy<codgram>,
					base::null_type
					>::type
{
public:
	codgram();
	bool open(const netcomm::net_addr& remote_addr,event_base* base);
	void close();
	bool send(const void* data,size_t len);

protected:
	static void	handle_read(evutil_socket_t fd, short ev, void *arg);
	static void	handle_timeout(evutil_socket_t fd, short ev, void *arg);
	void handle_timeout();

private:
	int	  sock_;
	uint64_t last_out_flow_;
	event ev_read_;
	event ev_timer_;
};

#endif
