#include "function.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

bool netcomm::set_block_attr(int sock, bool block)
{
	int opts = fcntl(sock,F_GETFL);
	if (opts < 0)
		return false;
	opts = block ? (opts & ~O_NONBLOCK):(opts | O_NONBLOCK);
	return -1 != fcntl(sock,F_SETFL,opts);
}

bool netcomm::get_block_attr(int sock, bool& block)
{
	int opts = fcntl(sock,F_GETFL);
	if (opts < 0 )
		return false;
	block = (opts & O_NONBLOCK);
	return true;
}

bool netcomm::enable_keepalive(int sock,int idle,int intv,int cnt)    
{    
	int alive = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&alive,sizeof(alive)))   
		return false;    

	if (setsockopt(sock,SOL_TCP,TCP_KEEPIDLE,&idle,sizeof(idle)))
		return false;    

	if (setsockopt(sock,SOL_TCP,TCP_KEEPINTVL,&intv,sizeof(intv)))    
		return false;    
	
	return 0==setsockopt(sock,SOL_TCP,TCP_KEEPCNT,&cnt,sizeof(cnt));
}    

bool netcomm::disable_keepalive(int sock)
{
	int alive = 0;
	return 0==setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&alive,sizeof(alive));   
}
