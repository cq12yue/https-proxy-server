#ifndef MEMORY_NEW_POLICY_H
#define MEMORY_NEW_POLICY_H

#include "mem_pool.h"

namespace memory 
{
/**
 * @class any_new_policy
 * @brief can be used to construct single object with new and array object with new[]
 */
template<class Alloc>
class any_new_policy
{
public:
	static void* operator new(size_t size) throw (std::bad_alloc)
	{  
		void* ptr = Alloc::malloc(size);
		if(NULL==ptr) {
			throw std::bad_alloc();
		}
		return ptr;
	}

	static void* operator new(size_t size,void* ptr) throw()
	{ return ptr; }

	static void* operator new(size_t size,const std::nothrow_t&) throw()
	{ return Alloc::malloc(size); }

	static void operator delete(void* ptr) throw()
	{  Alloc::free(ptr); }

	static void operator delete(void* ptr, const std::nothrow_t&) throw()
	{  Alloc::free(ptr); }

	static void operator delete(void*, void*) throw()
	{ }

	static void* operator new[](size_t size) throw(std::bad_alloc)
	{ return operator new (size); }

	static void* operator new[](size_t size,void* ptr) throw()
	{ return ptr; }

	static void* operator new[](size_t size, const std::nothrow_t&) throw()
	{ return operator new (size, std::nothrow); }

	static void operator delete[](void* ptr) throw()
	{  operator delete (ptr); }

	static void operator delete[](void* ptr, const std::nothrow_t&) throw()
	{ operator delete (ptr); }

	static void operator delete[](void*, void*) throw()
	{ }
};

//three type definitions with regard to any_new_policy,each one with different memory pool
typedef any_new_policy<st_pgs_mem_pool>   st_any_new_policy;
typedef any_new_policy<mt_pgs_mem_pool>   mt_any_new_policy;
typedef any_new_policy<tls_mem_pool>	  tls_any_new_policy;

/**
 * @class special_new_policy
 * @brief only can be used to construct single object with new,not support array object with new[]
 */
template<typename T,class Alloc>
class special_new_policy
{
public:
	static void* operator new(size_t size) throw (std::bad_alloc)
	{  
		void* ptr = Alloc::allocate(sizeof(T));
		if(NULL==ptr) {
			throw std::bad_alloc();
		}
		return ptr;
	}

	static void* operator new(size_t size,void* ptr) throw()
	{ return ptr; }

	static void* operator new(size_t size,const std::nothrow_t&) throw()
	{ return Alloc::allocate(sizeof(T)); }

	static void operator delete(void* ptr) throw()
	{  Alloc::deallocate(ptr,sizeof(T)); }

	static void operator delete(void* ptr, const std::nothrow_t&) throw()
	{  Alloc::deallocate(ptr,sizeof(T)); }

	static void operator delete(void*, void*) throw()
	{}

private:
	static void* operator new[](size_t size) throw(std::bad_alloc);
	static void* operator new[](size_t size,void* ptr) throw();
	static void* operator new[](size_t size, const std::nothrow_t&) throw();

	static void operator delete[](void* ptr) throw();
	static void operator delete[](void* ptr, const std::nothrow_t&) throw();
	static void operator delete[](void*, void*) throw();
};

//for process that only has single thread
template<typename T>
class st_spec_new_policy : public special_new_policy<T,st_pgs_mem_pool>
{
};

//for process that has more than one thread
template<typename T>
class mt_spec_new_policy : public special_new_policy<T,mt_pgs_mem_pool>
{
};

/**
 * for process that has more than or equal to one thread,but memory manage of each thread  
 * is standalone,and has no performance cost from lock and unlock,so is efficient
 */
template<typename T>
class tls_spec_new_policy : public special_new_policy<T,tls_mem_pool>
{
};

}

#endif