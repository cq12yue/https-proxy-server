#ifndef _CORE_ACCEPTOR_H
#define _CORE_ACCEPTOR_H

#include "../net/net_addr.h"
#include "../mem/new_policy.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../net/net_addr.h"
#include <event.h>

/**
 * @class acceptor 
 * @brief this class encapsulates listen object that accept connections from client at some special binding address
 * 
 * because construct and destroy of this class object with new and delete only can be used for single 
 * thread,so it inherit from class tls_spec_new_policy of namespace memory
 */
class acceptor : public base::if_then_else<_USE_MEM_POOL,
									   memory::tls_spec_new_policy<acceptor>,
									   base::null_type
									   >::type
{
	friend class server;
public:
	explicit acceptor(const netcomm::net_addr& addr,event_base* base,bool is_on_ssl_port);
	acceptor(bool is_on_ssl_port);
	~acceptor();

	bool open(const netcomm::net_addr& addr,event_base* base);
	void close();
	
	void enable(bool is_accept)
	{ is_accept_ = is_accept; }

	void set_on_ssl_port(bool is_on_ssl_port)
	{ is_on_ssl_port_ = is_on_ssl_port_; }

	bool is_on_ssl_port() const
	{ return is_on_ssl_port_; }

protected:	
	bool dispatch_conn(int sock,const char* ip,uint16_t port);
	void handle_accept();
	static void handle_accept(evutil_socket_t fd,short ev,void *arg);

private:	
	bool  is_accept_;
	bool  is_on_ssl_port_;

	int   sock_; // for listen
	event ev_accept_;
	netcomm::net_addr addr_;
};

#endif
