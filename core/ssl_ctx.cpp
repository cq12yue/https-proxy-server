#if USE_SSL
#include "ssl.h"
#include "ssl_ctx.h"
#include "../log/log_send.h"
#include "../loki/ScopeGuard.h"
#include <assert.h>
#include <stdexcept>
using namespace std;

static void ssl_info_callback(const SSL *ssl, int where, int ret)
{
	/*ngx_connection_t  *c;

	if (where & SSL_CB_HANDSHAKE_START) {
		c = ngx_ssl_get_connection((ngx_ssl_conn_t *) ssl_conn);

		if (c->ssl->handshaked) {
			c->ssl->renegotiation = 1;
			ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0, "SSL renegotiation");
		}
	}*/
}

static int ssl_verify_callback(int ok, X509_STORE_CTX *x509_store)
{
#if DEBUG
	char        *subject = NULL, *issuer = NULL;
	int         err, depth;
	X509        *cert;
	X509_NAME   *sname, *iname;

	cert = X509_STORE_CTX_get_current_cert(x509_store);
	err = X509_STORE_CTX_get_error(x509_store);
	depth = X509_STORE_CTX_get_error_depth(x509_store);

	sname = X509_get_subject_name(cert);
        if(sname) subject = X509_NAME_oneline(sname,NULL,0);

	iname = X509_get_issuer_name(cert);
	if(iname) issuer = X509_NAME_oneline(iname,NULL,0);

	log_debug("verify:%d, error:%d, depth:%d,subject:\"%s\",issuer: \"%s\"",
		ok,err,depth,subject?subject:"none",issuer?issuer:"none");

	if (subject) OPENSSL_free(subject);
	if (issuer) OPENSSL_free(issuer);
#endif

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
ssl_ctx_t::ssl_ctx_t()
{
	ctx_ = SSL_CTX_new(SSLv23_method());
	if (NULL==ctx_)
		throw runtime_error("SSL_CTX_new() failed");
	
	/* client side options */
	SSL_CTX_set_options(ctx_, SSL_OP_MICROSOFT_SESS_ID_BUG);
	SSL_CTX_set_options(ctx_, SSL_OP_NETSCAPE_CHALLENGE_BUG);

	/* server side options */
	SSL_CTX_set_options(ctx_, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
	SSL_CTX_set_options(ctx_, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);

	/* this option allow a potential SSL 2.0 rollback (CAN-2005-2969) */
	SSL_CTX_set_options(ctx_, SSL_OP_MSIE_SSLV2_RSA_PADDING);

	SSL_CTX_set_options(ctx_, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
	SSL_CTX_set_options(ctx_, SSL_OP_TLS_D5_BUG);
	SSL_CTX_set_options(ctx_, SSL_OP_TLS_BLOCK_PADDING_BUG);

	SSL_CTX_set_options(ctx_, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);

	SSL_CTX_set_options(ctx_, SSL_OP_SINGLE_DH_USE);
	SSL_CTX_set_options(ctx_, SSL_OP_ALL);

#ifdef SSL_OP_NO_COMPRESSION
	SSL_CTX_set_options(ctx_, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_MODE_RELEASE_BUFFERS
	SSL_CTX_set_mode(ctx_, SSL_MODE_RELEASE_BUFFERS);
#endif

	SSL_CTX_set_read_ahead(ctx_,1);
	SSL_CTX_set_info_callback(ctx_,ssl_info_callback);

	ctx_->default_passwd_callback_userdata = NULL;
}

ssl_ctx_t::~ssl_ctx_t()
{
	if(ctx_->default_passwd_callback_userdata){
		delete static_cast<passwd_cb_base*>(ctx_->default_passwd_callback_userdata);
		ctx_->default_passwd_callback_userdata = NULL;
	}
	SSL_CTX_free(ctx_);
}

void ssl_ctx_t::set_protocol(int protocols)
{
	if (!(protocols & SSL_SSLv2)) {
		SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2);
	}
	if (!(protocols & SSL_SSLv3)) {
		SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv3);
	}
	if (!(protocols & SSL_TLSv1)) {
		SSL_CTX_set_options(ctx_, SSL_OP_NO_TLSv1);
	}
//	SSL_CTX_set_options(ctx_,SSL_OP_CIPHER_SERVER_PREFERENCE);
#ifdef SSL_OP_NO_TLSv1_1
	if (!(protocols & SSL_TLSv1_1)) {
		SSL_CTX_set_options(ctx_, SSL_OP_NO_TLSv1_1);
	}
#endif
#ifdef SSL_OP_NO_TLSv1_2
	if (!(protocols & SSL_TLSv1_2)) {
		SSL_CTX_set_options(ctx_, SSL_OP_NO_TLSv1_2);
	}
#endif
}

bool ssl_ctx_t::set_certificate(const char *certfile,const char *keyfile,const char *keypass/*=NULL*/)
{
	assert(certfile && keyfile);

	if (0==SSL_CTX_use_certificate_chain_file(ctx_,certfile)){
    	log_error("SSL_CTX_use_certificate_chain_file(\"%s\") fail",certfile);
    	return false;
    }

	if(keypass){
		BIO *key = BIO_new(BIO_s_file());
		if(NULL==key){
			log_error("ssl_ctx_t set_certificate: BIO_new fail");		
			return false;
		}
		LOKI_ON_BLOCK_EXIT(BIO_free,key);
		
		if(0==BIO_read_filename(key,keyfile)){
			log_error("ssl_ctx_t set_certificate: BIO_read_filename fail");
			return false;
		}
		EVP_PKEY *pkey=PEM_read_bio_PrivateKey(key,NULL,NULL,(char*)keypass);
		if(NULL==pkey){
			log_error("ssl_ctx_t set_certificate: PEM_read_bio_PrivateKey fail");
			return false;
		}
		LOKI_ON_BLOCK_EXIT(EVP_PKEY_free,pkey);

		if (SSL_CTX_use_PrivateKey(ctx_,pkey) <= 0){
			log_error("ssl_ctx_t SSL_CTX_use_PrivateKey fail");
			return false;
		}

	}else if(0==SSL_CTX_use_PrivateKey_file(ctx_,keyfile,SSL_FILETYPE_PEM)){
        log_error("SSL_CTX_use_PrivateKey_file(\"%s\") fail",keyfile);
    	return false;
	}

	return true;
}

bool ssl_ctx_t::set_client_certificate(const char *certfile,int depth)
{
    assert(certfile);

    STACK_OF(X509_NAME)  *list;
    /**
     * Notice,SSL_VERIFY_FAIL_IF_NO_PEER_CERT is not set,this cause the handshake to continue rather than terminate 
     * if no certificate is provided by the client
     */
    SSL_CTX_set_verify(ctx_,SSL_VERIFY_PEER,ssl_verify_callback);
    SSL_CTX_set_verify_depth(ctx_,depth);

    if(0==SSL_CTX_load_verify_locations(ctx_,certfile,NULL)){
        log_error("SSL_CTX_load_verify_locations(\"%s\") failed",certfile);
        return false;
    }

    list = SSL_load_client_CA_file(certfile);
    if (list == NULL) {
        log_error("SSL_load_client_CA_file(\"%s\") failed",certfile);
        return false;
    }

    /*
     * before 0.9.7h and 0.9.8 SSL_load_client_CA_file() always leaved an error in the error queue
     */
    ERR_clear_error();
    SSL_CTX_set_client_CA_list(ctx_,list);

    return true;
}

bool ssl_ctx_t::set_crl(const char *crl)
{
    assert(crl);

    X509_STORE *store = SSL_CTX_get_cert_store(ctx_);
    if (store == NULL) {
        log_error("SSL_CTX_get_cert_store() failed");
        return false;
    }

    X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
    if (lookup == NULL) {
        log_error("X509_STORE_add_lookup() failed");
        return false;
    }
    if (0==X509_LOOKUP_load_file(lookup,crl,X509_FILETYPE_PEM)){
        log_error("X509_LOOKUP_load_file(\"%s\") failed", crl);
        return false;
    }

    X509_STORE_set_flags(store,X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
    return true;
}

bool ssl_ctx_t::set_dhparam(const char *file)
{
    DH   *dh;
    BIO  *bio;

    /*
     * -----BEGIN DH PARAMETERS-----
     * MIGHAoGBALu8LcrYRnSQfEP89YDpz9vZWKP1aLQtSwju1OsPs1BMbAMCducQgAxc
     * y7qokiYUxb7spWWl/fHSh6K8BJvmd4Bg6RqSp1fjBI9osHb302zI8pul34HcLKcl
     * 7OZicMyaUDXYzs7vnqAnSmOrHlj6/UmI0PZdFGdX2gcd8EXP4WubAgEC
     * -----END DH PARAMETERS-----
     */

    static unsigned char dh1024_p[] = {
        0xBB, 0xBC, 0x2D, 0xCA, 0xD8, 0x46, 0x74, 0x90, 0x7C, 0x43, 0xFC, 0xF5,
        0x80, 0xE9, 0xCF, 0xDB, 0xD9, 0x58, 0xA3, 0xF5, 0x68, 0xB4, 0x2D, 0x4B,
        0x08, 0xEE, 0xD4, 0xEB, 0x0F, 0xB3, 0x50, 0x4C, 0x6C, 0x03, 0x02, 0x76,
        0xE7, 0x10, 0x80, 0x0C, 0x5C, 0xCB, 0xBA, 0xA8, 0x92, 0x26, 0x14, 0xC5,
        0xBE, 0xEC, 0xA5, 0x65, 0xA5, 0xFD, 0xF1, 0xD2, 0x87, 0xA2, 0xBC, 0x04,
        0x9B, 0xE6, 0x77, 0x80, 0x60, 0xE9, 0x1A, 0x92, 0xA7, 0x57, 0xE3, 0x04,
        0x8F, 0x68, 0xB0, 0x76, 0xF7, 0xD3, 0x6C, 0xC8, 0xF2, 0x9B, 0xA5, 0xDF,
        0x81, 0xDC, 0x2C, 0xA7, 0x25, 0xEC, 0xE6, 0x62, 0x70, 0xCC, 0x9A, 0x50,
        0x35, 0xD8, 0xCE, 0xCE, 0xEF, 0x9E, 0xA0, 0x27, 0x4A, 0x63, 0xAB, 0x1E,
        0x58, 0xFA, 0xFD, 0x49, 0x88, 0xD0, 0xF6, 0x5D, 0x14, 0x67, 0x57, 0xDA,
        0x07, 0x1D, 0xF0, 0x45, 0xCF, 0xE1, 0x6B, 0x9B
    };
    static unsigned char dh1024_g[] = { 0x02 };

    if (NULL==file){
        dh = DH_new();
        if (dh == NULL) {
            log_error("DH_new() fail");
            return false;
        }
		LOKI_ON_BLOCK_EXIT(DH_free,dh);

        dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
        dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
        if (dh->p == NULL || dh->g == NULL) {
            log_error("BN_bin2bn() failed");
            return false;
        }

        SSL_CTX_set_tmp_dh(ctx_, dh);
        return true;
    }

    bio = BIO_new_file(file, "r");
    if (bio == NULL) {
        log_error("BIO_new_file(\"%s\") failed",file);
        return false;
    }
	LOKI_ON_BLOCK_EXIT(BIO_free,bio);

    dh = PEM_read_bio_DHparams(bio,NULL,NULL,NULL);
    if (dh == NULL) {
        log_error("PEM_read_bio_DHparams(\"%s\") failed",file);
        return false;
    }

    SSL_CTX_set_tmp_dh(ctx_, dh);
    DH_free(dh);

    return true;
}

bool ssl_ctx_t::set_ecdh_curve(const char *name)
{
#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#ifndef OPENSSL_NO_ECDH
	assert(name);

    int      nid;
    EC_KEY  *ecdh;

    /*
     * Elliptic-Curve Diffie-Hellman parameters are either "named curves"
     * from RFC 4492 section 5.1.1, or explicitly described curves over
     * binary fields. OpenSSL only supports the "named curves", which provide
     * maximum interoperability.
     */
	
    nid = OBJ_sn2nid(name);
    if (nid == 0) {
        log_error("Unknown curve name \"%s\"", name);
        return false;
    }

    ecdh = EC_KEY_new_by_curve_name(nid);
    if (ecdh == NULL) {
        log_error("Unable to create curve \"%s\"", name);
        return false;
    }

    SSL_CTX_set_tmp_ecdh(ctx_, ecdh);
    SSL_CTX_set_options(ctx_, SSL_OP_SINGLE_ECDH_USE);

    EC_KEY_free(ecdh);
#endif
#endif

    return true;
}

void ssl_ctx_t::do_set_passwd_cb(passwd_cb_base *pc)
{
	if(ctx_->default_passwd_callback_userdata) 
		delete static_cast<passwd_cb_base*>(ctx_->default_passwd_callback_userdata);
	
	ctx_->default_passwd_callback_userdata = pc;

	SSL_CTX_set_default_passwd_cb(ctx_,&ssl_ctx_t::passwd_cb_fun);
}

int ssl_ctx_t::passwd_cb_fun(char *buf,int size,int purpose,void *data)
{
	//it is safe convert from derived class object pointer to base class object pointer
	passwd_cb_base *pc = static_cast<passwd_cb_base*>(data);

	string_t str = pc->call(size,purpose?false:true);

	size_t len = str.size();
	if(len > size) len = size;
	memcpy(buf,str.c_str(),len);

	return len;
}
#endif
