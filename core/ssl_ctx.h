#ifndef _CORE_SSL_CTX_H
#define _CORE_SSL_CTX_H

#include "connection.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include "../mem/new_policy.h"
#include "../base/noncopyable.h"
#include <openssl/ssl.h>

class ssl_ctx_t : public base::if_then_else<_USE_MEM_POOL,
					memory::tls_spec_new_policy<ssl_ctx_t>,
					base::null_type
					>::type
 		, base::noncopyable 
{
public:
	ssl_ctx_t();
	~ssl_ctx_t();

	void set_protocol(int protocols);
	bool set_certificate(const char *certfile,const char *keyfile,const char *keypass=NULL);
	bool set_client_certificate(const char *certfile,int depth);

	bool set_crl(const char *crl);
	bool set_dhparam(const char *file);
	bool set_ecdh_curve(const char *name);

	//support function callback and also functor
	template<typename T>
	void set_passwd_cb(T cb)
	{ do_set_passwd_cb(new passwd_cb<T>(cb)); }

	operator SSL_CTX*()
	{ return ctx_; }

	operator const SSL_CTX*() const
	{ return ctx_; }

private:
	class passwd_cb_base
	{
	public:
		virtual string_t call(size_t size,bool is_read) = 0;

		virtual ~passwd_cb_base()
		{ }
	};

	template<typename T>
	class passwd_cb : public passwd_cb_base
	{
	public:
		explicit passwd_cb(T callback)
			:callback_(callback)
		{}
		
		string_t call(size_t size,bool is_read)
		{ return callback_(size,is_read); }

	private:
		T callback_;
	};

	void do_set_passwd_cb(passwd_cb_base *pc);
	static int passwd_cb_fun(char *buf,int size,int purpose,void *data);

private:
	SSL_CTX *ctx_;
};

#endif
