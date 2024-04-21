#ifndef _BASE_ATOMIC_H
#define _BASE_ATOMIC_H

#include <stdint.h>
#include <cstddef>

namespace base
{
template<typename T>
class atomic_base 
{
public:
	typedef T int_type;
	typedef atomic_base<T> self_type;

public:
	atomic_base(int_type val = int_type())
	: val_(val)
	{
	}
	~atomic_base()
	{
	}
	
	atomic_base(const self_type& other)
	{
		if(this!=&other){
			int_type val = other;
			__sync_lock_test_and_set(&val_, val);
		}
	}
	self_type& operator = (const self_type& other)
	{
		if (this!=&other){
			int_type val = other;
			__sync_lock_test_and_set(&val_, val);
		}
		return *this;
	}
	operator int_type() const
	{
		return __sync_val_compare_and_swap(const_cast<volatile int_type*>(&val_),0,0);
	}
	
	self_type operator++(int)
	{
		atomic_base tmp = *this;
		__sync_fetch_and_add(&val_,1);
		return tmp;
	}

	self_type& operator++()
	{
		__sync_add_and_fetch(&val_,1);
		return *this;
	}

	self_type operator--(int)
	{
		atomic_base tmp = *this;
		__sync_fetch_and_sub(&val_,1);
		return tmp;
	}

	self_type& operator--()
	{
		__sync_sub_and_fetch(&val_,1);
		return *this;
	}

	self_type& operator+=(int_type val)
	{
		__sync_add_and_fetch(&val_,val);
		return *this;
	}

	self_type& operator-=(int_type val)
	{
		__sync_sub_and_fetch(&val_,val);
		return *this;
	}

	self_type& operator&=(int_type val)
	{ 
		__sync_and_and_fetch(&val_, val); 
		return *this;
	}

    self_type& operator|=(int_type val)
	{
		__sync_or_and_fetch(&val_,val); 
		return *this;
	}
	
	self_type& operator^=(int_type val)
	{
		__sync_xor_and_fetch(&val_,val); 
		return *this;
	}

private:
	volatile T val_;
};

template<typename T>
class atomic_base<T*> 
{
public:
	typedef T* pointer_type;
	typedef atomic_base<T*> self_type;

public:
	atomic_base(const pointer_type val = NULL)
		: val_(val)
	{
	}

	atomic_base(const self_type& other)
	{
		pointer_type val = other;
		__sync_lock_test_and_set(&val_, val);
	}

	self_type& operator=(const self_type& other)
	{
		if (this!=&other){
			pointer_type val = other;
			__sync_lock_test_and_set(&val_, val);
		}
		return *this;
	}

	self_type& operator=(const pointer_type& val)
	{
		__sync_lock_test_and_set(&val_, val);
		return *this;
	}

	operator pointer_type() const
	{
		return __sync_val_compare_and_swap(const_cast<volatile pointer_type*>(&val_),0,0);
	}

	self_type operator++(int)
	{
		atomic_base tmp = *this;
		__sync_fetch_and_add(&val_,1);
		return tmp;
	}

	self_type& operator++()
	{
		__sync_add_and_fetch(&val_,1);
		return *this;
	}

	self_type operator--(int)
	{
		atomic_base tmp = *this;
		__sync_fetch_and_sub(&val_,1);
		return tmp;
	}

	self_type& operator--()
	{
		__sync_sub_and_fetch(&val_,1);
		return *this;
	}

	self_type& operator+=(std::ptrdiff_t d)
	{
		__sync_add_and_fetch(&val_,d);
		return *this;
	}

	self_type& operator-=(std::ptrdiff_t d)
	{
		__sync_sub_and_fetch(&val_,d);
		return *this;
	}

private:
	volatile T* val_;
};

template<>
class atomic_base<bool>
{
public:
	typedef atomic_base<bool> self_type;
public:
    atomic_base(bool val = false)
		: val_(val)
	{
	}
	
	atomic_base(const self_type& other)
	{
		__sync_lock_test_and_set(&val_, other.val_);
	}
	
	self_type& operator=(const self_type& other)
	{
		if (this!=&other)
			__sync_lock_test_and_set(&val_, other.val_);
		return *this;
	}
	
	operator bool() const
	{
		return __sync_val_compare_and_swap(const_cast<volatile bool*>(&val_),0,0);
	}

	self_type& operator&=(bool val)
	{ 
		__sync_and_and_fetch(&val_, val); 
		return *this;
	}

	self_type& operator|=(bool val)
	{
		__sync_or_and_fetch(&val_,val); 
		return *this;
	}

	self_type& operator^=(bool val)
	{
		__sync_xor_and_fetch(&val_,val); 
		return *this;
	}

private:
	volatile bool val_;
};

//basic standard integer type
typedef atomic_base<bool>				atomic_bool;
typedef atomic_base<signed char>		atomic_char;
typedef atomic_base<unsigned char>		atomic_uchar;
typedef atomic_base<short>				atomic_short;
typedef atomic_base<unsigned short>		atomic_ushort;
typedef atomic_base<int>				atomic_int;
typedef atomic_base<unsigned int>		atomic_uint;
typedef atomic_base<long>				atomic_long;
typedef atomic_base<unsigned long>		atomic_ulong;
typedef atomic_base<long long>			atomic_llong;
typedef atomic_base<unsigned long long>	atomic_ullong;

//fixed size atomic integer type
typedef atomic_base<int8_t>			atomic_int8;
typedef atomic_base<uint8_t>		atomic_uint8;
typedef atomic_base<int16_t>		atomic_int16;
typedef atomic_base<uint16_t>	    atomic_uint16;
typedef atomic_base<int32_t>		atomic_int32;
typedef atomic_base<uint32_t>	    atomic_uint32;
typedef atomic_base<int64_t>		atomic_int64;
typedef atomic_base<uint64_t>	    atomic_uint64;
}

#endif
