#ifndef _LOG_TYPE_H
#define _LOG_TYPE_H

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct
{
	const char *str;
    int level;
}log_level_text_t;

#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

extern const log_level_text_t log_level_text[];

#ifdef __cplusplus
}
#endif

#endif
