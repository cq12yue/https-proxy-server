#include "global.h"
#if USE_SSL
#include "ssl.h"
#include "ssl_ctx.h"
#include "../log/log_send.h"
#endif
#include <assert.h>

string_t g_conf_filename;

config g_conf;

string_t g_cross_domain_data;

//////////////////////////////////////////////////////////////////////////
#if USE_SSL
static string_t get_privatekey_passwd(size_t size,bool is_read)
{
	return g_conf.ssl_.keypass_;
}

bool init_ssl_ctx(const config::ssl_info &ssl,ssl_ctx_t *ctx)
{
	assert(ctx);
	if(!ssl.enable_)
		 return true;

	ctx->set_passwd_cb(get_privatekey_passwd);
	ctx->set_protocol(ssl.protocols_);

	if(!ctx->set_certificate(ssl.cert_.c_str(),ssl.key_.c_str())){
		return false;
	}
	if(!ssl.ciphers_.empty())
		if(!SSL_CTX_set_cipher_list(*ctx,ssl.ciphers_.c_str()))
			log_error("SSL_CTX_cipher_list error %s",ssl.ciphers_.c_str());
		else
			log_info("SSL_CTX_cipher_list ok %s",ssl.ciphers_.c_str());

	if(ssl.verify_){
		if(!ctx->set_client_certificate(ssl.ca_.c_str(),ssl.depth_)){
			log_error("ssl_client_certificate ca=%s depth=%d fail",ssl.ca_.c_str(),ssl.depth_);		
			return false;
		}
		if(!ssl.crl_.empty()&&!ctx->set_crl(ssl.crl_.c_str())){
			log_error("ssl_crl %s fail",ssl.crl_.c_str());	
			return false;
		}
	}

	SSL_CTX_set_tmp_rsa_callback(*ctx,ssl_rsa512_key_callback);

	const char *dhparam = NULL;
	if(!ssl.dhparam_.empty())
		dhparam = ssl.dhparam_.c_str();
	if (!ctx->set_dhparam(dhparam)) {
		log_error("ssl_dhparam %s fail",dhparam?dhparam:"null");	
		return false;
	}

	if (!ssl.ecdh_curve_.empty()&&!ctx->set_ecdh_curve(ssl.ecdh_curve_.c_str())) {
		log_error("ssl_ecdh_curve %s fail",ssl.ecdh_curve_.c_str());	
		return false;
	}

	return true;
}
#endif
