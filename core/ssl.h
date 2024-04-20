#ifndef _CORE_SSL_H
#define _CORE_SSL_H

#include "typedef.h"
#include <stdint.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
//#include <openssl/engine.h>
#include <openssl/evp.h>

extern const uint16_t SSL_SSLv2;
extern const uint16_t SSL_SSLv3;
extern const uint16_t SSL_TLSv1;
extern const uint16_t SSL_TLSv1_1;
extern const uint16_t SSL_TLSv1_2;

struct c_str_t
{
	size_t len;
	const char *data;
};

#define const_string(x) {sizeof(x)-1,x}

struct c_str_mask_t
{
	c_str_t str;
	unsigned int mask;
};

extern const c_str_mask_t SSL_PROTOCOLS[];

extern string_t ssl_protocol_to_string(int protocols);

extern int ssl_string_to_protocol(const string_t &str);

extern bool ssl_thread_setup();

extern void ssl_thread_cleanup();

extern bool ssl_init();

extern void ssl_clear_error();

extern void ssl_error();

extern RSA* ssl_rsa512_key_callback(SSL *ssl, int is_export, int key_length);

#endif
