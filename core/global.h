#ifndef _CORE_GLOBAL_H
#define _CORE_GLOBAL_H

#include "typedef.h"
#include "config.h"

extern string_t g_conf_filename;

extern config g_conf;

extern string_t g_cross_domain_data;

#if USE_SSL
class ssl_ctx_t;
extern bool init_ssl_ctx(const config::ssl_info &ssl,ssl_ctx_t *ctx);
#endif

#endif
