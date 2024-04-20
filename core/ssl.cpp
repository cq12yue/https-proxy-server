#if USE_SSL
#include "ssl.h"
#include "../base/util.h"
#include "../log/log_send.h"
#include <string.h> //for strtok_r strncmp
#include <pthread.h>

const uint16_t SSL_SSLv2   = 0x0002;
const uint16_t SSL_SSLv3   = 0x0004;
const uint16_t SSL_TLSv1   = 0x0008;
const uint16_t SSL_TLSv1_1 = 0x0010;
const uint16_t SSL_TLSv1_2 = 0x0020;

const c_str_mask_t SSL_PROTOCOLS[] = 
{
	{ const_string("SSLv2"), SSL_SSLv2 },
	{ const_string("SSLv3"), SSL_SSLv3 },
	{ const_string("TLSv1"), SSL_TLSv1 },
	{ const_string("TLSv1.1"), SSL_TLSv1_1 },
	{ const_string("TLSv1.2"), SSL_TLSv1_2 }
};

string_t ssl_protocol_to_string(int protocols)
{
	string_t str;
	for(size_t i = 0; i < NUM_ELEMENTS(SSL_PROTOCOLS); ++i){
		if(protocols & SSL_PROTOCOLS[i].mask){
			if(!str.empty())
				str += ' ';
			str += SSL_PROTOCOLS[i].str.data;
		}
	}
	return str;
}

int ssl_string_to_protocol(const string_t &str)
{
	int protocols = 0;
	char old,*b,*s,*beg = (char*)&str[0],*end = beg + str.size();

	for(s=strtok_r(beg," ",&b); s; s=strtok_r(NULL," ",&b)){
		if(b!=end) {
			old = *--b, *b = '\0';
		}
		for(size_t i=0; i<NUM_ELEMENTS(SSL_PROTOCOLS); ++i){
			if(0==strcmp(s,SSL_PROTOCOLS[i].str.data)){
				protocols |= SSL_PROTOCOLS[i].mask;
				break;
			}
		}
		if(b!=end) *b++ = old;
	}
	return protocols;
}

bool ssl_init()
{
	OPENSSL_config(NULL);

	SSL_library_init();
	SSL_load_error_strings();

	OpenSSL_add_all_algorithms();
	
	return true;
}

void ssl_clear_error()
{
	if(ERR_peek_error())
		ssl_error();

	ERR_clear_error();
}

void ssl_error()
{
	unsigned long n;

	char  errstr[1024];
	char *p = errstr,*last = p + sizeof(errstr);

	strncpy(p,"(SSL:",last - p);
	p += sizeof("(SSL:")-1;

	for (;;) {
		n = ERR_get_error();
		if (n == 0) {
			break;
		}
		if (p >= last) {
			continue;
		}
		*p++ = ' ';
		ERR_error_string_n(n, p, last - p);

		while (p < last && *p) p++;
	}

	log_error("%s)", errstr);
}

RSA* ssl_rsa512_key_callback(SSL *ssl, int is_export, int key_length)
{
	static RSA  *key;

	if (key_length == 512) {
		if (key == NULL) {
			key = RSA_generate_key(512, RSA_F4, NULL, NULL);
		}
	}

	return key;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static pthread_mutex_t *s_lock = NULL;
#if DEBUG
static long *s_lock_cnt = NULL;
#endif

static void pthreads_locking_callback(int mode,int type,const char *file,int line)
{
	if (mode & CRYPTO_LOCK){
		pthread_mutex_lock(&(s_lock[type]));
#if DEBUG
		s_lock_cnt[type]++;
#endif
	}else{
		pthread_mutex_unlock(&(s_lock[type]));
	}
}

static unsigned long pthreads_thread_id()
{
	return (unsigned long)pthread_self();
}

bool ssl_thread_setup()
{
	s_lock = (pthread_mutex_t*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	if(NULL==s_lock) return false;

#if DEBUG
	s_lock_cnt = (long*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
	if(NULL==s_lock_cnt){
		OPENSSL_free(s_lock); s_lock = NULL;
		return false;
	}
#endif

	for (int i=0; i<CRYPTO_num_locks(); ++i){
#if DEBUG
		s_lock_cnt[i] = 0;
#endif
		pthread_mutex_init(&(s_lock[i]),NULL);
	}

	CRYPTO_set_id_callback(pthreads_thread_id);
	CRYPTO_set_locking_callback(pthreads_locking_callback);

	return true;
}

void ssl_thread_cleanup()
{
	if(NULL==s_lock) return;

#if DEBUG
	if(NULL==s_lock_cnt) return;
#endif
	
	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);

	for (int i=0; i<CRYPTO_num_locks(); i++){
		pthread_mutex_destroy(&(s_lock[i]));
#if DEBUG
		log_info("%8ld:%s",s_lock_cnt[i],CRYPTO_get_lock_name(i));
#endif
	}

	OPENSSL_free(s_lock);
#if DEBUG
	OPENSSL_free(s_lock_cnt);
#endif
}
#endif
