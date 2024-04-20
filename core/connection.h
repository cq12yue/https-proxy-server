#ifndef _CORE_CONNECTION_H
#define _CORE_CONNECTION_H

#include "http.h"
#include "buffer.h"
#include "exception.h"
#include "../base/mutex.h"
#include "../base/null_mutex.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include <event.h>

class worker;
struct conn_bucket;

/**
 * @class connection
 * 
 * construct and destroy of this class object with new and delete can be used for multi-thread,
 * so it inherit from class mt_spec_new_policy of namespace memory
 */
class connection : public base::if_then_else<_USE_MEM_POOL,
										 memory::tls_spec_new_policy<connection>,
										 base::null_type
										 >::type
{
public:
	static const uint8_t CLIENT;
	static const uint8_t IDLE_SERVER;
	static const uint8_t BUSY_SERVER;

	static const uint8_t MASK_BIT_RD;
	static const uint8_t MASK_BIT_WR;
	static const uint8_t MASK_BIT_RDWR;
	static const uint8_t MASK_BIT_ENABLE_RD;
	static const uint8_t MASK_BIT_CONNECTED;
	static const uint8_t MASK_BIT_TIMEOUT;
	static const uint8_t MASK_BIT_CLOSE;
	static const uint8_t MASK_BIT_BROWSER;
	static const uint8_t MASK_BIT_SSL;

	friend class worker;
	friend class client_conn_table;
	friend class server_conn_table;
	friend class http_stream;

public:
	connection(int sock,const char* ip,uint16_t port);
	virtual ~connection();

	//attach connection to a certain worker which monitor read and write event on it
	void attach(worker *host);

	//detach connection from its worker
	void detach();

	void match(int sock,bool is_client);

	bool is_detach() const 
	{ return 0==(flag_&MASK_BIT_RDWR); }

	int get_fd() const 
	{ return sock_; }

	const char* get_ip() const
	{ return ip_.c_str(); }

	uint16_t get_port() const
	{ return port_; }

	void send_error(int error);
	
	void close(uint8_t flag,bool reuse_server=false);

	connection* get_other()
	{ return other_;	}

	bool is_client() const
	{ return __sync_bool_compare_and_swap(const_cast<volatile uint8_t*>(&type_),CLIENT,CLIENT); }

protected:
	typedef void (connection::*io_handler)(short ev);
	io_handler read_handler_;
	io_handler write_handler_;
	uint8_t	     flag_;

protected:
	static void handle_read(evutil_socket_t fd, short ev, void *arg);
	static void handle_write(evutil_socket_t fd, short ev, void *arg);
	static void handle_timeout(evutil_socket_t fd, short ev, void *arg);
	
	virtual ssize_t recv(void *buf,size_t len);
	virtual ssize_t send(const void *buf,size_t len);
	virtual void close();

	void attach(worker *host,event_callback_fn rfn,event_callback_fn wfn);
	void handle_uri(buffer *&buf,int verb,char *uri,size_t len);
	void match_client(int sock);
	void match_server(const char* dev_id,bool reuse_server=false);
	void update();
	void handle_read(short ev);
	void handle_write(short ev);
	void async_send(uint8_t flag=SELF_WR_ERROR);
	void async_send_impl(uint8_t flag);
	void match(connection* conn,bool is_client);
	void request_new_server();

	void enable_read();
	void disable_read() 
	{ flag_ &= ~MASK_BIT_ENABLE_RD; }

	bool is_enable_read() const
	{  return (flag_&MASK_BIT_ENABLE_RD)!=0; }

	void active_event(bool is_read)
	{ event_active(is_read?&ev_read_:&ev_write_,is_read?EV_READ:EV_WRITE,1); }

	void add_event(bool is_read,const struct timeval &tv);

private:
	worker* host_;
	// for matched connection
	connection*  other_;  
	// current pointer to circular buffer's some bucket element
	conn_bucket* cb_tail_;

	int          sock_;
	volatile uint8_t  type_;

	event        ev_timer_;
	event        ev_read_;
	event        ev_write_;
	string_t     dev_id_;
	//for web client
	string_t     callback_; 

	st_buffer_queue in_queue_;
	st_buffer_queue out_queue_;

	http_stream  hstream_;
	
	string_t ip_;
	uint16_t port_;
};

//////////////////////////////////////////////////////////////////////////
class client_conn_table : public base::singleton<client_conn_table,base::mutex>
{
	typedef client_conn_map::iterator iterator;
	typedef client_conn_map::const_iterator const_iterator;
	SINGLETON_DECLARE(client_conn_table,base::mutex,true)	

public:
	void add(connection* conn);
	bool remove(int sock);
	connection* get(int sock,worker** host=NULL) const;

	void remove(connection* conn)
	{ remove(conn->sock_); }

	size_t size() const
	{ 
		base::lock_guard<base::mutex> guard(lock_);
		return conns_.size();
	}

	size_t match_size() const;

private:
	client_conn_map conns_;
	mutable base::mutex lock_; 
};

class server_conn_table : public base::singleton<server_conn_table,base::mutex>
{
	typedef server_conn_map::iterator iterator;
	typedef server_conn_map::const_iterator const_iterator;
	SINGLETON_DECLARE(server_conn_table,base::mutex,true)

public:
	void add(connection* conn);
	bool remove(connection* conn);

	connection* get(int sock) const;
	connection* get(const string_t& dev_id,int sock,worker** host=NULL) const;
	connection* get_idle(const string_t& dev_id,int* sock=NULL,worker** host=NULL) const;

	size_t size() const
	{ 
		base::lock_guard<base::mutex> guard(lock_);
		return conns_.size();
	}

private:
	server_conn_map conns_;
	mutable base::mutex lock_;
};

#endif
