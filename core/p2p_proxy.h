#ifndef _CORE_P2P_PROXY_H
#define _CORE_P2P_PROXY_H

#include <stdint.h>

#pragma pack(1)
struct p2p_proxy_response_msg
{
	unsigned char ver;
	unsigned char cmd;
	int sock;
};

#define DEV_ID_MAX_LEN 128
struct p2p_proxy_stun_msg
{
	char  ver;
	short size;
	unsigned char type;
	char dev_id[DEV_ID_MAX_LEN+1];
	int sock;
	uint32_t pub_addr;
	uint16_t port;
#if USE_SSL
	uint16_t ssl_port;
#endif
};

//p2p_proxy_stun_msg definition
#define P2P_PROXY_STUN_MSG_SIZE sizeof(p2p_proxy_stun_msg)
#define P2P_PROXY_STUN_MSG_REQUEST 0x10

//p2p_proxy_response_msg definition
#define P2P_PROXY_VERSION     1
#define P2P_PROXY_REQ_CONNECT 1
#define P2P_PROXY_RES_CONNECT 2

#define P2P_PROXY_RES_SIZE sizeof(p2p_proxy_response_msg)
#define P2P_PROXY_DATA_TYPE   "application/p2p-proxy"

#pragma pack()

#endif
