#ifndef _CORE_HTTP_H
#define _CORE_HTTP_H

#include "typedef.h"

/// HTTP message type

#define HTTP_REQUEST  0
#define HTTP_RESPONSE 1

//HTTP major and minor version
#define HTTP_MAJOR_VER 1
#define HTTP_MINOR_VER 1

//unknown http request method or response code
#define HTTP_UNKNOWN		           0xFFFF

/// request method correspond to http
#define HTTP_GET                       0x0001
#define HTTP_HEAD                      0x0002
#define HTTP_POST                      0x0004
#define HTTP_PUT                       0x0008
#define HTTP_DELETE                    0x0010
#define HTTP_MKCOL                     0x0020
#define HTTP_COPY                      0x0040
#define HTTP_MOVE                      0x0080
#define HTTP_OPTIONS                   0x0100
#define HTTP_PROPFIND                  0x0200
#define HTTP_PROPPATCH                 0x0400
#define HTTP_LOCK                      0x0800
#define HTTP_UNLOCK                    0x1000
#define HTTP_PATCH                     0x2000
#define HTTP_TRACE                     0x4000
#define HTTP_CONNECT                   0x8000

//response code for http
#define HTTP_CONTINUE				   100
#define HTTP_SWITCH_PROTOCOL           101

#define HTTP_OK                        200
#define HTTP_CREATED                   201
#define HTTP_ACCEPTED                  202
#define HTTP_NO_CONTENT                204
#define HTTP_PARTIAL_CONTENT           206

#define HTTP_SPECIAL_RESPONSE          300
#define HTTP_MOVED_PERMANENTLY         301
#define HTTP_MOVED_TEMPORARILY         302
#define HTTP_SEE_OTHER                 303
#define HTTP_NOT_MODIFIED              304
#define HTTP_TEMPORARY_REDIRECT        307

#define HTTP_BAD_REQUEST               400
#define HTTP_UNAUTHORIZED              401
#define HTTP_FORBIDDEN                 403
#define HTTP_NOT_FOUND                 404
#define HTTP_NOT_ALLOWED               405
#define HTTP_REQUEST_TIME_OUT          408
#define HTTP_CONFLICT                  409
#define HTTP_LENGTH_REQUIRED           411
#define HTTP_PRECONDITION_FAILED       412
#define HTTP_REQUEST_ENTITY_TOO_LARGE  413
#define HTTP_REQUEST_URI_TOO_LARGE     414
#define HTTP_UNSUPPORTED_MEDIA_TYPE    415
#define HTTP_RANGE_NOT_SATISFIABLE     416
#define HTTP_EXPECTATIONFAILED         417

#define HTTP_INTERNAL_SERVER_ERROR     500
#define HTTP_NOT_IMPLEMENTED           501
#define HTTP_BAD_GATEWAY               502
#define HTTP_SERVICE_UNAVAILABLE       503
#define HTTP_GATEWAY_TIME_OUT          504
#define HTTP_INSUFFICIENT_STORAGE      507

//extended response code
#define HTTP_ORIGINAL_SERVER_TIMEOUT     550
#define HTTP_PROXY_SERVER_INTERNAL_ERROR 551
#define HTTP_NULL_CONNECTION		     552
#define HTTP_INVALID_CONNECTION          553
#define HTTP_MATCHED_CONNECTION          554
#define HTTP_CONFLICT_CONNECTION         555
#define HTTP_DEVICE_ID_DIFFERENT         556
#define HTTP_INVALID_MATCH				 557

#define HTTP_FAILED  -1
#define HTTP_SUCCESS 0
#define HTTP_AGAIN   1

#define HTTP_BODY_NONE       0
#define HTTP_BODY_CONTENT    1
#define HTTP_BODY_CHUNK_SIZE 2
#define HTTP_BODY_CHUNK_DATA 3

#define HTTP_INIT_HEADER_SIZE   1024
#define HTTP_MAX_CHUNK_SIZE_LEN 18
#define HTTP_CHUNK_SIZE_LEN     10
#define HTTP_EACH_BODY_SIZE     65536

//////////////////////////////////////////////////////////////////////////////////////////
extern size_t http_max_header_size;
//extern size_t http_max_body_size;

extern void http_set_max_header_size(size_t val);
//extern void set_http_max_body_size(size_t val);

/**
* @brief encode a string for inclusion in a URI
* @param 
*/
extern void http_encode_uri(const char *uri, size_t len, string_t& str,bool space_as_plus = false);
extern void http_encode_uri(const char *uri, string_t& str,bool space_as_plus = false);

/*
* @brief decode a string for inclusion in a URI.

* @param decode_plus: if 1, we decode plus into space;if 0, we don't;if -1, 
* when true we transform plus to space only after we've seen a ?.  -1 is deprecated.
*/

extern void http_decode_uri(const char *uri, size_t len, string_t& str, int decode_plus = 0);
extern void http_decode_uri(const char *uri, string_t& str, int decode_plus = 0);
extern size_t http_decode_uri(char *uri, size_t len, int decode_plus = 0);
extern size_t http_decode_uri(char *uri, int decode_plus = 0);

//////////////////////////////////////////////////////////////////////////////////////////
struct http_header
{
	int  type;
	char major;
	char minor;

	union{
		struct {
			int   verb; //method
			char* uri;  //point to uri of input buffer
			int   len;  //length of uri
		}request;

		struct {
			int   code; //status code
			char* text; //point to statue text of input buffer
			int   len;  //length of text
		}response;
	};

	str_hash_map headers;
};

class http_parser 
{
public:
	http_parser();	

	int parse_first_line(const char* data,size_t len);
	
	int parse_header_line(const char* data,size_t len);

	int parse_trailer(const char* data,size_t len) 
	{ return parse_header_line(data,len); }

	int parse_header(const char* data,size_t len)
	{
		int ret = parse_first_line(data,len);
		if(0==ret)
			ret = parse_header_line(data,len);
		return ret;
	}

	bool get_body_info(int& type,size_t& len,int& error) const;

	void reset()
	{
		off_=0; name_beg_ = NULL; state_=start;
		hdr_.headers.clear(); 
	}

private:
	/// Check if a byte is an HTTP character.
	static bool is_char(int c);

	/// Check if a byte is an HTTP control character.
	static bool is_ctl(int c);

	/// Check if a byte is defined as an HTTP tspecial character.
	static bool is_tspecial(int c);

	/// Check if a byte is a digit.
	static bool is_digit(int c);
	
	enum {
		start,
		check,
		space_before_uri,
		uri,
		http_version_h,
		http_version_t_1,
		http_version_t_2,
		http_version_p,
		http_version_slash,
		http_version_major,
		http_version_minor,
		status_code,
		status_text,
		header_lws,
		header_name,
		space_before_header_value,
		header_value,
		almost_done,
		header_almost_done
	} state_;

	char *line_beg_,*line_end_;	
	char *name_beg_,*name_end_; //for temp be used to record position

public:
	size_t off_;
	http_header hdr_;
	size_t hdr_len_;
};

class buffer;
class connection;

class http_stream 
{
public:
	explicit http_stream(connection* conn);
	~http_stream();

	enum sendMode {
		nosend=0x0000,
		send_close=0x0002,
		send_keepalive=0x0003
	};

	void send_response(int status,str_hash_map* headers,const void* body,size_t len,int flag=send_keepalive);
	void send_error(int status);
	void send_ok(bool is_html,int flag=send_keepalive);
	void send_crossdomain();
	void send_system_info();
	void read_request();
	void read_response();

	bool is_data_break() const 
	{ return reading_header!=state_; }

protected:
	void onHeader();
	void onHeaderError(int error);
	void onBody();
	void onBodyError(int error);
	void onTrailerError(int error);
	void onReadError(bool closed,const char* desc);

private:
	bool handle_special_request();
	int read_message(const char*& desc);
	void read_message_no_parse();
	int read_header();
	int read_body();
	int read_trailer();

private:
	http_parser parser_;

	enum readState {
		reading_header,
		reading_body,
		reading_trailer
	} state_;
	
	enum dataState{
		data_finish=1,
		data_error,
		data_unreadable,
		conn_closed,
		conn_error
	};

	int body_type_;
	//if body_type is content,then it specifies total length of body.else total length of each chunked
	size_t body_len_; 
	size_t body_tran_;	

	connection* conn_;
	buffer* buf_;	
};

#endif
