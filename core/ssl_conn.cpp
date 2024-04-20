#if USE_SSL
#include "ssl.h"
#include "ssl_conn.h"
#include "ssl_ctx.h"
#include "constant.h"
#include "../log/log_send.h"
#include <assert.h>
#include <new>
#include <stdexcept>
using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////////
const uint8_t ssl_conn_t::MASK_BIT_SSL_NO_WAIT = 0x01;
const uint8_t ssl_conn_t::MASK_BIT_SSL_NO_SENT = 0x02;
const uint8_t ssl_conn_t::MASK_BIT_SSL_NO_ALL  = 0x03; //MASK_BIT_SSL_NO_WAIT|MASK_BIT_SSL_NO_SENT

ssl_conn_t::ssl_conn_t(ssl_ctx_t *ssl_ctx,int sock,const char* ip,int port)
:connection(sock,ip,port)
,old_read_handler_(NULL)
,old_write_handler_(NULL)
,shutdown_flag_(0)
{
	assert(ssl_ctx);
	ssl_ = SSL_new(*ssl_ctx);
	if(NULL==ssl_)
		throw bad_alloc();

	if(0==SSL_set_fd(ssl_,sock)) {
		SSL_free(ssl_);
		throw runtime_error("SSL_set_fd() failed");
	}
	SSL_set_accept_state(ssl_);
	SSL_set_mode(ssl_,SSL_MODE_ENABLE_PARTIAL_WRITE);

	read_handler_ = (io_handler)&ssl_conn_t::handshake_handler;
	/**
	the write_handler_ also can be ssl_conn_t::handshake_handler,but because it may perform unnecessary call when do SSL        handshake,so is less efficient than ssl_conn_t::empty_handler.
	*/
	write_handler_ = (io_handler)&ssl_conn_t::empty_handler;
	flag_ |= MASK_BIT_SSL;

}

ssl_conn_t::~ssl_conn_t()
{
	SSL_free(ssl_);
}

void ssl_conn_t::close()
{
	shutdown();
}

void ssl_conn_t::empty_handler(short ev)
{
}

/*
 * the callback function is used to call do_ssl_handshake again when do_ssl_handshake return OP_AGAIN and
 * the underlying buffer is readable or writable.
 */
void ssl_conn_t::handshake_handler(short ev)
{
	handshake();
}

//handshake directly instead of reading with MSG_PEEK first
//void ssl_conn_t::handle_handshake()
//{
//	char c;
//	ssize_t len = ::recv(sock_,&c,1,MSG_PEEK);
//	if(1==len){
//		if(c&0x80 || c&0x16){
//			handshake();			
//		}else{
//			set_event_callback(true,handle_read);
//			set_event_callback(false,handle_write);
//			handle_read();
//		}
//	}else if(-1==len && (EAGAIN==errno||EWOULDBLOCK==errno)){
//		return;
//	}else{
//		log_error("%d-%s:%d handle_ssl_handshake error=%d",get_fd(),get_ip(),get_port(),errno);	
//		throw io_exception(self_rd_error);
//	}
//}

void ssl_conn_t::handshake()
{
	int ret = do_handshake();	

	switch(ret){
		case OP_OK: 
			{
				const SSL_CIPHER * cipher = SSL_get_current_cipher(ssl_);
				const char* cipherName = SSL_CIPHER_get_name(cipher);
				log_info("ssl cipher name: %s",cipherName);

				const COMP_METHOD *comp = SSL_get_current_compression(ssl_);
				const char * compName = SSL_COMP_get_name(comp);
				log_info("ssl comp name: %s",compName);

				read_handler_ = &connection::handle_read;
				write_handler_ = &connection::handle_write;
				handle_read(EV_READ);
			}
			break;

		case OP_CLOSE:
		case OP_ERROR:
			shutdown_flag_ |= MASK_BIT_SSL_NO_WAIT;
			shutdown();
			break;
	}
}

int ssl_conn_t::do_handshake()
{
	ssl_clear_error();

	int ret = SSL_do_handshake(ssl_);
	if(1==ret){
		/* initial handshake done, disable renegotiation (CVE-2009-3555) */
		if (ssl_->s3)
			ssl_->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
		return OP_OK;
	}
	/**
	 * underlying buffer may be neither readable and nor writable in a moment,on this case,
	 * read or write callback function is called when the underlying buffer is readable or writable.
	 */
	int sslerr = SSL_get_error(ssl_,ret), err;
	switch(sslerr){
		case SSL_ERROR_WANT_READ:
			read_handler_ = (io_handler)&ssl_conn_t::handshake_handler;
			write_handler_ = (io_handler)&ssl_conn_t::/*handshake_handler*/empty_handler;	
			return OP_AGAIN;

		case SSL_ERROR_WANT_WRITE:
			read_handler_ = (io_handler)&ssl_conn_t::/*handshake_handler*/empty_handler;
			write_handler_ = (io_handler)&ssl_conn_t::handshake_handler;	
			return OP_AGAIN;

		case SSL_ERROR_ZERO_RETURN:
			log_warn("%d-%s:%d close in SSL handshake",get_fd(),get_ip(),get_port());
			return OP_CLOSE;

		default:
			err = (sslerr==SSL_ERROR_SYSCALL) ? errno : 0;
			log_error("%d-%s:%d error in SSL handshake: ssl error=%d,error=%d",get_fd(),get_ip(),get_port(),sslerr,err);
			ssl_error();
			return OP_ERROR;
	}					
}

ssize_t ssl_conn_t::recv(void *buf,size_t len)
{
	ssl_clear_error();

	int ret = SSL_read(ssl_,buf,len);
	if(ret>0){
		if(old_write_handler_){
			write_handler_ = old_write_handler_;
			old_write_handler_ = NULL;
			active_event(false);
		}
		return ret;
	}

	int sslerr = SSL_get_error(ssl_,ret), err;
	switch(sslerr){
		case SSL_ERROR_WANT_READ:
			return OP_AGAIN;

		case SSL_ERROR_WANT_WRITE:
			if(NULL==old_write_handler_){
				old_write_handler_ = write_handler_;
				write_handler_ = (io_handler)&ssl_conn_t::write_handler;
			}
			return OP_AGAIN;

		case SSL_ERROR_ZERO_RETURN:
			log_warn("%d-%s:%d close in SSL read",get_fd(),get_ip(),get_port());
			return OP_CLOSE;

		default:
			shutdown_flag_ = MASK_BIT_SSL_NO_ALL;
			err = (sslerr==SSL_ERROR_SYSCALL) ? errno : 0;
			log_error("%d-%s:%d error in SSL read: ssl error=%d,error=%d",get_fd(),get_ip(),get_port(),sslerr,err);
			ssl_error();
			return OP_ERROR;
	}
}

//the ssl_write_handler is used to call ssl_recv again when ssl_recv return OP_AGAIN and the underlying buffer is writable
void ssl_conn_t::write_handler(short ev)
{
	(this->*read_handler_)(EV_WRITE);
}

ssize_t ssl_conn_t::send(const void *buf,size_t len)
{
	ssl_clear_error();

	int ret = SSL_write(ssl_,buf,len);
	if(ret>0){
		if(old_read_handler_){
			read_handler_ = old_read_handler_;
			old_read_handler_ = NULL;
			active_event(true);
		}
		return ret;
	}

	int sslerr = SSL_get_error(ssl_,ret), err;
	switch(sslerr){
		case SSL_ERROR_WANT_WRITE:
			return OP_AGAIN;

		case SSL_ERROR_WANT_READ:
			if(NULL==old_read_handler_){
				old_read_handler_ = read_handler_;
				read_handler_ = (io_handler)&ssl_conn_t::read_handler;
			}
			return OP_AGAIN;

		case SSL_ERROR_ZERO_RETURN:
			log_warn("%d-%s:%d close in SSL write",get_fd(),get_ip(),get_port());
			return OP_CLOSE;

		default:
			shutdown_flag_ = MASK_BIT_SSL_NO_ALL;
			err = (sslerr==SSL_ERROR_SYSCALL) ? errno : 0;
			log_error("%d-%s:%d error in SSL write: ssl error=%d,error=%d",get_fd(),get_ip(),get_port(),sslerr,err);
			ssl_error();
			return OP_ERROR;
	}
}

//the ssl_read_handler is used to call ssl_send again when ssl_send return OP_AGAIN and the underlying buffer is readable
void ssl_conn_t::read_handler(short ev)
{
	(this->*write_handler_)(EV_READ);
}

void ssl_conn_t::shutdown(bool is_timeout/*=false*/)
{
	if (do_shutdown(is_timeout) != OP_AGAIN)
		delete this;
}

int ssl_conn_t::do_shutdown(bool is_timeout)
{
	int  ret,mode;

	if(is_timeout){
		log_info("%d-%s:%d timeout in SSL shutdown",get_fd(),get_ip(),get_port());
		mode = SSL_RECEIVED_SHUTDOWN|SSL_SENT_SHUTDOWN;
		SSL_set_quiet_shutdown(ssl_,1);
	}else{
		mode = SSL_get_shutdown(ssl_);
		if (shutdown_flag_&MASK_BIT_SSL_NO_WAIT)
			mode |= SSL_RECEIVED_SHUTDOWN;

		if (shutdown_flag_&MASK_BIT_SSL_NO_SENT)
			mode |= SSL_SENT_SHUTDOWN;

		if (shutdown_flag_&MASK_BIT_SSL_NO_ALL)
			SSL_set_quiet_shutdown(ssl_, 1);
	}
	SSL_set_shutdown(ssl_,mode);

	ssl_clear_error();

	ret = SSL_shutdown(ssl_);
	if(1==ret)
		return OP_OK;

	int sslerr = SSL_get_error(ssl_,ret), err;
	switch(sslerr){
		case SSL_ERROR_ZERO_RETURN:
			log_warn("%d-%s:%d close in SSL shutdown",get_fd(),get_ip(),get_port());
			return OP_CLOSE;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			read_handler_ = (io_handler)&ssl_conn_t::shutdown_handler;
			write_handler_ = (io_handler)&ssl_conn_t::shutdown_handler;

			if(SSL_ERROR_WANT_READ==sslerr){
				struct timeval tv;
				tv.tv_sec = 30,tv.tv_usec = 0;
				add_event(true,tv);
			}
			return OP_AGAIN;

		default: 
			err = (SSL_ERROR_SYSCALL==sslerr) ? errno : 0;
			log_error("%d-%s:%d error in SSL shutdown: ssl error=%d,error=%d",get_fd(),get_ip(),get_port(),sslerr,err);
			ssl_error();
			return OP_ERROR;
	}
}

void ssl_conn_t::shutdown_handler(short ev)
{
	shutdown(ev&EV_TIMEOUT);
}
#endif
