#include "net_addr.h"
#include "function.h"
#include <string.h> //for memcpy
#include <arpa/inet.h> //for ntohl
#include <cassert>
using namespace netcomm;

/**************************************************************************************************
2013-11-18 improve the implement of is_multicast and is_loopback methods when family is AF_INET
	   ,first convert network byte order to host byte order	
**************************************************************************************************/

net_addr::net_addr()
{
}

net_addr::net_addr(const sockaddr* addr,size_t len)
{
	set(addr,len);
}

void* net_addr::ip_addr()
{
	if(AF_INET==family())
		return ip4_addr();
	return ip6_addr();
}

in_addr* net_addr::ip4_addr()
{
	return &addr_.in4_.sin_addr;
}

in6_addr* net_addr::ip6_addr()
{
	return &addr_.in6_.sin6_addr;
}

unsigned short* net_addr::ip_port()
{
	if(AF_INET==family())
		return ip4_port();
	return ip6_port();
}

unsigned short* net_addr::ip4_port()
{
	assert(AF_INET==family());
	return &addr_.in4_.sin_port;
}

unsigned short* net_addr::ip6_port()
{
	assert(AF_INET6==family());
	return &addr_.in6_.sin6_port;
}

//////////////////////////////////////////////////////////////////////////////////////////
const void* net_addr::ip_addr() const
{
	if(AF_INET==family())
		return ip4_addr();
	return ip6_addr();
}

const in_addr* net_addr::ip4_addr() const
{
	return &addr_.in4_.sin_addr;
}

const in6_addr* net_addr::ip6_addr() const
{
	return &addr_.in6_.sin6_addr;
}

const unsigned short* net_addr::ip_port() const
{
	if(AF_INET==family())
		return ip4_port();
	return ip6_port();
}

const unsigned short* net_addr::ip4_port() const
{
	assert(AF_INET==family());
	return &addr_.in4_.sin_port;
}

const unsigned short* net_addr::ip6_port() const
{
	assert(AF_INET6==family());
	return &addr_.in6_.sin6_port;
}

void net_addr::set(const sockaddr* addr,size_t len)
{
	size_t maxlen = 0;
	
	if(AF_INET==addr->sa_family)
		maxlen = sizeof(sockaddr_in);
	else if(AF_INET6==addr->sa_family)
		maxlen = sizeof(sockaddr_in6);
//else
//	assert(false);

	if(len > maxlen) 
		len = maxlen;
	memcpy(&addr_,addr,len);	
}

bool net_addr::is_any() const
{
	if (AF_INET6==family())
		return IN6_IS_ADDR_UNSPECIFIED(&addr_.in6_.sin6_addr);
	return INADDR_ANY==addr_.in4_.sin_addr.s_addr;
}

// Return @c true if the IP address is IPv4/IPv6 loopback address.
bool net_addr::is_loopback() const
{
	if (AF_INET6==family())
		return IN6_IS_ADDR_LOOPBACK(&addr_.in6_.sin6_addr);
	return IN_LOOPBACK(ntohl(addr_.in4_.sin_addr.s_addr));
}

// Return @c true if the IP address is IPv4/IPv6 multicast address.
bool net_addr::is_multicast() const
{
	if (AF_INET6==family())
		return IN6_IS_ADDR_MULTICAST(&addr_.in6_.sin6_addr);
	return IN_MULTICAST(ntohl(addr_.in4_.sin_addr.s_addr));
}

bool net_addr::is_all_broadcast() const
{
	if(AF_INET==family())
		return INADDR_BROADCAST==addr_.in4_.sin_addr.s_addr;
	return false;
}
