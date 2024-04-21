#ifndef _BASE_NULL_MUTEX_H
#define _BASE_NULL_MUTEX_H

#include "noncopyable.h"
#include "lock_guard.h"

namespace base
{
class null_mutex
{
public:
	int lock() { return 0;}
	int trylock() { return 0;}
	int unlock() { return 0; }
};

template<>
class lock_guard<null_mutex>
{
public:
	explicit lock_guard(null_mutex& lock){}
	~lock_guard() {}
};
}//end base

#endif
