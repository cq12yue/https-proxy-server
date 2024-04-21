#ifndef _BASE_LOCKGUARD_H
#define _BASE_LOCKGUARD_H

#include "noncopyable.h"

namespace base
{
template<class T>
class lock_guard : noncopyable
{
public:
	explicit lock_guard(T& lock, bool block = true)
		: lock_(&lock)
	{
		block ? lock_->lock() : lock_->trylock();
	}
	~lock_guard()
	{
		lock_->unlock();
	}

private:
	T* lock_;
};
} //end base

#endif