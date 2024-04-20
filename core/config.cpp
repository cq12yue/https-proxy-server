#include "config.h"
#if USE_SSL
#include "ssl.h"
#endif
#include "../log/log_send.h"
#include "../base/ini_file.h"
#include "../base/util.h"
#include "../net/net_addr.h"
#include <strings.h> //for strcasecmp
#include <arpa/inet.h> //for inet_ntop
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
using namespace netcomm;
using namespace base;
using namespace std;

static bool get_string(const base::ini_file &ini,const string &sec,const string &key,uint32_t &mask,uint32_t bit,
					   string_t &val,const string_t &def_val="")
{
	string str;
	if(!ini.get_value_string(sec,key,str)){
		val = def_val;
		log_error("config get [%s]->(%s) fail",sec.c_str(),key.c_str());
		return false;
	}
	if(strcasecmp(val.c_str(),str.c_str())){
		mask |= bit;		
		val.assign(str.c_str());
	}else
		mask &= ~bit;
	return true;
}

template<typename T>
static bool get_integer(const base::ini_file &ini,const string &sec,const string &key,uint32_t &mask,uint32_t bit,
						T &val,const T &def_val=(T)0)
{
	T _val;
	if(!ini.get_value_number(sec,key,_val)){
		val = def_val;
		log_error("config get [%s]->(%s) fail",sec.c_str(),key.c_str());
		return false;
	}
	if(val != _val){
		mask |= bit;
		val = _val;
	}else
		mask &= ~bit;	
	return true;
}

template<typename T>
static bool get_integer(const base::ini_file &ini,const string &sec,const string &key,uint32_t &mask,uint32_t bit,
						base::atomic_base<T> &val,const T &def_val=(T)0)
{
	T _val = val;
	bool ret = get_integer(ini,sec,key,mask,bit,_val,def_val);
	val = _val;		
	return ret;
}

//////////////////////////////////////////////////////////////////////////
const uint32_t config::MASK_BIT_LOCAL           = 0x00000001;
const uint32_t config::MASK_BIT_P2P             = 0x00000002;
const uint32_t config::MASK_BIT_DISPATCH        = 0x00000004;
const uint32_t config::MASK_BIT_PUB_ADDR        = 0x00000008;
const uint32_t config::MASK_BIT_TIMEOUT_IDLE    = 0x00000010;
const uint32_t config::MASK_BIT_TIMEOUT_RESPOND = 0x00000020;
const uint32_t config::MASK_BIT_INTERVAL_REPORT = 0x00000040;
const uint32_t config::MASK_BIT_LIMIT_FLOW      = 0x00000080;
const uint32_t config::MASK_BIT_LIMIT_RATE      = 0x00000100;
const uint32_t config::MASK_BIT_CROSS_DOMAIN	= 0x00000200;
const uint32_t config::MASK_BIT_ALL             = 0xFFFFFFFF;

#if USE_SSL
const uint32_t config::MASK_BIT_SSL_INFO        = 0x80000000;
#endif

config::config()
:changed_handler_(NULL)
,data_(NULL)
{
}

bool config::set(const base::ini_file &ini,uint32_t mask,bool is_notify_changed)
{
	uint32_t _mask;

	if(mask&MASK_BIT_LOCAL){
		get_string(ini,"host","address",_mask,MASK_BIT_LOCAL,address_,"*");
		if(address_.empty()){
			log_error("config address can not be empty");
			return false;
		}
		if(get_integer(ini,"host","port",_mask,MASK_BIT_LOCAL,port_) && 0==port_){
			log_error("config port can not be zero");
			return false;
		}		
	}
	if(mask&MASK_BIT_PUB_ADDR){
		if(!get_string(ini,"host","pub_addr",_mask,MASK_BIT_PUB_ADDR,pub_addr_))
			return false;
		if(pub_addr_.empty()){
			log_error("config pub_addr can not be empty");
			return false;
		}	
		addrinfo hints,*res;
		memset(&hints,0,sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		int ret;
		if(ret=getaddrinfo(pub_addr_.c_str(),NULL,&hints,&res)){
			log_fatal("config getaddrinfo fail:%s,%s",pub_addr_.c_str(),gai_strerror(ret));
			return false;
		}
		net_addr na(res->ai_addr,res->ai_addrlen);
		pub_ip_ = na.ip4_addr()->s_addr;
		freeaddrinfo(res);

		char ip[INET6_ADDRSTRLEN] = {'\0'};
		struct in_addr in = {pub_ip_};
		inet_ntop(AF_INET,&in,ip,sizeof(ip));
		log_info("config pub_ip=%s",ip);
	}
	if(mask&MASK_BIT_P2P){
		if(!get_string(ini,"p2p","p2p_addr",_mask,MASK_BIT_P2P,p2p_addr_))
			return false;
		if(p2p_addr_.empty()){
			log_error("config p2p_addr can not be empty");
			return false;
		}
		if(!get_integer(ini,"p2p","p2p_port",_mask,MASK_BIT_P2P,p2p_port_))
			return false;
		if(0==p2p_port_){
			log_error("config p2p_port can not be zero");
			return false;
		}
		get_integer(ini,"p2p","p2p_enable",_mask,MASK_BIT_P2P,p2p_enable_,1);
	}
	if(mask&MASK_BIT_DISPATCH){
		if(!get_string(ini,"dispatch","disp_addr",_mask,MASK_BIT_DISPATCH,disp_addr_))
			return false;
		if(disp_addr_.empty()){
			log_error("config disp_addr can not be empty");
			return false;
		}
		if(!get_integer(ini,"dispatch","disp_port",_mask,MASK_BIT_DISPATCH,disp_port_))	
			return false;
		if(0==disp_port_){
			log_error("config disp_port can not be zero");
			return false;
		}
		get_integer(ini,"dispatch","disp_enable",_mask,MASK_BIT_P2P,disp_enable_,1);
	}
	if(mask&MASK_BIT_TIMEOUT_IDLE){
		get_integer(ini,"time","timeout_idle",_mask,MASK_BIT_TIMEOUT_IDLE,timeout_idle_,600);
	}
	if(mask&MASK_BIT_TIMEOUT_RESPOND){
		get_integer(ini,"time","timeout_respond",_mask,MASK_BIT_TIMEOUT_RESPOND,timeout_respond_,60);
	}
	if(mask&MASK_BIT_INTERVAL_REPORT){
		get_integer(ini,"time","interval_report",_mask,MASK_BIT_INTERVAL_REPORT,interval_report_,30);
	}
	if(mask&MASK_BIT_LIMIT_FLOW){
		get_integer(ini,"control","limit_flow",_mask,MASK_BIT_LIMIT_FLOW,limit_flow_,0L);
	}
	if(mask&MASK_BIT_LIMIT_RATE){
		get_integer(ini,"control","limit_rate",_mask,MASK_BIT_LIMIT_RATE,limit_rate_,0L);
	}
	if(mask&MASK_BIT_CROSS_DOMAIN){
		if(!get_string(ini,"http","cross_domain",_mask,MASK_BIT_CROSS_DOMAIN,cross_domain_file_)){
			log_error("the cross domain file must be exist");
			return false;
		}
	}

#if USE_SSL
	if(mask&MASK_BIT_SSL_INFO){
		get_integer(ini,"ssl","ssl_enable",_mask,MASK_BIT_SSL_INFO,ssl_.enable_,1);
		if(!ssl_.enable_){
			if(0==port_) port_ = 80;
			ssl_.port_ = 0;
			return true;
		}

		if(get_integer(ini,"ssl","ssl_port",_mask,MASK_BIT_SSL_INFO,ssl_.port_,(uint16_t)443) && 0==ssl_.port_){
			log_error("ssl_port can not be zero");
			return false;
		}
		if(ssl_.port_ == port_){
			log_error("ssl_port %d can not be same with port %d",ssl_.port_,port_);
			return false;
		}
		
		if(!get_string(ini,"ssl","ssl_cert",_mask,MASK_BIT_SSL_INFO,ssl_.cert_))
			return false;	
		if(!get_string(ini,"ssl","ssl_key",_mask,MASK_BIT_SSL_INFO,ssl_.key_))
			return false;	
		
		get_string(ini,"ssl","ssl_keypass",_mask,MASK_BIT_SSL_INFO,ssl_.keypass_);

		string_t str;
		get_string(ini,"ssl","ssl_protocols",_mask,MASK_BIT_SSL_INFO,str,"SSLv2 SSLv3 TLSv1 TLSv1.1 TLSv1.2");
		ssl_.protocols_ = ssl_string_to_protocol(str);
		if(0==ssl_.protocols_){
			log_error("SSL Protocol must be specified");
			return false;
		}

		get_string(ini,"ssl","ssl_ciphers",_mask,MASK_BIT_SSL_INFO,ssl_.ciphers_,"HIGH:!aNULL:!MD5");
		get_string(ini,"ssl","ssl_dhparam",_mask,MASK_BIT_SSL_INFO,ssl_.dhparam_);
		get_string(ini,"ssl","ssl_ecdh_curve",_mask,MASK_BIT_SSL_INFO,ssl_.ecdh_curve_);
		
		get_integer(ini,"ssl","ssl_verify",_mask,MASK_BIT_SSL_INFO,ssl_.verify_,0);
		if(ssl_.verify_){
			get_integer(ini,"ssl","ssl_depth",_mask,MASK_BIT_SSL_INFO,ssl_.depth_,1);
			if(ssl_.depth_<0)
				ssl_.depth_ = 1;
			if(!get_string(ini,"ssl","ssl_ca",_mask,MASK_BIT_SSL_INFO,ssl_.ca_))
				return false;
			get_string(ini,"ssl","ssl_crl",_mask,MASK_BIT_SSL_INFO,ssl_.crl_);
		}
	}
#else
	if(0==port_) port_ = 80;
#endif
	
	if(is_notify_changed && changed_handler_)
		changed_handler_(this,_mask,data_);

	return true;
}

void config::print()
{
	log_info("config basic info: address=%s,port=%d,pub_addr=%s\n"
		"p2p_addr=%s,p2p_port=%d,p2p_enable=%d\n"
		"disp_addr=%s,disp_port=%d,disp_enable=%d\n"
		"timeout_idle=%d,timeout_respond=%d,interval_report=%d\n"
		"limit_flow=%ld,limit_rate=%ld\n"
		"cross_domain=%s",
		address_.c_str(),port_,pub_addr_.c_str(),
		p2p_addr_.c_str(),p2p_port_,p2p_enable_,
		disp_addr_.c_str(),disp_port_,disp_enable_,
		(int)timeout_idle_,(int)timeout_respond_,(int)interval_report_,
		(long)limit_flow_,(long)limit_rate_,
		cross_domain_file_.c_str());

#if USE_SSL
	string_t str = ssl_protocol_to_string(ssl_.protocols_);

	log_info("config ssl info: port=%d,protocols=%s,ciphers=%s\n"
		"cert=%s,ssl_key=%s,ssl_keypass=%s\n"
		"verify=%d,depth=%d,ca=%s,crl=%s\n"
		"dhparam=%s,ecdh_curve=%s",
		ssl_.port_,str.c_str(),ssl_.ciphers_.c_str(),
		ssl_.cert_.c_str(),ssl_.key_.c_str(),ssl_.keypass_.c_str(),
		ssl_.verify_,ssl_.depth_,ssl_.ca_.c_str(),ssl_.crl_.c_str(),
		ssl_.dhparam_.c_str(),ssl_.ecdh_curve_.c_str());
#endif
}
