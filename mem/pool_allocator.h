#ifndef _MEMORY_POOL_ALLOCATOR_H
#define _MEMORY_POOL_ALLOCATOR_H

#include "mem_pool.h"
#include <memory>

namespace memory
{
/**
 * @class tls_allocator
 * @brief a class template to be used in stl container for their memory allocator
 */
template<class T>
class tls_allocator : public std::allocator<T>
{
public:
	typedef std::allocator<T> base_type;
	typedef typename base_type::pointer pointer;
    typedef typename base_type::size_type size_type;

	template<class Other>
	struct rebind
	{
		typedef tls_allocator<Other> other;
	};

	tls_allocator() {}

	tls_allocator(tls_allocator<T> const&) {}

	tls_allocator<T>& operator=(tls_allocator<T> const&) 
	{ return (*this); }

	template<class Other>
	tls_allocator(tls_allocator<Other> const&) {}

	template<class Other>
	tls_allocator<T>& operator=(tls_allocator<Other> const&) 
	{ return (*this); }

    pointer allocate(size_type n,const void* p=0)
	{ return (pointer)tls_mem_pool::allocate(n*sizeof(T)); }

	void deallocate(pointer ptr, size_type n) 
	{ return tls_mem_pool::deallocate(ptr,n*sizeof(T)); }
};

/**
 * @class pgs_allocator
 * @brief a class template to be used in stl container for their memory allocator
 */
template<class T>
class pgs_allocator : public std::allocator<T>
{
public:
	typedef std::allocator<T> base_type;
	typedef typename base_type::pointer pointer;
	typedef typename base_type::size_type size_type;

	template<class Other>
	struct rebind
	{
		typedef pgs_allocator<Other> other;
	};

	pgs_allocator() {}

	pgs_allocator(pgs_allocator<T> const&) {}

	pgs_allocator<T>& operator=(pgs_allocator<T> const&) 
	{ return (*this); }

	template<class Other>
	pgs_allocator(pgs_allocator<Other> const&) {}

	template<class Other>
	pgs_allocator<T>& operator=(pgs_allocator<Other> const&) 
	{ return (*this); }

	pointer allocate(size_type n,const void* p=0)
	{ return (pointer)mt_pgs_mem_pool::allocate(n*sizeof(T)); }

	void deallocate(pointer ptr, size_type n) 
	{ return mt_pgs_mem_pool::deallocate(ptr,n*sizeof(T)); }
};

}

#endif
