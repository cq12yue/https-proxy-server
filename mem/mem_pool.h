#ifndef _MEMORY_MEM_POOL_H
#define _MEMORY_MEM_POOL_H

#include "../base/null_mutex.h"
#include "../base/mutex.h"
#include "../base/singleton.h"
#include "smart_pool.h"
#include <cstddef>
#include <new>

namespace memory
{
/**
 *	@class tls_mem_pool 
 *	@brief a non thread-safe mem_pool that use thread local storage (tls).
 *  malloc and free memory which ptr to point must be in the same thread,otherwise 
 *  will result in undefined behaviour
 */
class tls_mem_pool 
{
public:
	//allocate memory,its length is size bytes
	static void* malloc(size_t size)
	{
		try{
			return get_cur_tls_pool()->malloc(size);
		}catch(...){
			return NULL;
		}
	}
	//free memory which ptr to point
	static void free(void* ptr)
	{
		try{
			get_cur_tls_pool()->free(ptr);
		}catch(...){
		}
	}
	
	//allocate memory,its length is size bytes
	static void* allocate(size_t size)
	{
		try{
			return get_cur_tls_pool()->allocate(size);
		}catch(...){
			return NULL;
		}
	}
	//free memory which ptr to point,and its length is size bytes
	static void deallocate(void* ptr,size_t size)
	{
		try{
			get_cur_tls_pool()->deallocate(ptr,size);
		}catch(...){
		}
	}

private:
	static smart_pool* get_cur_tls_pool();
};

/**
 * @class pgs_mem_pool
 * @brief process global storage
 */
template<class LockT>
class pgs_mem_pool 
{
public:
	//allocate memory,its length is size bytes
	static void* malloc(size_t size)
	{
		try{
			smart_pool* pool = base::singleton<smart_pool,LockT>::instance();
			base::lock_guard<LockT> guard(lock_);
			return pool->malloc(size);
		}catch(...){
			return NULL;
		}
	}
	//free memory which ptr to point
	static void free(void* ptr)
	{
		try{
			smart_pool* pool = base::singleton<smart_pool,LockT>::instance();
			base::lock_guard<LockT> guard(lock_);
			pool->free(ptr);
		}catch(...){
		}
	}

	//allocate memory,its length is size bytes
	static void* allocate(size_t size)
	{
		try{
			smart_pool* pool = base::singleton<smart_pool,LockT>::instance();
			base::lock_guard<LockT> guard(lock_);
			return pool->allocate(size);
		}catch(...){
			return NULL;
		}
	}
	//free memory which ptr to point,and its length is size bytes
	static void deallocate(void* ptr,size_t size)
	{
		try{
			smart_pool* pool = base::singleton<smart_pool,LockT>::instance();
			base::lock_guard<LockT> guard(lock_);
			pool->deallocate(ptr,size);
		}catch(...){
		}
	}

private:
	static LockT lock_;
};

template<class LockT>
LockT pgs_mem_pool<LockT>::lock_;

//two type definitions with regard to class pgs_mem_pool,each one with different lock
typedef pgs_mem_pool<base::null_mutex> st_pgs_mem_pool;
typedef pgs_mem_pool<base::mutex>      mt_pgs_mem_pool;

}

#endif
