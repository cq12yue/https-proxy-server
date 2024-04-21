#include "smart_pool.h"
#include <utility>
using namespace memory;

void* smart_pool::malloc(size_t size)
{
	void* ptr = allocate(size);
	try{
		allocated_chunk_.insert(std::make_pair((key_type)ptr,size));
	}catch(...){
		deallocate(ptr,size);
		ptr = NULL;
	}
	return ptr;
}

void smart_pool::free(void* ptr)
{
	chunk_hash_map::iterator it = allocated_chunk_.find((key_type)ptr);
	if(it != allocated_chunk_.end()){
		deallocate(ptr,it->second);
	}else{
	/* 
	  if the ptr not in allocated_chunk that had recorded allocated chunk,then 
	  it is error that to be avoid,because this tls_pool class is designed to 
	  apply to single thread.
	*/
		assert(false); 
	}
}
