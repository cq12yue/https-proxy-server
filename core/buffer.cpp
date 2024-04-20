#include "buffer.h"
#include <string.h> //for memcpy
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////////////////////
buffer::buffer(size_t buf_size /*= 1*/)
:dlen_(0)
,dtran_(0)
,next_(NULL)
{
	dsize_ = buf_size ? buf_size : 1; 
}

buffer::buffer(const void* data,size_t len)
:dsize_(len)
,dlen_(len)
,dtran_(0)
,next_(NULL)
{
	//it must has enough space to copy data first
	memcpy(data_,data,len);
}

void* buffer::operator new(size_t size,size_t buf_size /*= 1*/)
{ 
	if (buf_size) --buf_size;  
#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
	return memory::tls_mem_pool::allocate(sizeof(buffer)+buf_size);	
#else
	return malloc(sizeof(buffer)+buf_size);
#endif
}

void buffer::operator delete(void* ptr)
{
	if(NULL==ptr) return;
#if defined(_USE_MEM_POOL) && _USE_MEM_POOL == 1
	memory::tls_mem_pool::deallocate(ptr,static_cast<buffer*>(ptr)->dsize_-1+sizeof(buffer));
#else
	free(ptr);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
