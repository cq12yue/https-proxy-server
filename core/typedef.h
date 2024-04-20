#ifndef _CORE_TYPEDEF_H
#define _CORE_TYPEDEF_H

#include "../base/str_hash.h"
#include <ctype.h>
#include <string>
#include <ext/hash_map>

#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
#include "../mem/pool_allocator.h"
#endif

template<class T>
struct hash
{
	size_t operator()(const T& s) const
	{ return (size_t)base::bkdr_hash(s.c_str(),toupper); }
};

template<typename T>
struct hash<T*>
{
	size_t operator()(const T* p) const
	{ return __gnu_cxx::hash<unsigned long>()(reinterpret_cast<unsigned long>(p)); }
};

class connection;

#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
	typedef std::basic_string<char,std::char_traits<char>,memory::tls_allocator<char> > string_t;
	typedef memory::tls_allocator<string_t> str_alloc_t;
	typedef memory::tls_allocator<connection*> conn_alloc_t;
#else
	typedef std::string string_t;
	typedef std::allocator<string_t> str_alloc_t;
	typedef std::allocator<connection*> conn_alloc_t;
#endif

typedef hash<string_t> str_hash_fun;
typedef __gnu_cxx::hash_map<string_t,string_t,str_hash_fun,std::equal_to<string_t>,str_alloc_t> str_hash_map;
typedef __gnu_cxx::hash<int> int_hash_fun;
typedef __gnu_cxx::hash_map<int,connection*,int_hash_fun,std::equal_to<int>,conn_alloc_t> client_conn_map;
typedef __gnu_cxx::hash_multimap<string_t,connection*,str_hash_fun,std::equal_to<string_t>,conn_alloc_t> server_conn_map;

#endif
