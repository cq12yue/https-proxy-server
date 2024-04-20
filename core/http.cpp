#include "http.h"
#include "connection.h"
#include "exception.h"
#include "buffer.h"
#include "global.h"
#include "constant.h"
#include "../macros.h"
#include "../base/cstring.h"
#include "../base/util.h"
#include "../base/proc_stat.h"
#include "../log/log_send.h"
#include <stdio.h>  //for sprintf
#include <string.h> //for memcpy
#include <cassert>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace memory;
using namespace std;

/*****************************************************************************************************
2013-1-10  1) change http_session into http_stream,add send_error and read_message methods,improve send
			  _response method,read_message implement http header,body that include chunked and trailer 
			  data receiving  
		   2) improve class http_parser's implementation: split parse_header into parse_first_line and 
		      parse_header_line,add parse_trailer

2013-4-15  add send_system_info to easily know server's run status which include connection and runtime
2013-4-16  improve read_message function to read all data until tcp buffer is empty
2013-5-30  add cpu and memory usage in send_system_info function
*****************************************************************************************************/

static const char* http_normal_status_lines[] = 
{
	"100 Continue",
    "101 Switch Protocol",

#define HTTP_LAST_1XX 102
#define HTTP_OFF_2XX (HTTP_LAST_1XX - 100)

	"200 OK",
	"201 Created",
	"202 Accepted",
	"",  /* "203 Non-Authoritative Information" */
	"204 No Content",
	"",  /* "205 Reset Content" */
	"206 Partial Content",

	/* "", */  /* "207 Multi-Status" */

#define HTTP_LAST_2XX  207
#define HTTP_OFF_3XX  (HTTP_LAST_2XX - 200 + HTTP_OFF_2XX)

	/* "", */  /* "300 Multiple Choices" */

	"301 Moved Permanently",
	"302 Moved Temporarily",
	"303 See Other",
	"304 Not Modified",
	"",  /* "305 Use Proxy" */
	"",  /* "306 unused" */
	"307 Temporary Redirect",

#define HTTP_LAST_3XX  308
#define HTTP_OFF_4XX   (HTTP_LAST_3XX - 301 + HTTP_OFF_3XX)

	"400 Bad Request",
	"401 Unauthorized",
	"402 Payment Required",
	"403 Forbidden",
	"404 Not Found",
	"405 Not Allowed",
	"406 Not Acceptable",
	"",  /* "407 Proxy Authentication Required" */
	"408 Request Time-out",
	"409 Conflict",
	"410 Gone",
	"411 Length Required",
	"412 Precondition Failed",
	"413 Request Entity Too Large",
	"",  /* "414 Request-URI Too Large", but we never send it
		 * because we treat such requests as the HTTP/0.9
		 * requests and send only a body without a header
		 */
	 "415 Unsupported Media Type",
	 "416 Requested Range Not Satisfiable",

	 "417 Expectation Failed",
	 /* "", */  /* "418 unused" */
	 /* "", */  /* "419 unused" */
	 /* "", */  /* "420 unused" */
	 /* "", */  /* "421 unused" */
	 /* "", */  /* "422 Unprocessable Entity" */
	 /* "", */  /* "423 Locked" */
	 /* "", */  /* "424 Failed Dependency" */
#define HTTP_LAST_4XX  418
#define HTTP_OFF_5XX   (HTTP_LAST_4XX - 400 + HTTP_OFF_4XX)

	 "500 Internal Server Error",
	 "501 Method Not Implemented",
	 "502 Bad Gateway",
	 "503 Service Temporarily Unavailable",
	 "504 Gateway Time-out",

	 "",        /* "505 HTTP Version Not Supported" */
	 "",        /* "506 Variant Also Negotiates" */
	 "507 Insufficient Storage"
	 /* "", */  /* "508 unused" */
	 /* "", */  /* "509 unused" */
	 /* "", */  /* "510 Not Extended" */

#define HTTP_LAST_5XX  508
};

static const char* http_extended_status_line[] = 
{
#define HTTP_OFF_EXTENDED 0
	"550 Other Connection Timeout",
	"551 Proxy Server Internal Error",
	"552 Null Connection",
	"553 Invalid Connection",
	"554 Matched Connection",
	"555 Conflict Connection",
	"556 Device ID difference",
	"557 Invalid match"	
#define HTTP_LAST_EXTENDED 558
};

static const char* http_get_status_line(int status) 
{
	const char* line;
	if(status >= HTTP_CONTINUE && status < HTTP_LAST_1XX){
		status -= HTTP_CONTINUE;
		line = http_normal_status_lines[status];
	}else if (status >= HTTP_OK && status < HTTP_LAST_2XX){
		/* 2XX */
		status = status - HTTP_OK + HTTP_OFF_2XX;
		line = http_normal_status_lines[status];
	}else if (status >= HTTP_MOVED_PERMANENTLY	&& status < HTTP_LAST_3XX){
		/* 3XX */
		status = status - HTTP_MOVED_PERMANENTLY + HTTP_OFF_3XX;
		line = http_normal_status_lines[status];
	}else if (status >= HTTP_BAD_REQUEST && status < HTTP_LAST_4XX){
		/* 4XX */
		status = status - HTTP_BAD_REQUEST + HTTP_OFF_4XX;
		line = http_normal_status_lines[status];
	}else if (status >= HTTP_INTERNAL_SERVER_ERROR && status < HTTP_LAST_5XX){
		/* 5XX */
		status = status - HTTP_INTERNAL_SERVER_ERROR + HTTP_OFF_5XX;
		line = http_normal_status_lines[status];
	}else if(status >= HTTP_ORIGINAL_SERVER_TIMEOUT && status < HTTP_LAST_EXTENDED){
		status = status - HTTP_ORIGINAL_SERVER_TIMEOUT + HTTP_OFF_EXTENDED;
		line = http_extended_status_line[status];
	}else
		line = NULL;
	return line;
}

//////////////////////////////////////////////////////////////////////////////////////////
size_t http_max_header_size = 8192; //default max header size is 8192 bytes
//size_t http_max_body_size   = 1024; //default max body size is 1024 bytes

void http_set_max_header_size(size_t val)
{
    http_max_header_size = val;
}

//void set_http_max_body_size(size_t val)
//{
//	http_max_body_size = val;
//}

static const char http_uri_table[256] = 
{
	/* 0 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 0, 0, 0, 0, 0, 0,
	/* 64 */
	0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,
	0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};

#define HTTP_CHAR_IS_UNRESERVED(c) (http_uri_table[(unsigned char)(c)])

template<class T>
void http_encode_uri_impl(const char *uri, size_t len, T& str,bool space_as_plus/*=false*/)
{
	char c;

	for (size_t i = 0; i < len; i++) {
		c = uri[i];
		if (HTTP_CHAR_IS_UNRESERVED(c)) {
			str.push_back(c);
		}else if(c == ' ' && space_as_plus) {
			str.push_back('+');
		}else{
			char buf[4];
			sprintf(buf,"%%%02X",(unsigned char)c);
			str.append(buf);
		}
	}
}

void http_encode_uri(const char *uri, size_t len, string_t& str,bool space_as_plus/*=false*/)
{
	http_encode_uri_impl(uri,len,str,space_as_plus);
}

void http_encode_uri(const char *uri, string_t& str,bool space_as_plus/*=false*/)
{
	http_encode_uri_impl(uri,strlen(uri),str,space_as_plus);
}

//////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void http_decode_uri_impl(const char *uri, size_t len, T& str, int decode_plus/*=0*/)
{
	char c,tmp[3];

	for (size_t i = 0; i < len; i++) {
		c = uri[i];
		if (c == '?') {
			if(decode_plus < 0)
				decode_plus = 1;
		} else if (c == '+' && decode_plus) {
			c = ' ';
		} else if (c == '%' && isxdigit(uri[i+1]) && isxdigit(uri[i+2])) {
			tmp[0] = uri[i+1],tmp[1] = uri[i+2],tmp[2] = '\0';
			c = (char)strtol(tmp, NULL, 16);
			i += 2;
		}
		str.push_back(c);
	}
}

void http_decode_uri(const char *uri, size_t len, string_t& str, int decode_plus /*= 0*/)
{
	http_decode_uri_impl(uri,len,str,decode_plus);
}

void http_decode_uri(const char *uri, string_t& str, int decode_plus/*=0*/)
{
	http_decode_uri_impl(uri,strlen(uri),str,decode_plus);
}

//in place decode function
size_t http_decode_uri(char *uri, size_t len, int decode_plus/*=0*/)
{
	char c,tmp[3];

	for (size_t i = 0; i < len; i++) {
		c = uri[i];
		if (c == '?') {
			if(decode_plus < 0)
				decode_plus = 1;
		} else if (c == '+' && decode_plus) {
			c = ' ';
		} else if (c == '%' && isxdigit(uri[i+1]) && isxdigit(uri[i+2])) {
			tmp[0] = uri[i+1],tmp[1] = uri[i+2],tmp[2] = '\0';
			c = (char)strtol(tmp, NULL, 16);
			memmove(&uri[i+1],&uri[i+3],len-i-3);
			len -= 2;
		}
		uri[i] = c;
	}
	return len;
}

size_t http_decode_uri(char *uri,int decode_plus/*=0*/)
{
	return http_decode_uri(uri,strlen(uri),decode_plus);
}

static void print_header(const http_header* hdr)
{
	log_info("--------http header info--------");
	log_info("type=%s,version=%d.%d",hdr->type==HTTP_REQUEST?"request":"response",hdr->major,hdr->minor);

	string_t str;
	if(HTTP_REQUEST==hdr->type){
		const char* method;
		switch(hdr->request.verb){
			case HTTP_GET: method="get"; break;
			case HTTP_HEAD: method="head"; break;
			case HTTP_PUT: method="put"; break;
			case HTTP_POST: method="post"; break;
			case HTTP_PATCH: method="patch"; break;
			case HTTP_OPTIONS: method="options"; break;
			case HTTP_DELETE: method="delete"; break;
		}
		log_info("request method=%s",method);
		str.assign(hdr->request.uri,hdr->request.len);
		log_info("request uri=%s",str.c_str());
	}else{
		log_info("response code=%d",hdr->response.code);
		str.assign(hdr->response.text,hdr->response.len);
		log_info("response text=%s",str.c_str());
	}
	
	for(str_hash_map::const_iterator it=hdr->headers.begin();it!=hdr->headers.end();++it)
		log_info("name=%s, value=%s",it->first.c_str(),it->second.c_str());	
}

static const int READABLE_MAX_LEN = 65535;
static const int READABLE_MIN_LEN = 512;
static const int SSL_READ_BUFFER_LEN = 1024;



//////////////////////////////////////////////////////////////////////////////////////////
http_parser::http_parser()
:state_(start)
,off_(0)
,name_beg_(NULL)
{
}

/**
 * -1--parse fail,0--parse success,1--need more data
 */
int http_parser::parse_first_line(const char* data,size_t len)
{
	char *beg = const_cast<char*>(data) + off_,*end = const_cast<char*>(data) + len,*p;
	char c;
	
	for (p=beg; p<end; ++p){
		c = *p; ++off_;
		
		switch(state_){
		case start:
			if (!is_char(c) || is_ctl(c) || is_tspecial(c)){
				log_fatal("http parse_first_line start: %d",c);
				return -1;
			 }
			state_    = check;
			hdr_.type = HTTP_REQUEST;
			line_beg_ = p;
			break;

		case check:
			if(c==' ') return -1;
			switch(p-line_beg_){
				case 2: 
					if(0==strncmp(line_beg_,"GET",3)){
						hdr_.request.verb = HTTP_GET;
						state_ = space_before_uri;
					}else if(0==strncmp(line_beg_,"PUT",3)){
						hdr_.request.verb = HTTP_PUT;
						state_ = space_before_uri;
					}
					break;

				case 3:
					if(0==strncmp(line_beg_,"HEAD",4)){
						hdr_.request.verb = HTTP_HEAD;
						state_ = space_before_uri;
					}else if(0==strncmp(line_beg_,"POST",4)){
						hdr_.request.verb = HTTP_POST;
						state_ = space_before_uri;
					}else if(0==strncmp(line_beg_,"HTTP",4)){
						hdr_.type = HTTP_RESPONSE;
						state_ = http_version_slash;
					}
					break;

				case 4:
					if(0==strncmp(line_beg_,"TRACE",5)){
						hdr_.request.verb = HTTP_TRACE;
						state_ = space_before_uri;
					}else if(0==strncmp(line_beg_,"PATCH",5)){
						hdr_.request.verb = HTTP_PATCH;
						state_ = space_before_uri;
					}
					break;

				case 5:
					if(0==strncmp(line_beg_,"DELETE",6)){
						hdr_.request.verb = HTTP_DELETE;
						state_ = space_before_uri;
					}
					break;

				case 6:
					if(0==strncmp(line_beg_,"OPTIONS",7)){
						hdr_.request.verb = HTTP_OPTIONS;
						state_ = space_before_uri;
					}else if(0==strncmp(line_beg_,"CONNECT",7)){
						hdr_.request.verb = HTTP_CONNECT;
						state_ = space_before_uri;
					}
					break;
			}
			break;

		case space_before_uri:
			if(' '!=c){
				log_fatal("http parse_first_line space_before_uri: %c",c);
				return -1;
			}
			state_ = uri;
			hdr_.request.uri = p + 1;
			break;

		case uri:
			if (is_ctl(c)){
				log_fatal("http parse_first_line uri: %d",c);
				return -1;
			}
			if (' '==c){
				hdr_.request.len = p - hdr_.request.uri;
				state_ = http_version_h;
			}
			break;

		case http_version_h:
			if ('H'!=c){
				log_fatal("http parse_first_line version_h: %c",c);
				return -1;
			}
			state_ = http_version_t_1;
			break;

		case http_version_t_1:
			if ('T'!=c){
				log_fatal("http parse_first_line version_t_1: %c",c);
				return -1;
			}
			state_ = http_version_t_2;
			break;

		case http_version_t_2:
			if ('T'!=c){
				log_fatal("http parse_first_line version_t_2: %c",c);
				return -1;
			}
			state_ = http_version_p;
			break;

		case http_version_p:
			if ('P'!=c){
				log_fatal("http parse_first_line version_p: %c",c);
				return -1;
			}
			state_ = http_version_slash;
			break;

		case http_version_slash:
			if ('/' != c){
				log_fatal("http parse_first_line version_slash: %c",c);
				return -1;
			}
			hdr_.major = 0;
			state_ = http_version_major;
			break;

		case http_version_major:
			if(is_digit(c))
				hdr_.major = hdr_.major * 10 + c - '0';
			else if ('.'==c){
				hdr_.minor = 0;
				state_ = http_version_minor;
			}else{
				log_fatal("http parse_first_line version_major: %c",c);
				return -1;
			}
			break;

		case http_version_minor:
			if (is_digit(c))
				hdr_.minor = hdr_.minor * 10 + c - '0';
			else if ('\r'==c)
				state_ = almost_done;
			else if(' '==c){
				hdr_.response.code = 0;
				state_ = status_code;
			}else{
			 	log_fatal("http parse_first_line version_minor: %c",c);
			    return -1;
			}
			break;

		case status_code:
			if(is_digit(c))
				hdr_.response.code = hdr_.response.code*10 + c - '0';
			else if(' '==c){
				state_ = status_text; 
				hdr_.response.text = p + 1;
			}else{
			 	log_fatal("http parse_first_line status_code: %c",c);
				return -1;	
			}
			break;

		case status_text:
			if ('\r'==c){
				hdr_.response.len = p - hdr_.response.text;
				state_ = almost_done;
			}
			else if (is_ctl(c)){
			 	log_fatal("http parse_first_line status_text: %d",c);
				return -1;
			}
			break;

		case almost_done:
			if('\n'!=c){
				log_fatal("http parse_first_line almost_done: %c",c);
				return -1;
			}
			state_ = start;
			line_end_ = p + 1;
			return 0;
		}
	}
	return 1;
}

/**
 * -1--parse fail,0--parse success,1--need more data
 */
int http_parser::parse_header_line(const char* data,size_t len)
{
	char *beg = const_cast<char*>(data)+off_,*end = const_cast<char*>(data)+len,*p;
	char c;
	string_t value;

	for(p=beg; p<end; ++p){
		c = *p; ++off_;

		switch(state_){
			case start:
				if (c == '\r')
					state_ = header_almost_done;
				else if (name_beg_ && (c==' '||c=='\t')){
					state_ = header_lws;
				}else if (!is_char(c) || is_ctl(c) || is_tspecial(c)){
					return -1;
				}else{
				//	name.push_back(c);
					if(name_beg_){
						hdr_.headers.insert(make_pair(string_t(name_beg_,name_end_),value));
						value.clear();
					}
					name_beg_ = p;
					state_ = header_name;
				}
				break;

			case header_lws:
				if (c == '\r')
					state_ = almost_done;
				else if(c==' '||c=='\t'){

				}else if (is_ctl(c))
					return -1;
				else{
					state_ = header_value;
					value.push_back(c);
				}
				break;

			case header_name:
				if (c==':'){
					state_ = space_before_header_value;
					name_end_ = p;
				}else if (!is_char(c) || is_ctl(c) || is_tspecial(c)){
					return -1;
				}else{				
				//	name.push_back(c);
				}
				break;

			case space_before_header_value:
				if (c==' ')
					state_ = header_value;
				else
					return -1;
				break;

			case header_value:
				if ('\r' == c)
					state_ = almost_done;
				else if (is_ctl(c) && (c != ' ' || c != '\t')){
					return -1;
				}else
					value.push_back(c);
				break;

			case almost_done:
				if (c=='\n'){
					state_ = start;
				}else
					return -1;
				break;

			case header_almost_done:
				if(c=='\n'){
					if(name_beg_){
						hdr_.headers.insert(make_pair(string_t(name_beg_,name_end_),value));
					}
					hdr_len_ = off_;
					return 0;
				}
				return -1;
			}
	}
	return 1;
}

bool http_parser::get_body_info(int& type,size_t& len,int& error) const
{
	const str_hash_map& headers = hdr_.headers;

	str_hash_map::const_iterator it = headers.find("Transfer-Encoding");

	if(it != headers.end() && !strcasecmp(it->second.c_str(),"chunked"))
		type = HTTP_BODY_CHUNK_SIZE;
	else if((it = headers.find("Content-Length")) != headers.end()){
		if(base::strtoul(it->second.c_str(),(unsigned long&)len))
			type = HTTP_BODY_CONTENT;
		else{
			error = HTTP_BAD_REQUEST;		
			return false;
		}
	}else
		type = HTTP_BODY_NONE;

	if(HTTP_REQUEST==hdr_.type){
		switch (hdr_.request.verb){
			case HTTP_POST:
			case HTTP_PUT:
			case HTTP_PATCH:
				if(HTTP_BODY_NONE==type){
					error = HTTP_BAD_REQUEST;
					return false;
				}
				break;
		}
		if(hdr_.major>1||(hdr_.major==1&&hdr_.minor==1)){
			it = headers.find("Except");
			if(it != headers.end()){
				if(!strcasecmp(it->second.c_str(),"100-Continue"))
					error = HTTP_CONTINUE;
				else{
					error = HTTP_EXPECTATIONFAILED;
					return false;
				}
			}
		}
	}	
	return true;
}

bool http_parser::is_char(int c)
{
	return c >= 0 && c <= 127;
}

bool http_parser::is_ctl(int c)
{
	return (c >= 0 && c <= 31) || (c == 127);
}

bool http_parser::is_tspecial(int c)
{
	switch (c){
	case '(': case ')': case '<': case '>': case '@':
	case ',': case ';': case ':': case '\\': case '"':
	case '/': case '[': case ']': case '?': case '=':
	case '{': case '}': case ' ': case '\t':
		return true;
	default:
		return false;
	}
}

bool http_parser::is_digit(int c)
{
	return c >= '0' && c <= '9';
}

//////////////////////////////////////////////////////////////////////////////////////////
static const int CONN_BREAK_MASK = 0x04;

http_stream::http_stream(connection* conn)
:state_(reading_header)
,conn_(conn)
,buf_(NULL)
{
}

http_stream::~http_stream()
{
	if(buf_) delete buf_;
}

void http_stream::send_response(int status,str_hash_map* headers,const void* body,size_t len,int flag/*=send_keepalive*/)
{
	// the length of "HTTP/"
	size_t size = 5; 
	// the total length of major,.,minor and space
	size += base::get_digit_len(parser_.hdr_.major) + base::get_digit_len(parser_.hdr_.minor) + 2;
	
	// the total length of text and \r\n
	const char* status_line = http_get_status_line(status);
	if(NULL==status_line)
		status_line = "unknow";

	//include end of line: \r\n, there are 2 bytes
	size += strlen(status_line) + 2;

	str_hash_map::iterator it;
	if(headers){
		//the total length of field name,value,:,space,and \r\n
		for(it = headers->begin();it != headers->end();++it){
			size += it->first.length() + it->second.length() + 4;
		}
	}
	//the length of \r\n end
	size += 2; 
	if(body) size += len; 
	
	buffer* buf = new (size) buffer(size);
	char* data = buf->data_;
	int ret = sprintf(data,"HTTP/%d.%d %s\r\n",parser_.hdr_.major,parser_.hdr_.minor,status_line);
	data += ret;
	
	if(headers){
		for(it = headers->begin();it != headers->end();++it){
			ret = sprintf(data,"%s: %s\r\n",it->first.c_str(),it->second.c_str());
			data += ret;
		}
	}
	memcpy(data,"\r\n",2); data += 2;
	if(body && len) memcpy(data,body,len);

	buf->dlen_ = size;
	conn_->out_queue_.push(buf);

	switch(flag){
		case nosend: break;

		case send_close:
		case send_keepalive:
			if(send_close==flag)
				conn_->flag_ |= connection::MASK_BIT_CLOSE;
			else
				conn_->flag_ &= ~connection::MASK_BIT_CLOSE;
			conn_->async_send();	
			break;		

		default: assert(false);
	}
}

void http_stream::send_error(int status)
{
	char buf[1024];

	const char* line = http_get_status_line(status), *fmt;
	assert(line);
	const char* type;

	int ret;
	if(conn_->flag_&connection::MASK_BIT_BROWSER){
		type = "application/json";
		fmt = "%s({\"code\":%d,\"message\":\"%s\",\"server\":\"%s\"})";
		ret = snprintf(buf,sizeof(buf),fmt,conn_->callback_.c_str(),status,line,NTI_PROXY_SRV);
		status = HTTP_OK;
	}else{
		type = "text/html";
		fmt =  "<HTML><HEAD>\n" \
		"<TITLE>%s</TITLE>\n" \
		"</HEAD><BODY>\n" \
		"<H1>%s</H1>\n" \
		"</BODY></HTML>\n";
		ret = snprintf(buf,sizeof(buf),fmt,line,line);
	}
	buf[ret] = '\0';

	str_hash_map headers;
	headers.insert(make_pair("Access-Control-Allow-Origin","*"));
	headers.insert(make_pair("Content-Type",type));
	headers.insert(make_pair("Server",NTI_PROXY_SRV));
	headers.insert(make_pair("Connection","close"));

	char len[32];
	sprintf(len,"%d",ret);
	headers.insert(make_pair("Content-Length",len));

	send_response(status,&headers,buf,ret,send_close);
}

void http_stream::send_ok(bool is_html,int flag/*=send_keepalive*/)
{
	const char *fmt, *type;
	char buf[1024];
	int ret;

	if(is_html){
		type = "text/html";
		fmt =  "<HTML><HEAD>\n" \
		"<TITLE>%s</TITLE>\n" \
		"</HEAD><BODY>\n" \
		"<H1>%s</H1>\n" \
		"</BODY></HTML>\n";
		const char* line = http_get_status_line(HTTP_OK);
		ret = snprintf(buf,sizeof(buf)-1,fmt,line,line);
	}else{
		type = "application/json";
		fmt = "{}";
		ret = snprintf(buf,sizeof(buf)-1,"%s",fmt);
	}
	buf[ret] = '\0';

	str_hash_map headers;
	headers.insert(make_pair("Content-Type",type));
	headers.insert(make_pair("Server",NTI_PROXY_SRV));

	char len[32];
	sprintf(len,"%d",ret);
	headers.insert(make_pair("Content-Length",len));

	send_response(HTTP_OK,&headers,buf,ret,flag);
}

void http_stream::send_crossdomain()
{
	/*static __thread int s_fd = -1;
	static __thread char s_xml[1024];
	static __thread ssize_t s_ret;

	if(-1==s_fd){
		string_t xml_path = server::instance()->cur_path_ + (string_t)"crossdomain.xml";
		s_fd = open(xml_path.c_str(),O_RDONLY,0);
		if(-1==s_fd){
			 log_fatal("http stream open crossdomain.xml fail: errno=%d",errno);
			 send_error(HTTP_NOT_FOUND);
			 return;
		}
		s_ret = read(s_fd,s_xml,sizeof(s_xml));
	}
	if(s_ret <= 0){
		 log_fatal("http stream read crossdomain.xml fail: errno=%d",errno);
		 send_error(HTTP_NO_CONTENT);
		 return;		
	}*/

	str_hash_map headers;
	headers.insert(make_pair("Content-Type","text/xml"));
	headers.insert(make_pair("Server",NTI_PROXY_SRV));

	char len[32];
	sprintf(len,"%ld",g_cross_domain_data.size());
	headers.insert(make_pair("Content-Length",len));
	
	send_response(HTTP_OK,&headers,g_cross_domain_data.data(),g_cross_domain_data.size(),send_keepalive);
}

void http_stream::send_system_info()
{
	const char* fmt = "{\"client_conns\":%d,\"server_conns\":%d,\"match_conns\":%d";

	int client_conns = client_conn_table::instance()->size();
	int matched_conns = client_conn_table::instance()->match_size();
	int server_conns = server_conn_table::instance()->size();

	char buf[1024];
	const size_t fmt_len = sizeof(buf)-1;
	int ret = snprintf(buf,fmt_len,fmt,client_conns,server_conns,matched_conns);

	float cpu,mem;
	unsigned long long uptime;
	if(base::get_process_usage(getpid(),cpu,mem,&uptime)){
		unsigned int day,hour,minute,sec, remain;
		day = uptime/86400, remain = uptime%86400;
		hour = remain/3600, remain = remain%3600;
		minute = remain/60, sec = remain%60;
		ret += snprintf(buf+ret,fmt_len-ret,",\"self_cpu\":%0.2f,\"self_mem\":%0.2f,\"uptime\":\"%d day %d hour %d minute %d second\"",
						cpu,mem,day,hour,minute,sec);
	}
//	if(base::get_system_usage(cpu,mem))
//		ret += snprintf(buf+ret,fmt_len-ret,",\"sys_cpu\":%0.2f,\"sys_mem\":%0.2f",cpu,mem);
	
	ret += snprintf(buf+ret,fmt_len-ret,",\"server\":\"%s\"}",NTI_PROXY_SRV);
	buf[ret] = '\0';
	
	str_hash_map headers;
	headers.insert(make_pair("Content-Type","application/json"));
	headers.insert(make_pair("Server",NTI_PROXY_SRV));

	char len[32];
	sprintf(len,"%d",ret);
	headers.insert(make_pair("Content-Length",len));

	send_response(HTTP_OK,&headers,buf,ret,send_keepalive);
}

void http_stream::read_request()
{
	assert(conn_);

	int   ds;
	const char* desc;

	for(;;){
		ds = read_message(desc);
		switch(ds){
			case data_finish: {
				connection* other = conn_->other_;
				if(other && other->out_queue_.is_high()){
					conn_->disable_read();
					return;
				}else if(reading_header==state_&&conn_->is_detach())
					return;
				}
				break;

			case data_unreadable:
				return;

			case data_error:
				return;

			case conn_closed:
			case conn_error:
				onReadError(conn_closed==ds?true:false,desc);
				return;
		}
	} 
}

void http_stream::read_response()
{
	read_message_no_parse();
}

void http_stream::read_message_no_parse()
{
	connection *other = conn_->other_;
	assert(other);

	ssize_t ret; 

	for(;;){
		if(NULL==buf_){
			int len = SSL_READ_BUFFER_LEN;
			if (conn_->flag_&connection::MASK_BIT_SSL){
				len = SSL_READ_BUFFER_LEN;
			}else{
				if(ioctl(conn_->sock_,FIONREAD,&len) || len > READABLE_MAX_LEN)
					len = READABLE_MAX_LEN; 
			}	
			buf_ = new (len) buffer(len);
		}

		//the value of new buf_size argument and value of buffer constructor buf_size argument must be same
		ret = conn_->recv(buf_->data_,buf_->dsize_);

		if(ret > 0){
			buf_->dlen_ = ret;
			other->out_queue_.push(buf_);
			buf_ = NULL;
			if(other->out_queue_.is_high()){
				conn_->disable_read();
				other->async_send(OTHER_WR_ERROR);
				break;
			}	
		}else if(OP_AGAIN==ret){
			if(!other->out_queue_.empty()) 
				other->async_send(OTHER_WR_ERROR);
			break;
		}else{
			delete buf_;  buf_ = NULL;
			//the connection had occurred error or been closed
			log_debug("%d-%s:%d connection retransmit fail",conn_->sock_,conn_->ip_.c_str(),conn_->port_);
			throw io_exception(SELF_RD_ERROR);
			break;
		}
	}
}

int http_stream::read_message(const char*& desc)
{
	int ds;

	switch(state_){
		case reading_header: 
			ds = read_header();
			if(data_error==ds){
				onHeaderError(HTTP_BAD_REQUEST); 
			}else if(ds&CONN_BREAK_MASK)
				desc = "read_header";
			break;

		case reading_body:
			ds = read_body();
			if(data_error==ds){
				onBodyError(HTTP_BAD_REQUEST); 
			}else if(ds&CONN_BREAK_MASK){
				if(body_type_==HTTP_BODY_CONTENT)
					desc = "read_body_content";
				else if(HTTP_BODY_CHUNK_DATA==body_type_)
					desc = "read_chunk_data";
				else 
					desc = "read_chunk_size";
			}
			break;

		case reading_trailer:
			ds = read_trailer();
			if(data_error==ds){
				onTrailerError(HTTP_BAD_REQUEST); 
			}else if(ds&CONN_BREAK_MASK){
				desc = "read_trailer";
			}
			break;

		default: assert(false); break;
	}
	return ds;
}

int http_stream::read_header()
{
	assert(reading_header==state_);

	buffer *tmp;
	char* data;
	size_t dsize,dlen,ndlen;

	if(NULL==buf_)
		buf_ = new (HTTP_INIT_HEADER_SIZE) buffer(HTTP_INIT_HEADER_SIZE);

	ssize_t ret = conn_->recv(buf_->data_+buf_->dlen_,1);		
	if(ret > 0) {
		buf_->dlen_ += ret;
		data = buf_->data_;
		dlen = buf_->dlen_;

		if(dlen >= 4 &&data[dlen-4]=='\r'&&data[dlen-3]=='\n'&&data[dlen-2]=='\r'&&data[dlen-1]=='\n'){
			int res;
			parser_.reset();
			if(0==(res=parser_.parse_header(data,dlen))){
				int error = 0;
				if(parser_.get_body_info(body_type_,body_len_,error)){
					if(HTTP_CONTINUE==error){
			   		    send_response(error,NULL,NULL,0);
						char str[]="Except: 100-Continue\r\n";
						char* beg = strcasestr(data,str),*end;
						assert(beg);
						end = beg + sizeof(str) - 1;
						memmove(beg,end,data+parser_.hdr_len_-end);
						buf_->dlen_ -= sizeof(str) -1;
					}
					onHeader();
					if (HTTP_BODY_CONTENT==body_type_&&body_len_ || HTTP_BODY_CHUNK_SIZE==body_type_){
						state_ = reading_body;	
						body_tran_ = 0;
						dsize = (HTTP_BODY_CONTENT==body_type_?std::min<size_t>(HTTP_EACH_BODY_SIZE,body_len_):HTTP_CHUNK_SIZE_LEN);
						buf_ = new (dsize) buffer(dsize);
					}
				}else
					return data_error;					
			}else if(-1==res){
				return data_error;
			}else{
				char fmt[128];
				snprintf(fmt,sizeof(fmt),"http_stream read_header %%.%lus",dlen);
				log_fatal(fmt,data);
				assert(false);
			}
		}else if(dlen < http_max_header_size && dlen == buf_->dsize_){
			ndlen = dlen << 1;
			tmp = new (ndlen) buffer(ndlen);
			memcpy(tmp->data_,data,tmp->dlen_=dlen);
			delete buf_; 
			buf_ = tmp;
		}else if(dlen >= http_max_header_size){
			return data_error;
		}
		return data_finish;

	}else if(OP_AGAIN==ret){
		return data_unreadable;
	}else
		return OP_CLOSE==ret?conn_closed:conn_error;
}

int http_stream::read_body()
{
	assert(reading_body==state_);

	ssize_t ret;
	size_t dsize,dlen,ndlen;
	char* data;
	buffer* nbuf;

	switch(body_type_){
		case HTTP_BODY_CONTENT:
		case HTTP_BODY_CHUNK_DATA:
			ret = conn_->recv(buf_->data_+buf_->dlen_,buf_->dsize_-buf_->dlen_);
			if(ret > 0){
				buf_->dlen_ += ret;
				if(buf_->dlen_==buf_->dsize_){
					body_tran_ += buf_->dlen_;
					onBody();
					if(body_tran_ < body_len_){
						dsize = std::min<size_t>(body_len_-body_tran_,HTTP_EACH_BODY_SIZE);
						buf_ = new (dsize) buffer(dsize);
					}else{
						if(HTTP_BODY_CHUNK_DATA==body_type_){
							body_type_ = HTTP_BODY_CHUNK_SIZE;
						    buf_ = new (HTTP_CHUNK_SIZE_LEN) buffer(HTTP_CHUNK_SIZE_LEN);
						}else{
							state_ = reading_header; 
						}
					}
				}
				return data_finish;

			}else if(OP_AGAIN==ret)
				return data_unreadable;
			else
				return OP_CLOSE==ret?conn_closed:conn_error;
			break;

		case HTTP_BODY_CHUNK_SIZE:
			ret = conn_->recv(buf_->data_+buf_->dlen_,1);
			if(ret > 0){
				buf_->dlen_ += ret;
				dlen = buf_->dlen_;
				data = buf_->data_;

				if(dlen>2&&data[dlen-2]=='\r'&&data[dlen-1]=='\n'){
					if(!base::strtoul(data,(unsigned long&)body_len_,16))
						return data_error;
					
					onBody();			
					if(body_len_){
						body_type_ = HTTP_BODY_CHUNK_DATA;
						body_tran_ = 0;
						dsize = std::min<size_t>(body_len_+=2,HTTP_EACH_BODY_SIZE);
						buf_ = new (dsize) buffer(dsize);
					}else{
						state_ = reading_trailer;
						buf_ = new (HTTP_INIT_HEADER_SIZE) buffer(HTTP_INIT_HEADER_SIZE);
					}
				}else if(dlen < HTTP_MAX_CHUNK_SIZE_LEN && dlen==buf_->dsize_){
					ndlen = dlen << 1;
					nbuf = new (ndlen) buffer(ndlen);
					memcpy(nbuf->data_,data,nbuf->dlen_=dlen);
					delete buf_; 
					buf_ = nbuf;
				}else if(dlen >= HTTP_MAX_CHUNK_SIZE_LEN)
					return data_error;		
				return data_finish;

			}else if(OP_AGAIN==ret)
				return data_unreadable;
			else
				return OP_CLOSE==ret?conn_closed:conn_error;
			break;

		default: assert(false); break;
	}
}

int http_stream::read_trailer()
{
	assert(reading_trailer==state_);

	buffer *nbuf;
	char* data;
	size_t dlen,ndlen;

	ssize_t ret = conn_->recv(buf_->data_+buf_->dlen_,1);
	if(ret > 0) {
		buf_->dlen_ += ret;
		data = buf_->data_;
		dlen = buf_->dlen_;

		if(dlen >= 2 &&data[dlen-2]=='\r'&&data[dlen-1]=='\n'){
			switch(parser_.parse_trailer(data,dlen)){
				case -1: 
					return data_error;
				case 0: 						
					onBody(); 
					state_ = reading_header;
					break;
				default: assert(false); break;
			}			
		}else if(dlen < http_max_header_size && dlen == buf_->dsize_){
			ndlen = dlen << 1;
			nbuf = new (ndlen) buffer(ndlen);
			memcpy(nbuf->data_,data,nbuf->dlen_=dlen);
			delete buf_; 
			buf_ = nbuf;
		}else if(dlen >= http_max_header_size)
			return data_error;
		return data_finish;

	}else if(OP_AGAIN==ret)
		return data_unreadable;
	else
		return OP_CLOSE==ret?conn_closed:conn_error;
}

bool http_stream::handle_special_request()
{
	http_header &hdr = parser_.hdr_;

	if(HTTP_GET==hdr.request.verb){
		char *uri_beg = hdr.request.uri,*uri_end = uri_beg+hdr.request.len, old = *uri_end;
		*uri_end = '\0';

		bool del = true;
		if(strcasestr(uri_beg,"crossdomain.xml")){
			send_crossdomain();
		}else if(0==strcmp(uri_beg,"/")){
			send_system_info();
		}else
			del = false;

		*uri_end = old;

		if(del){
			delete buf_; buf_ = NULL;
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
void http_stream::onHeader()
{
	http_header &hdr = parser_.hdr_;

	if(HTTP_REQUEST==hdr.type){
		char fmt[128];
		snprintf(fmt,sizeof(fmt),"%%d-%%s:%%d,len=%%d,ssl=%%d,%%.%lus",parser_.hdr_len_);
		log_info(fmt,conn_->sock_,conn_->ip_.c_str(),conn_->port_,parser_.hdr_len_,
			conn_->flag_&connection::MASK_BIT_SSL,buf_->data_);

		if(handle_special_request())
			return;		
		
		conn_->handle_uri(buf_,hdr.request.verb,hdr.request.uri,hdr.request.len);
	}else
		throw http_exception(HTTP_BAD_REQUEST);
}

void http_stream::onHeaderError(int error)
{
	throw http_exception(error);
}

void http_stream::onBody()
{
	connection*& other = conn_->other_;

	st_buffer_queue* bq = other ? &other->out_queue_ : &conn_->in_queue_;
	bq->push(buf_);  buf_ = NULL;

	if(other && !other->out_queue_.empty()){
		other->async_send(OTHER_WR_ERROR);			
	}
}

void http_stream::onBodyError(int error)
{
	throw http_exception(error);
}

void http_stream::onTrailerError(int error)
{
	throw http_exception(error);
}

void http_stream::onReadError(bool closed,const char*desc)
{
	log_debug("%d-%s:%d http_stream %s fail",conn_->sock_,conn_->ip_.c_str(),conn_->port_,desc);
	throw io_exception(SELF_RD_ERROR); 
}
