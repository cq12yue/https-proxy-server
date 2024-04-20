#ifndef _CORE_CONFIG_H
#define _CORE_CONFIG_H

#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../mem/new_policy.h"
#include "../base/atomic.h"
#include "typedef.h"

namespace base
{
	class ini_file;
}

struct config;
typedef void (*changed_handle_fn)(config*,uint32_t,void*);

struct config : public base::if_then_else<_USE_MEM_POOL,
								memory::tls_spec_new_policy<config>,
								base::null_type
								>::type
{
	static const uint32_t MASK_BIT_LOCAL;
	static const uint32_t MASK_BIT_P2P;
	static const uint32_t MASK_BIT_DISPATCH;
	static const uint32_t MASK_BIT_PUB_ADDR;
	static const uint32_t MASK_BIT_TIMEOUT_IDLE;
	static const uint32_t MASK_BIT_TIMEOUT_RESPOND;
	static const uint32_t MASK_BIT_INTERVAL_REPORT;
	static const uint32_t MASK_BIT_LIMIT_FLOW;
	static const uint32_t MASK_BIT_LIMIT_RATE;
	static const uint32_t MASK_BIT_CROSS_DOMAIN;
	static const uint32_t MASK_BIT_ALL;

public:
	config();

	bool set(const base::ini_file &ini,uint32_t mask,bool is_notify_changed);
	
	bool set_all(const base::ini_file &ini,bool is_notify_changed)
	{ return set(ini,MASK_BIT_ALL,is_notify_changed); }

	void set_changed_handler(changed_handle_fn handler,void *data)
	{
		changed_handler_ = handler;
		data_ = data;
	}

	void print();

	//the follow variable are accessed in same thread
	string_t address_;
	// value 0 indicates that 'port' key is not exist
	uint16_t port_;
	
	string_t p2p_addr_;
	int p2p_port_;
	int p2p_enable_;
	
	string_t disp_addr_;
	int disp_port_;
	int disp_enable_;
	
	//the follow variable are accessed in different thread
	string_t pub_addr_;
	base::atomic_uint32 pub_ip_; 
	
	base::atomic_int timeout_idle_;
	base::atomic_int timeout_respond_;
	base::atomic_int interval_report_;

    //Available total flow, unit is GB, negative or zero value indicates it is unlimited,otherwise limit.
	base::atomic_long limit_flow_; 

    //Available total rate, unit is KB, negative or zero value indicates it is unlimited,otherwise limit.
	base::atomic_long limit_rate_;

	string_t cross_domain_file_;

#if USE_SSL
	static const uint32_t MASK_BIT_SSL_INFO;

	struct ssl_info
	{
		int enable_;
		// value -1 indicates that 'ssl_port' key is not exist
		uint16_t port_; 
		int protocols_;
		int verify_;
		int depth_;
		string_t ciphers_;		
		string_t cert_;
		string_t key_;		
		string_t keypass_;
		string_t ca_;		
		string_t dhparam_;
		string_t crl_;
		string_t ecdh_curve_;
	};

	ssl_info ssl_;
#endif

private:
	changed_handle_fn changed_handler_;
	void *data_;
};

#endif
