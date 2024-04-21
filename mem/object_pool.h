#ifndef _MEMORY_OBJECT_POOL_H
#define _MEMORY_OBJECT_POOL_H

#include "mem_pool.h"

namespace memory
{
/**
 * @class any_object_pool
 * @brief can be used to construct one object of any type and destroy it.
*/
template<class Alloc>
class any_object_pool
{
public:
	template<typename T>
	static T* construct() 
	{
		T* const p = static_cast<T*>(Alloc::allocate(sizeof(T)));
		try { new (p) T(); }
		catch(...){ Alloc::deallocate(p,sizeof(T)); throw; }
		return p;
	}

	template<typename T>
	static void destroy(T* const ptr)
	{
		if(ptr) {
			ptr->~T();
		    Alloc::deallocate(ptr,sizeof(T));
		}
	}
};

//three type definitions with regard to any_object_pool,each one with different memory pool
typedef any_object_pool<st_pgs_mem_pool>  st_any_obj_pool;
typedef any_object_pool<mt_pgs_mem_pool>  mt_any_obj_pool;
typedef any_object_pool<tls_mem_pool>	  tls_any_obj_pool;

/**
 * @class special_object_pool
 * @brief only can be used to construct one object of special type and destroy it,
 * in its internal,just call method of class any_object_pool to implement construct and destroy
*/
template<typename T,class Alloc>
class special_object_pool
{
public:
	static T* construct() 
	{  return any_object_pool<Alloc>::construct<T>(); }

	static void destroy(T* const ptr)
	{ any_object_pool<Alloc>::destroy(ptr); }
};

//for process that only has single thread
template<typename T>
class st_spec_obj_pool : public special_object_pool<T,st_pgs_mem_pool>
{
};

//for process that has more than one thread
template<typename T>
class mt_spec_obj_pool : public special_object_pool<T,mt_pgs_mem_pool>
{
};

/**
 * for process that has more than or equal to one thread,but memory manage of each thread  
 * is standalone,and has no performance cost from lock and unlock,so is efficient
 */
template<typename T>
class tls_spec_obj_pool : public special_object_pool<T,tls_mem_pool>
{
};

}

#endif