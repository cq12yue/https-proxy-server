#ifndef _CORE_SSL_CONN_H
#define _CORE_SSL_CONN_H

#include "connection.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../mem/new_policy.h"
#include "../base/noncopyable.h"
#include <openssl/ssl.h>

class ssl_ctx_t;

class ssl_conn_t : public connection
		 , base::noncopyable
{
public:
	ssl_conn_t(ssl_ctx_t *ssl_ctx,int sock,const char* ip,int port);
	~ssl_conn_t();

	void handshake();
	void shutdown(bool is_timeout = false);

private:
	static const uint8_t MASK_BIT_SSL_NO_WAIT;
	static const uint8_t MASK_BIT_SSL_NO_SENT;
	static const uint8_t MASK_BIT_SSL_NO_ALL;

protected:
	virtual void close();
	virtual ssize_t recv(void *buf,size_t len);
	virtual ssize_t send(const void *buf,size_t len);
	int do_handshake();
	int do_shutdown(bool is_timeout);

private:
	void handshake_handler(short ev);
	void shutdown_handler(short ev);
	void read_handler(short ev);
	void write_handler(short ev);
	void empty_handler(short ev);

private:
	SSL *ssl_;
	uint8_t shutdown_flag_;
	io_handler old_read_handler_;
	io_handler old_write_handler_;
};

#endif
