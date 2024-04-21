#ifndef _NETCOMM_FUNCTION_H
#define _NETCOMM_FUNCTION_H

#include <stdint.h>
#include <unistd.h>

namespace netcomm 
{
	#define SAFE_CLOSE_PIPE(fd) do{::close(fd[0]);::close(fd[1]);fd[0]=fd[1]=-1;}while(0)
	#define SAFE_CLOSE_SOCKETPAIR(fd) SAFE_CLOSE_PIPE(fd)
	#define SAFE_CLOSE_FD(fd) do{::close(fd);fd=-1;}while(0)
	
	bool set_block_attr(int sock, bool block);
	bool get_block_attr(int sock, bool& block);
	
	bool enable_keepalive(int sock,int idle,int intv,int cnt);
	bool disable_keepalive(int sock);

	#define IN_LOOPBACK(a) ((((uint32_t)(a))&0xff000000)==0xff000000)
}

#endif
