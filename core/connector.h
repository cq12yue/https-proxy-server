#ifndef _CORE_CONNECTOR_H
#define _CORE_CONNECTOR_H

#include "buffer.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../net/net_addr.h"
#include "../mem/new_policy.h"
#include <event.h>

class connector : public base::if_then_else<_USE_MEM_POOL,
						memory::tls_spec_new_policy<connector>,
						base::null_type
						>::type
{
public:
	enum state { failed = -1,disconnected, connecting, connected };

	connector();
	int  open(const netcomm::net_addr& addr,event_base* base);
	void close();
	bool async_send(const void* data,size_t len);
	void send(const void* data,size_t len);

protected:
	static void handle_notify(evutil_socket_t fd, short ev, void *arg);
	static void handle_read(evutil_socket_t fd, short ev, void *arg);
	static void handle_write(evutil_socket_t fd, short ev, void *arg);
	static void handle_connect_timeout(evutil_socket_t fd, short ev, void *arg);

	void handle_read();
	void handle_write();
	void handle_notify();
	bool open_pipe(event_base* base);
	int  open_sock(event_base* base);
	int  open(event_base* base);

	void close_pipe();
	void close_sock();

	void reopen_sock();
	void async_send();

private:
	static const size_t PIPE_MSG_SIZE = sizeof(intptr_t);

	state state_;
	int   sock_;
	int   fd_[2];

	st_buffer_queue out_queue_;
	netcomm::net_addr addr_;

	event ev_read_;
	event ev_write_;
	event ev_timer_;
	event ev_notify_;

	char pipe_buf_[PIPE_MSG_SIZE];
	size_t pipe_tran_;
};

#endif
