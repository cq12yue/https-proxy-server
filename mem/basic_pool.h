#ifndef _MEMORY_BASIC_POOL_H
#define _MEMORY_BASIC_POOL_H

#include <cstddef> //for size_t
#include <cassert>

namespace memory
{
/**
 *  @class basic_pool
 *  @brief a kind of basic pool based free list,by 16 bytes size align,
 *  max chunk size is 16k bytes,but lack of garbage collection.
 */
class basic_pool
{
protected:
	enum { Alignment = 16,MaxChunkSize = 16*1024 };
	enum { NodeNum = 16,ChunkNum = MaxChunkSize/Alignment};

public:
	basic_pool();
	virtual ~basic_pool();

	void* allocate(size_t size);
	void deallocate(void* ptr,size_t size);
	//void reallocate(void* p,size_t oldsz,size_t newsz);
	void release();

private:
	static size_t round_up(size_t size) {
		return (size+(size_t)Alignment-1)&(~((size_t)Alignment-1));
	}

	static size_t chunk_index(size_t size){
		assert(size && size <= MaxChunkSize);
		return (--size)/Alignment;
	}
	struct free_node; //Forward declaration of refill member function

	void* chunk_alloc(size_t size,size_t& num);
	void* refill(free_node*& head,size_t size);
	void  add_heap(void* mem,size_t size);
#ifndef NDEBUG
	int heap_num() const;
#endif

private:
	//one free list that has 1024 class sizes
	struct free_node{
		free_node* next;
	}* free_list_[ChunkNum];
	
	//one singled-list that record all allocated memory chunks,to be release later
	struct heap_node{
		void* mem;
		size_t size;
		heap_node* next;
	}* heap_list_;

	char* start_free_;
	char* end_free_;
};
}

#endif
