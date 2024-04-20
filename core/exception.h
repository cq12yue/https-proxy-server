#ifndef _CORE_PROXY_EXCEPTION_H
#define _CORE_PROXY_EXCEPTION_H

#include <stdint.h>

struct http_exception
{

explicit http_exception(int code)
:code_(code)
{	}	

int code_;
};

static const uint8_t SELF_RD_ERROR  = 0x01;
static const uint8_t SELF_WR_ERROR  = 0x02;
static const uint8_t OTHER_RD_ERROR = 0x04;
static const uint8_t OTHER_WR_ERROR = 0x08;
static const uint8_t IDLE_TIMEOUT   = 0x10;

struct io_exception
{
explicit io_exception(uint8_t flag)
:flag_(flag)
{	}

uint8_t flag_;
};

#endif
