#ifndef _NETCOMM_NET_ADDR_H
#define _NETCOMM_NET_ADDR_H

#include <netinet/in.h>

namespace netcomm
{
class net_addr
{
public:
	net_addr();
	net_addr(const sockaddr* addr,size_t len);

	int family() const
	{ return addr_.in4_.sin_family; }

	int length() const
	{ return AF_INET==family()? sizeof(sockaddr_in) : sizeof(sockaddr_in6); }

	void* ip_addr();
	in_addr* ip4_addr();
	in6_addr* ip6_addr();
	const void* ip_addr() const;
	const in_addr* ip4_addr() const;
	const in6_addr* ip6_addr() const;

	unsigned short* ip_port();
	unsigned short* ip4_port();
	unsigned short* ip6_port();
	const unsigned short* ip_port() const;
	const unsigned short* ip4_port() const;
	const unsigned short* ip6_port() const;

	bool is_any() const;
	bool is_loopback() const;
	bool is_multicast() const;
	bool is_all_broadcast() const;

	void set(const sockaddr* addr,size_t len);
	
	operator sockaddr_in* ()
	{ return reinterpret_cast<sockaddr_in*>(&addr_); }

	operator sockaddr* ()
	{ return reinterpret_cast<sockaddr*>(&addr_); }

	operator const sockaddr_in* () const
	{ return reinterpret_cast<const sockaddr_in*>(&addr_); }

	operator const sockaddr* () const
	{ return reinterpret_cast<const sockaddr*>(&addr_); }

private:
	union {
		sockaddr_in  in4_;
		sockaddr_in6 in6_;
	}addr_;
};
}

#endif
