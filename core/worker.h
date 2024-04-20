#ifndef _CORE_WORKER_H
#define _CORE_WORKER_H

#include "typedef.h"
#if USE_SSL
#include "ssl_ctx.h"
#endif
#include "../mem/new_policy.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../base/circle_buffer.h"
#include "../loki/Typelistex.h"

#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
#include "../mem/pool_allocator.h"
#endif

#include <ext/hash_set>
#include <pthread.h>
#include <event.h>

class connection;

typedef __gnu_cxx::hash_set<connection*,hash<connection*>,std::equal_to<connection*>,conn_alloc_t> conn_bucket_t;

class conn_bucket : public conn_bucket_t
{
	typedef conn_bucket_t base;
	void clear();
public:
	conn_bucket& operator=(const conn_bucket& other);
};

#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
	typedef memory::tls_allocator<conn_bucket> cb_alloc_t;
#else
	typedef std::allocator<conn_bucket> cb_alloc_t;
#endif

/**
 * @class worker
 * @brief this class encapsulates thread object that run one thread with callback function thread_entry
 * 
 * because construct and destroy of this class object with new and delete only can be used for single 
 * thread,so it inherit from class tls_spec_new_policy of namespace memory
 */
class worker : public base::if_then_else<_USE_MEM_POOL,
								     memory::tls_spec_new_policy<worker>,
									 base::null_type
									 >::type
{
	friend class connection;
public:
	explicit worker(int cpu_id,bool is_start=true);
	~worker();

public:
	bool start();
	bool stop();
	  
	/**
 	 notify new connection to worker that will process event in it.
	 should be call by one external thread which worker not belong to
    */
	bool notify_exit();

	bool notify_add_conn(int sock,const char* ip,uint16_t port,bool is_ssl);

	bool notify_match_conn(connection* src_conn,int dst_sock,bool flag);
	
protected:
	void handle_notify();
	void handle_timeout();
	bool send_iovec(char type,void* msg,size_t len);
	void state_machine();

protected:
	void run();
	static void handle_notify(evutil_socket_t fd, short ev, void *arg);
	static void handle_timeout(evutil_socket_t fd, short ev, void *arg);
	static void* thread_entry(void* param);

private:
	#pragma pack(1)
	struct pipe_msg_add_conn
	{
		int sock;
		char ip[INET6_ADDRSTRLEN];
		uint16_t port;
		bool ssl;
	};
	struct pipe_msg_match_conn
	{
		//one connection to be attach
		connection*  src_conn; 
		//one connection to be match with the connection which the src_sock member specifies,default value is -1.
		int  dst_sock;
		//true indicate server match client,otherwise client match server
		bool flag;  
	};
	#pragma pack()
	static const char EXIT_WORKER = 0;
	static const char ADD_SOCK	  = 1;
	static const char MATCH_SOCK  = 2;
	static const size_t PIPE_MSG_MAX_SIZE = Loki::TL::MaxSize<LOKI_TYPELIST_2(pipe_msg_add_conn,pipe_msg_match_conn) >::value;
	
	enum state { prepare,add_conn,match_conn };

	pthread_t id_;
	int cpu_id_;

	//for notify new connection be attach to this worker
	int fd_[2]; 
	
	event_base* ev_base_;
	event ev_notify_;
	event ev_timer_;
	char pipe_buf_[PIPE_MSG_MAX_SIZE];
	size_t pipe_tran_;
	size_t pipe_size_;
	state s_;

	base::circle_buffer<conn_bucket,cb_alloc_t> cb_;

#if USE_SSL
	ssl_ctx_t ssl_ctx_;
#endif
};

#endif
