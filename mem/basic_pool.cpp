#include "basic_pool.h"
#include <stdlib.h>  //for malloc,free
#include <string.h>  //for memset

#ifndef NDEBUG
#include <stdio.h> //for printf,snprintf
#endif

#include "../base/cstring.h"

using namespace memory;

basic_pool::basic_pool()
:heap_list_(NULL)
,start_free_(NULL)
,end_free_(NULL)
{
	memset(free_list_,0,sizeof(free_list_));
}

basic_pool::~basic_pool()
{
	release();
}

//If allocate fail,then throw std::bad_alloc exception
void* basic_pool::allocate(size_t size)
{
	assert(size);
	#ifndef NDEBUG
//	printf(4==sizeof(size_t)?"allocate size=%u\n":"allocate size=%llu\n",size);
	#endif

	void* ptr;
	if(size <= MaxChunkSize) {
		size_t idx = basic_pool::chunk_index(size);
		assert(idx<ChunkNum);

		free_node* &head = free_list_[idx],*node;
		if(NULL==head){
			ptr = refill(head,basic_pool::round_up(size));
		}else{
			node = head, head = head->next;		
			ptr = node;
		}
	}else {
        ptr = malloc(size);
	}
	return ptr;
}

//never throw any exception
void basic_pool::deallocate(void* ptr,size_t size)
{
	assert(ptr && size);

	if(size<=MaxChunkSize) {
		size_t idx = basic_pool::chunk_index(size);
		assert(idx<ChunkNum);

		free_node *&head = free_list_[idx],*node;
		node = (free_node*)ptr;
		node->next = head;
		head = node;
	}else{
		free(ptr);	
	}	
}

/**
	Note,the memory that had been recorded in heap_list_ can not be free directly,
	so first use extra memory to save them,then free them and extra memory.
 */
void basic_pool::release()
{
	heap_node *p;
	size_t n = 0;
	for (p=heap_list_; p; p=p->next)
	{	++n; 	}
	
	void** chunk = (void**)malloc(n*sizeof(void*));
	if(NULL==chunk) return;

	n = 0;
	for (p=heap_list_; p; p=p->next)
	{	chunk[n++] = p->mem; }

	while(n)
	{ free(chunk[--n]); }

	free(chunk);
}

//////////////////////////////////////////////////////////////////////////////////////////
#ifndef NDEBUG
int basic_pool::heap_num() const
{
	int n = 0;
	heap_node* p;
	for (p=heap_list_;p;p=p->next)
	{  ++n; }
	return n;
}
#endif

inline void basic_pool::add_heap(void* mem,size_t size)	
{
	heap_node* chunk = (heap_node*)allocate(sizeof(heap_node));		
	chunk->mem = mem;
	chunk->size = size;
	chunk->next = heap_list_ ? heap_list_: NULL;
	heap_list_ = chunk;
#ifndef NDEBUG
//	char buf[64];
//	snprintf(buf,sizeof(buf),"id=%%d,mem=%%p,size=%s\n",4==sizeof(size_t)?"%u":"%llu");
//	printf(buf,heap_num(),mem,size);
#endif
}

void* basic_pool::refill(free_node*& head,size_t size)
{
	assert(NULL==head);
    size_t num = NodeNum;
	char* chunk = (char*)chunk_alloc(size,num);
	assert(num);
	
#ifndef NDEBUG
//	printf(4==sizeof(size_t)?"refill size=%u,num=%d\n":"refill size=%llu,num=%d\n",size,num);
#endif

	if(1 != num) {
		char *cur,*next = chunk+size;
		head = (free_node*)next;

		for (size_t i=1;i<num-1;++i) {
			cur = next; next = cur + size;
			((free_node*)cur)->next = (free_node*)next;
		}
		((free_node*)next)->next = NULL;
	}	
	return chunk;
}

void* basic_pool::chunk_alloc(size_t size,size_t& num)
{
	size_t left_bytes = end_free_-start_free_;
	size_t total_bytes = size*num;
	char* ret;

	#ifndef NDEBUG
//	printf(4==sizeof(size_t)?"chunk_alloc size=%u,total=%u,left=%u\n":"chunk_alloc size=%llu,total=%llu,left=%llu\n",\
		size,total_bytes,left_bytes);
	#endif

	if(total_bytes<=left_bytes)	{
	    ret = start_free_;
		start_free_ += total_bytes;
		return ret;
	}
	else if(left_bytes>=size) {
		num = left_bytes/size;
		total_bytes = num * size;
		ret = start_free_;
		start_free_ += total_bytes;
		return ret;
	} else {
		free_node** head;
		if(left_bytes > 0) {
			assert(0==left_bytes%Alignment);
			head = free_list_ + chunk_index(left_bytes);
			((free_node*)start_free_)->next = *head;
			*head = ((free_node*)start_free_);
		}	
		size_t bytes = total_bytes<<1;
		start_free_ = (char*)malloc(bytes);
		if(NULL==start_free_) {
			for (size_t n = size+Alignment; n<=MaxChunkSize; n+=Alignment) {
				head = free_list_ + chunk_index(n);
				free_node* p = *head;
				if(p) {
					*head = p->next;
					start_free_ = (char*)p;
					end_free_ = start_free_ + n;
					return chunk_alloc(size,num);
				}
			}
		    return (end_free_ = NULL); 
		}
		end_free_ = start_free_ + bytes;
		add_heap(start_free_,bytes);
		return chunk_alloc(size,num);
	}
}
