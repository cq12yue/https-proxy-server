#ifndef _CORE_SERVER_H
#define _CORE_SERVER_H

#include "../mem/new_policy.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "config.h"
#include <vector>
#include <event.h>

class acceptor;
class worker;

/**
 * @class server
 * 
 * 1) because creation of class server object only occur at main thread,and thus use no lock
 * 2) because construct and destroy of this class object with new and delete only can be used for single 
 * thread,so it inherit from class tls_spec_new_policy of namespace memory
 */
class server : public base::singleton<server,base::null_mutex> 
	         , public base::if_then_else<_USE_MEM_POOL,
							          memory::tls_spec_new_policy<server>,
							          base::null_type
									 >::type
{
	SINGLETON_DECLARE(server,base::null_mutex,true)
	friend class acceptor;

public:
	bool init(bool is_ipv4);
	int run();	
	void stop();
	void enable_accept(bool is_accept);

protected:
	void reconfigure();
	bool start_connector();
	void stop_connector();
	bool start_codgram();
	void stop_codgram();
	bool start_workers();
	void stop_workers();
	bool start_acceptors();
	bool start_per_acceptor(const char* address,uint16_t port,bool is_on_ssl_port);
	void stop_acceptors();
	bool start_flow_grab();
	void stop_flow_grab();	
		
private:
	bool is_ipv4_;
	event_base* ev_base_;
	std::vector<worker*> workers_;
	int last_worker_;
	std::vector<acceptor*> acceptors_;
};

#endif
