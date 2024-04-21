#ifndef _MEMORY_SMART_POOL_H
#define _MEMORY_SMART_POOL_H

#include "basic_pool.h"
#include <ext/pool_allocator.h>
#include <ext/hash_map>

namespace memory 
{
class smart_pool : public basic_pool
{
public:
	void* malloc(size_t size);
	void free(void* ptr);

private:
	typedef unsigned long key_type;
	typedef size_t value_type;
	//typedef __gnu_cxx::hash_map<key_type,value_type> hash_map_chunk;

	typedef __gnu_cxx::hash<key_type> hash_fun;
	typedef __gnu_cxx::__pool_alloc<value_type> allocator_type;
	typedef __gnu_cxx::hash_map<key_type,value_type,hash_fun,std::equal_to<key_type>,allocator_type> chunk_hash_map;
	
	// note that the allocated_chunk_ is unsafe in multi-thread,therefore it is used in single-thread for tls.
	chunk_hash_map allocated_chunk_;
};
}

#endif
