#include "connection.h"
#include "connector.h"
#include "worker.h"
#include "global.h"
#include "p2p_proxy.h"
#include "constant.h"
#include "../log/log_send.h"
#include "../base/cstring.h"
#include "../net/function.h"
#include "../loki/ScopeGuard.h"
#include <utility>
#include <cassert>
#include <string.h>
#include <errno.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#ifdef __x86_64__
#include <unistd.h>
#endif
using namespace base;
using namespace memory;
using namespace std;

/***************** ChangeLog *********************************************************************************
2012-8-23	use edge-triggered instead of level-triggered,combine ev_et flag with ev_read or ev_write,
			otherwise write event will be triggered ceaselessly,cause high cpu usage. 

2012-8-24	call async_send to send data after append buffer,when underlying socket buffer is writable,
			handle_write can been called to send data automatically.

2012-12-25	add two functions: match_server and match_client,add class server_conn_table,change connection_table
			into client_conn_table
2013-5-6	add flow control between two matched connections
2013-5-20	add timeout mask to improve response timeout handle
2013-10-30  correct one bug about server match server or client match client
2013-11-22	improve parse_request_uri into handle_uri
2013-12-24	modify request_new_server: add ssl_port field to send if support https service mode,and set port or 
			ssl_port to zero according to whether client request connects ssl listen port or not
**************************************************************************************************************/

//////////////////////// class connection definition ///////////////////////
const uint8_t connection::CLIENT      = 0x01;
const uint8_t connection::IDLE_SERVER = 0x02;
const uint8_t connection::BUSY_SERVER = 0x03;

const uint8_t connection::MASK_BIT_RD          = 0x01;
const uint8_t connection::MASK_BIT_WR          = 0x02;
const uint8_t connection::MASK_BIT_RDWR	       = 0x03;//MASK_BIT_RD|MASK_BIT_WR
const uint8_t connection::MASK_BIT_ENABLE_RD   = 0x04;
const uint8_t connection::MASK_BIT_CONNECTED   = 0x08;
const uint8_t connection::MASK_BIT_TIMEOUT     = 0x10;
const uint8_t connection::MASK_BIT_CLOSE       = 0x20;
const uint8_t connection::MASK_BIT_BROWSER     = 0x40;
const uint8_t connection::MASK_BIT_SSL	       = 0x80;	

struct uri_info
{
	bool is_client;

	char *dev_id_beg;
	char *dev_id_end;

	union {
		struct {
			int sock;
		}server;

		struct {
			char *cb_beg;
			char *cb_end;
		}client;
	};
};

static void get_uri_info(int verb,char *uri,size_t uri_len,uri_info *ui)
{
	static const char sc_match1[] = "/proxy/response?Ver=1&Cmd=2&DeviceID=";
	static const char sc_match2[] = "&ConnectID=";
	static const char sc_match3[] = "DeviceID=";
	static const char sc_match4[] = "callback=";
	static const char sc_match5   = '&';

	char *uri_beg = uri, *uri_end = uri + uri_len, old = *uri_end;
	*uri_end = '\0';

	char* dev_id_beg = strcasestr(uri_beg,sc_match1),*dev_id_end = NULL;
	size_t len;

	if(dev_id_beg && HTTP_POST==verb){
		dev_id_beg += sizeof(sc_match1)-1;
		dev_id_end = strcasestr(dev_id_beg,sc_match2);
		if(dev_id_end){
			char *sock_beg = dev_id_end + sizeof(sc_match2) - 1;
			char *sock_end = strchr(sock_beg,sc_match5);
			if(NULL==sock_end)
				sock_end = uri_end;
			ui->server.sock = atoi(sock_beg);
		}
		ui->is_client = false;
	}else{
		dev_id_beg = strcasestr(uri_beg,sc_match3);
		if(dev_id_beg){
			dev_id_beg += sizeof(sc_match3)-1,
			dev_id_end = strchr(dev_id_beg,sc_match5);
			if(NULL==dev_id_end) 
				dev_id_end = uri_end;
		}
		char *cb_beg = strcasestr(uri_beg,sc_match4),*cb_end = NULL;
		if(cb_beg){
			cb_end=strchr(cb_beg+=sizeof(sc_match4)-1,sc_match5);
			if(NULL==cb_end) cb_end = uri_end;
		}
		ui->is_client = true;
		ui->client.cb_beg = cb_beg;
		ui->client.cb_end = cb_end;
	}
	ui->dev_id_beg = dev_id_beg;
	ui->dev_id_end = dev_id_end;

	*uri_end = old;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
connection::connection(int sock,const char* ip,uint16_t port)
:other_(NULL)
,sock_(sock)
,type_(CLIENT)
,hstream_(this)
,cb_tail_(NULL)
,host_(NULL)
,flag_(MASK_BIT_CONNECTED)
,port_(port)
,ip_(ip)
,read_handler_(&connection::handle_read)
,write_handler_(&connection::handle_write)
{
	client_conn_table::instance()->add(this);
}

connection::~connection()
{
	detach();
	if(__sync_bool_compare_and_swap(&type_,CLIENT,CLIENT))
		client_conn_table::instance()->remove(sock_);
	else
		server_conn_table::instance()->remove(this);
	::close(sock_);
}

ssize_t connection::recv(void *buf,size_t size)
{
	ssize_t ret = ::recv(sock_,buf,size,0);
	if(ret > 0){

	}else if(-1==ret && (errno==EAGAIN||errno==EWOULDBLOCK))
		ret = OP_AGAIN;
	else if(-1==ret){
		ret = OP_ERROR;
		log_error("%d-%s:%d error=%d in tcp_recv",sock_,ip_.c_str(),port_,errno);
	}else{
		ret = OP_CLOSE;
		log_warn("%d-%s:%d close in tcp_recv",sock_,ip_.c_str(),port_);
	}
	return ret;
}

ssize_t connection::send(const void *buf,size_t size)
{
	ssize_t ret = ::send(sock_,buf,size,0);
	if(ret > 0){

	}else if(-1==ret && (errno==EAGAIN||errno==EWOULDBLOCK))
		ret = OP_AGAIN;
	else if(-1==ret){
		ret = OP_ERROR;
		log_error("%d-%s:%d error=%d in tcp_send",sock_,ip_.c_str(),port_,errno);
	}else{
		ret = OP_CLOSE;
		log_warn("%d-%s:%d close in tcp_send",sock_,ip_.c_str(),port_);
	}
	return ret;
}

void connection::attach(worker *host)
{
	attach(host,handle_read,handle_write);
}

void connection::attach(worker *host,event_callback_fn rfn,event_callback_fn wfn)
{
	assert(host);

	conn_bucket* cb_tail = &host->cb_.back();
	std::pair<conn_bucket::iterator,bool> p = cb_tail->insert(this);
	assert(p.second);	

	if(event_assign(&ev_write_,host->ev_base_,sock_,EV_WRITE|EV_PERSIST|EV_ET,wfn,this)
		||event_add(&ev_write_,NULL))
		throw runtime_error("");
	flag_ |= MASK_BIT_WR;

	if(event_assign(&ev_read_,host->ev_base_,sock_,EV_READ|EV_PERSIST|EV_ET,rfn,this)
		||event_add(&ev_read_,NULL))
		throw http_exception(HTTP_PROXY_SERVER_INTERNAL_ERROR);

	flag_ |= MASK_BIT_RD|MASK_BIT_ENABLE_RD;

	if(evtimer_assign(&ev_timer_,host->ev_base_,handle_timeout,this))
		throw http_exception(HTTP_PROXY_SERVER_INTERNAL_ERROR);

	cb_tail_ = cb_tail;
	host_    = host;
}

void connection::detach()
{
	if(host_){
		if(flag_&MASK_BIT_RD){
		    if(-1==event_del(&ev_read_))
				log_fatal("connection %d-%s:%d detach event_del read fail",sock_,ip_.c_str(),port_);
		   	flag_ &= ~MASK_BIT_RD;
		}
		if(flag_&MASK_BIT_WR){
	  	    if(-1==event_del(&ev_write_))
				log_fatal("connection %d-%s:%d detach event_del write fail",sock_,ip_.c_str(),port_);
	 		flag_ &= ~MASK_BIT_WR;
		}
		if(flag_&MASK_BIT_TIMEOUT) {
			event_del(&ev_timer_);
			flag_ &= ~MASK_BIT_TIMEOUT;
		}
	}
	if(cb_tail_) cb_tail_->erase(this);
}

void connection::send_error(int error)
{
	assert(host_);
	if((!(flag_&MASK_BIT_WR)) && event_add(&ev_write_,NULL)){
		log_error("connection %d-%s:%d add write fail in send_error",sock_,ip_.c_str(),port_);
		close(SELF_WR_ERROR); 
		return;
	}

	if(flag_&MASK_BIT_RD){
		event_del(&ev_read_);
		flag_ &= ~MASK_BIT_RD;
	}
	try{
		hstream_.send_error(error);
	}catch(...){
		close(SELF_WR_ERROR);
	}
}

void connection::match_client(int sock)
{
	int error;
	worker* host;

	connection* conn = client_conn_table::instance()->get(sock,&host);
	if(conn && this!=conn){
		assert(host && host_);
		assert(NULL==other_);
		if(host_ != host){
			assert(cb_tail_);
			detach();
			if(host->notify_match_conn(this,sock,true))
				return;
			error = HTTP_PROXY_SERVER_INTERNAL_ERROR;
		}else if(NULL==conn->other_){
			if(!strcasecmp(conn->dev_id_.c_str(),dev_id_.c_str())){
				match(conn,true);  return;
			}else{
				error = HTTP_DEVICE_ID_DIFFERENT;			
				log_fatal("connection::match_client %d-%s:%d and %d-%s:%d device id are different:%s,%s",
					sock_,ip_.c_str(),port_,sock,conn->ip_.c_str(),conn->port_,dev_id_.c_str(),conn->dev_id_.c_str());
			}
		}else{
			error = HTTP_MATCHED_CONNECTION;		
			log_fatal("connection::match_client %d-%s:%d had already matched",sock,conn->ip_.c_str(),conn->port_);
		}
	}else if(NULL==conn){
		error = HTTP_NULL_CONNECTION;	
		log_fatal("connection::match_client %d was dead",sock);
	}else{
		error = HTTP_CONFLICT_CONNECTION;	
		log_fatal("connection::match_client %d-%s:%d and %d-%s:%d are conflicting",sock_,ip_.c_str(),port_,sock,
				conn->ip_.c_str(),conn->port_);
	}

	throw http_exception(error);
}

void connection::match_server(const char* dev_id,bool reuse_server/*=false*/)
{
	if(dev_id){
		try{
			dev_id_.assign(dev_id);
		}catch(...){
			throw http_exception(HTTP_PROXY_SERVER_INTERNAL_ERROR);
		}
	}
	if(!reuse_server) {
		request_new_server();  return;
	}	

	worker* host; int sock;
	connection* conn = server_conn_table::instance()->get_idle(dev_id_,&sock,&host);
	if(conn){
		assert(host && host_);
		if(host_ != host){
			assert(cb_tail_);
			detach();
			if(!host->notify_match_conn(this,sock,false))
				throw http_exception(HTTP_PROXY_SERVER_INTERNAL_ERROR);
		}else
			match(conn,false);
	}else
		request_new_server(); 
}

//when the match is called,sock corresponding connection and this must be in the same thread
void connection::match(int sock,bool is_client)
{
	assert(-1!=sock);
	connection* conn; worker* host;

	if(is_client){
		conn = client_conn_table::instance()->get(sock,&host);

		int error;
		if(NULL==conn){
			error = HTTP_NULL_CONNECTION;		
			log_fatal("connection::match_client %d was dead",sock);
		}else if(host != host_){
			error = HTTP_INVALID_CONNECTION;
			log_fatal("connection::match_client %d-%s:%d and %d-%s:%d are not in same thread",sock_,ip_.c_str(),
					  port_,sock,conn->ip_.c_str(),conn->port_);
		}else if(conn->other_){
			error = HTTP_MATCHED_CONNECTION;
			log_fatal("connection::match_client %d-%s:%d had already matched",sock,conn->ip_.c_str(),conn->port_);
		}else if(strcasecmp(conn->dev_id_.c_str(),dev_id_.c_str())){
			log_fatal("connection::match_client %d-%s:%d and %d-%s:%d device id are different:%s,%s",sock_,ip_.c_str(),
					  port_,sock,conn->ip_.c_str(),conn->port_,dev_id_.c_str(),conn->dev_id_.c_str());
			error = HTTP_DEVICE_ID_DIFFERENT;			
		}else{
			match(conn,true);  return;
		}
		throw http_exception(error);
	}else{
		conn = server_conn_table::instance()->get(dev_id_.c_str(),sock,&host);
		//this server connection had already been closed or matched
		if(NULL==conn||host != host_||conn->other_)
		// use either request_new_server or match_server,but which one is better is difficult and uncertain
			request_new_server();				
		else //it exist and have not yet matched
			match(conn,false);
	}
}

//when the match is called,conn and this must be in the same thread
void connection::match(connection* conn,bool is_client)
{
	assert(conn && NULL==conn->other_);
	assert(NULL==other_);

	if(is_client){
		if(!(conn->flag_&MASK_BIT_TIMEOUT)){
			log_fatal("connection match_client %d-%s:%d is not client",conn->sock_,conn->ip_.c_str(),conn->port_);
			throw http_exception(HTTP_INVALID_MATCH);
		}
		event_del(&conn->ev_timer_);
		conn->flag_ &= ~MASK_BIT_TIMEOUT;

		/**
		 * first send http ok response status code to server,then if other had existed,then retransmit data,
		 * push all data of other connection's input queue into this connection's output queue. 
		 */		
		hstream_.send_ok(false,http_stream::nosend);
		out_queue_.push(conn->in_queue_);
		async_send();
		assert(__sync_bool_compare_and_swap(&type_,IDLE_SERVER,IDLE_SERVER));
		__sync_lock_test_and_set(&type_,BUSY_SERVER);

		log_debug("server %d-%s:%d and client %d-%s:%d match,start retransmit...",
			sock_,ip_.c_str(),port_,conn->sock_,conn->ip_.c_str(),conn->port_);
	}else{
		conn->out_queue_.push(in_queue_);
		conn->async_send();		
		__sync_lock_test_and_set(&conn->type_,BUSY_SERVER);

		log_debug("client %d-%s:%d and server %d-%s:%d match,start retransmit...",
			sock_,ip_.c_str(),port_,conn->sock_,conn->ip_.c_str(),conn->port_);
	}
	other_ = conn, conn->other_ = this;
}

void connection::request_new_server()
{
	log_debug("%d-%s:%d request_new_server",sock_,ip_.c_str(),port_);

	p2p_proxy_stun_msg msg;
	msg.ver	 = 1;
	msg.size = htons(P2P_PROXY_STUN_MSG_SIZE);
	msg.type = P2P_PROXY_STUN_MSG_REQUEST;
	msg.sock = htonl(sock_);
	msg.pub_addr = g_conf.pub_ip_;
	msg.port = htons(g_conf.port_);
#if USE_SSL
	uint16_t ssl_port = g_conf.ssl_.port_;
	if(!(flag_&MASK_BIT_SSL))
		ssl_port = 0;
	msg.ssl_port = htons(ssl_port);
#endif
	
	assert(dev_id_.size()<=DEV_ID_MAX_LEN);
	strcpy(msg.dev_id,dev_id_.c_str());
	
	struct timeval tv;
	tv.tv_sec = g_conf.timeout_respond_, tv.tv_usec = 0;

	if(flag_&MASK_BIT_TIMEOUT){
		event_del(&ev_timer_);
		flag_ &= ~MASK_BIT_TIMEOUT;
	}
	if(evtimer_add(&ev_timer_,&tv)||!singleton<connector>::instance()->async_send(&msg,sizeof(msg)))
		throw http_exception(HTTP_PROXY_SERVER_INTERNAL_ERROR);
	flag_ |= MASK_BIT_TIMEOUT;
}

void connection::handle_uri(buffer *&buf,int verb,char *uri,size_t uri_len)
{	
	assert(buf);

	uri_info ui;
	get_uri_info(verb,uri,uri_len,&ui);

	if(ui.is_client && ui.client.cb_beg){
		callback_.assign(ui.client.cb_beg,ui.client.cb_end);
		flag_ |= MASK_BIT_BROWSER;
	}else
		flag_ &= ~MASK_BIT_BROWSER;

	if(NULL==ui.dev_id_beg || NULL==ui.dev_id_end)
		throw http_exception(HTTP_BAD_REQUEST);

	size_t len = ui.dev_id_end - ui.dev_id_beg;
	if(0==len || len>DEV_ID_MAX_LEN)
		throw http_exception(HTTP_BAD_REQUEST);
		
	if(ui.is_client){
		char dev_id[DEV_ID_MAX_LEN+1];
		
		strncpy(dev_id,ui.dev_id_beg,len);
		len = http_decode_uri(dev_id,len);
		dev_id[len] = '\0'; 

		if(other_ && !strcasecmp(dev_id_.c_str(),dev_id)){
			other_->out_queue_.push(buf); buf = NULL;
			other_->async_send(OTHER_WR_ERROR);
		}else{
			in_queue_.push(buf); buf = NULL;
			if(other_){
				assert(__sync_bool_compare_and_swap(&other_->type_,BUSY_SERVER,BUSY_SERVER));
				other_->close();  other_ = NULL;
			}
			match_server(dev_id);
		}
	}else{
		/**
		 * if other_ is not null,this case indicates response from client that had matched,it should not occur,
		 * if occur,proxy server response bad request to client
		 */
		if(other_)
			throw http_exception(HTTP_BAD_REQUEST);

		len = http_decode_uri(ui.dev_id_beg,len);
		dev_id_.assign(ui.dev_id_beg,len);

		bool ret = client_conn_table::instance()->remove(sock_);
		assert(ret);
		server_conn_table::instance()->add(this);
		__sync_lock_test_and_set(&type_,IDLE_SERVER);

		match_client(ui.server.sock);
	}
}

void connection::enable_read()
{
	if(!(flag_&MASK_BIT_ENABLE_RD)){
		event_active(&ev_read_,EV_READ,1);
		flag_ |= MASK_BIT_ENABLE_RD;
	}
}

void connection::async_send_impl(uint8_t flag)
{
	ssize_t ret; buffer* buf;

	while(!out_queue_.empty()) {
		buf = out_queue_.top();
		ret = send(buf->data_+buf->dtran_,buf->dlen_-buf->dtran_);

		if(ret > 0) {
			buf->dtran_ += ret;
			out_queue_.consume_size(ret);
			if(buf->dtran_ == buf->dlen_) {
				out_queue_.pop(false);
				delete buf;
			}
		}else if(OP_AGAIN==ret){
			return;
		}else{
			//the connection had occurred error or been closed
			flag_ |= MASK_BIT_CLOSE;
			log_debug("%d-%s:%d connection async_send fail",sock_,ip_.c_str(),port_);
			break;
		}
	}

	if(flag_ & MASK_BIT_CLOSE)
		throw io_exception(flag);
	else if(NULL==other_&&__sync_bool_compare_and_swap(&type_,BUSY_SERVER,BUSY_SERVER))
		__sync_lock_test_and_set(&type_,IDLE_SERVER);
}

void connection::async_send(uint8_t flag/*=SELF_WR_ERROR*/)
{
	update();
	async_send_impl(flag);

	if(other_) {
		struct tcp_info ti;
		socklen_t len=sizeof(tcp_info);
		getsockopt(other_->sock_,IPPROTO_TCP,TCP_INFO,(char*)&ti,&len);

		if((other_->flag_&MASK_BIT_CONNECTED)&&ti.tcpi_state!=TCP_ESTABLISHED){
			other_->flag_ &= ~MASK_BIT_CONNECTED;
			other_->enable_read();
		}else if(out_queue_.is_low()){
			other_->enable_read();
		}
	}
}

void connection::close()
{
	delete this;
}

void connection::close(uint8_t flag,bool reuse_server/*=false*/)
{
	switch(flag){
		case SELF_RD_ERROR:
		case SELF_WR_ERROR:
			if(__sync_bool_compare_and_swap(&type_,CLIENT,CLIENT) && other_){
				if(!hstream_.is_data_break()){
					if(reuse_server){
						other_->other_ = NULL;
						other_->enable_read();
						if(other_->out_queue_.empty())
							__sync_lock_test_and_set(&other_->type_,IDLE_SERVER);
					}else if(SELF_RD_ERROR==flag)
						other_->flag_ |= MASK_BIT_CLOSE, other_->other_ = NULL;
					else
						other_->close();
				}else{
					other_->close(); other_ = NULL;
				}
			}else if(other_){
				if(SELF_RD_ERROR==flag)  
					other_->flag_ |= MASK_BIT_CLOSE, other_->other_ = NULL;
				else
					other_->close();
			}
			if(SELF_RD_ERROR==flag && other_){
				try{
					other_->async_send();
				}catch(...){
					other_->close();
				}
			}	
			close();
			break;

		case OTHER_WR_ERROR:
			assert(other_);
			other_->close();

			if(!reuse_server||__sync_bool_compare_and_swap(&type_,CLIENT,CLIENT)){
				close();
			}else{
				other_ = NULL; 
				enable_read();
				if(out_queue_.empty())
					__sync_lock_test_and_set(&type_,IDLE_SERVER);
			}
			break;

		case IDLE_TIMEOUT:
			if(NULL==other_)
				close();
			else if(!__sync_bool_compare_and_swap(&type_,CLIENT,CLIENT)){
				other_->other_ = NULL;
				close();
			}
			break;

		default: assert(false); break;
	}
}

void connection::handle_read(short ev)
{
	if(!is_enable_read()) return;
	
	try{
		update();
		if(__sync_bool_compare_and_swap(&type_,CLIENT,CLIENT)){
			hstream_.read_request();
		}else if(other_)
			hstream_.read_response();	
		else
			close();
	}catch(http_exception& he){
		send_error(he.code_);
	}catch(io_exception& ie){
		close(ie.flag_);
	}catch(std::exception& se){
		close(SELF_RD_ERROR);
		log_fatal("connection handle_read exception: %s",se.what());
	}catch(...){
		close(SELF_RD_ERROR);
		log_fatal("connection handle_read unknow exception");
	}
}

void connection::handle_read(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_READ))
		 return;
	connection *conn = static_cast<connection*>(arg);
	(conn->*(conn->read_handler_))(ev);
}

void connection::handle_write(short ev)
{
	try{
		async_send();
	}catch(io_exception& ie){
		close(ie.flag_);
	}catch(...){
		close(SELF_WR_ERROR);
		log_fatal("connection handle_write unknow exception");
	}
}

void connection::handle_write(evutil_socket_t fd, short ev, void *arg)
{
	if(!(ev&EV_WRITE))
		 return;
	connection *conn = static_cast<connection*>(arg);
	(conn->*(conn->write_handler_))(ev);
}

void connection::handle_timeout(evutil_socket_t fd, short ev, void *arg)
{
	connection *conn = static_cast<connection*>(arg);
	log_debug("connection handle_timeout: %d-%s:%d",conn->sock_,conn->ip_.c_str(),conn->port_);
		
	assert(NULL==conn->other_);
	assert(conn->flag_&MASK_BIT_TIMEOUT);

	conn->send_error(HTTP_ORIGINAL_SERVER_TIMEOUT);
}

inline void connection::update()
{
	assert(host_ && cb_tail_);

	conn_bucket* cb_tail = &host_->cb_.back();
	if(cb_tail_!=cb_tail){
		cb_tail_->erase(this);
		cb_tail_ = cb_tail;
		cb_tail_->insert(this);
	}
}

void connection::add_event(bool is_read,const struct timeval &tv)
{
	struct event *ev = NULL;
	uint8_t flag;

	if(is_read && (flag_ & MASK_BIT_RD)){
		ev = &ev_read_;
		flag = MASK_BIT_RD;
	}else if(flag_ &MASK_BIT_WR){
		ev = &ev_write_;
		flag = MASK_BIT_WR;
	}

	if(ev && event_add(ev,&tv)){
		flag_ &= ~flag;
		throw runtime_error("");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
client_conn_table::client_conn_table()
{
}

client_conn_table::~client_conn_table()
{
}

void client_conn_table::add(connection* conn)
{ 
	lock_guard<base::mutex> guard(lock_);
	pair<iterator,bool> ret = conns_.insert(make_pair(conn->sock_,conn));
	assert(ret.second); 
}

bool client_conn_table::remove(int sock)
{
	iterator it;
	lock_guard<mutex> guard(lock_);

	it = conns_.find(sock);
	if(it == conns_.end()) {
		log_warn("client_conn_table remove can not find %d",sock);
		return false;
	}
	conns_.erase(it);
	return true;
}

connection* client_conn_table::get(int sock,worker** host/*=NULL*/) const
{
	const_iterator it;
	lock_guard<mutex> guard(lock_);

	it = conns_.find(sock);
	if(it == conns_.end()) return NULL;

	connection* conn = it->second;
	if(host) *host = conn->host_;
	return conn;
}

size_t client_conn_table::match_size() const
{
	size_t num = 0;
	const_iterator it;
	lock_guard<mutex> guard(lock_);
	
	for(it=conns_.begin();it!=conns_.end();++it) {
		if(it->second->other_)
			++num;
	}	
	return num;
}

//////////////////////////////////////////////////////////////////////////
server_conn_table::server_conn_table()
{
}

server_conn_table::~server_conn_table()
{
}

void server_conn_table::add(connection* conn)
{
	lock_guard<base::mutex> guard(lock_);
	conns_.insert(make_pair(conn->dev_id_,conn));
}

bool server_conn_table::remove(connection* conn)
{
	pair<iterator,iterator> its;
	iterator it;
	lock_guard<base::mutex> guard(lock_);

	its = conns_.equal_range(conn->dev_id_);
	for(it = its.first;it != its.second;++it){
		if(it->second==conn) {
			conns_.erase(it); 
			return true;
		}
	}
	return false;
}

connection* server_conn_table::get(int sock) const
{
	const_iterator it;
	connection* conn;
	lock_guard<base::mutex> guard(lock_);

	for(it = conns_.begin(); it != conns_.end(); ++it){
		conn = it->second;
		if(conn->sock_==sock)
			return conn;
	}
	return NULL;
}

connection* server_conn_table::get(const string_t& dev_id,int sock,worker** host/*=NULL*/) const
{
	pair<const_iterator,const_iterator> its;
	const_iterator it;
	connection* conn;
	lock_guard<base::mutex> guard(lock_);
	
	its = conns_.equal_range(dev_id);
	for(it = its.first;it != its.second;++it){
		conn = it->second;
		if(conn->sock_==sock){ 
			if(host) *host = conn->host_;
			return conn;
		}
	}
	return NULL;
}

/**
 * read or write one byte is always atomic in multi-thread,but compare and swap are two operations,so
 * need to be synchronized
 */
connection* server_conn_table::get_idle(const string_t& dev_id,int* sock/*=NULL*/,worker** host/*=NULL*/) const
{
	pair<const_iterator,const_iterator> its;
	const_iterator it;
	connection* conn;
	lock_guard<base::mutex> guard(lock_);

	its = conns_.equal_range(dev_id);
	for(it = its.first;it != its.second;++it){
		conn = it->second;
		assert(conn); 
		if(__sync_bool_compare_and_swap(&conn->type_,connection::IDLE_SERVER,connection::BUSY_SERVER)){
			if(host) *host = conn->host_; 
			if(sock) *sock = conn->sock_;
			return conn;
		}
	}
	return NULL;
}
