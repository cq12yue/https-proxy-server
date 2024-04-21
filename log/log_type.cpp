#include "log_type.h"
#include <assert.h>

const int LOG_LEVEL_TRACE = 0;
const int LOG_LEVEL_DEBUG = 1;
const int LOG_LEVEL_INFO  = 2;
const int LOG_LEVEL_WARN  = 3;
const int LOG_LEVEL_ERROR = 4;
const int LOG_LEVEL_FATAL = 5;

const log_level_text_t log_level_text[] = 
{
	{"TRACE",LOG_LEVEL_TRACE},
	{"DEBUG",LOG_LEVEL_DEBUG},
	{"INFO",LOG_LEVEL_INFO},
	{"WARN",LOG_LEVEL_WARN},
	{"ERROR",LOG_LEVEL_ERROR},
	{"FATAL",LOG_LEVEL_FATAL}
};
