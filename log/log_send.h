#ifndef _LOG_SEND_H
#define _LOG_SEND_H

extern bool log_init(const char* srv_fifo_path);

extern bool log_trace(const char* fmt,...);

extern bool log_info(const char* fmt,...);

extern bool log_debug(const char* fmt,...);

extern bool log_warn(const char* fmt,...);

extern bool log_error(const char* fmt,...);

extern bool log_fatal(const char* fmt,...);

#endif
