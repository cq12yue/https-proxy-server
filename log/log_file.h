#ifndef _LOG_FILE_H
#define _LOG_FILE_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct log_s* log_t;

log_t log_open(const char *name);

void log_set_rbsize(log_t log,size_t size);

void log_set_level_range(log_t log,int lrange);

void log_close(log_t log);

ssize_t log_trace(log_t log,const char *fmt,...);

ssize_t log_info(log_t log,const char *fmt,...);

ssize_t log_debug(log_t log,const char *fmt,...);

ssize_t log_warn(log_t log,const char *fmt,...);

ssize_t log_error(log_t log,const char *fmt,...);

ssize_t log_fatal(log_t log,const char *fmt,...);

#ifdef __cplusplus
}
#endif

#endif
