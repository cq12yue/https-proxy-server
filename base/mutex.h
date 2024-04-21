#ifndef _BASE_MUTEX_H
#define _BASE_MUTEX_H

#include "noncopyable.h"
#include <pthread.h>

namespace base
{
class mutex : noncopyable
{
public:
	mutex()
	{ pthread_mutex_init(&lock_,NULL); }

	~mutex()
	{ pthread_mutex_destroy(&lock_); }

	int lock(const timespec* ptime = NULL)
	{ return ptime ? pthread_mutex_timedlock(&lock_,ptime):pthread_mutex_lock(&lock_); }

	int trylock()
	{ return pthread_mutex_trylock(&lock_);	}

	int unlock()
	{ return pthread_mutex_unlock(&lock_); }

	operator pthread_mutex_t* ()
	{ return &lock_; }

private:
	pthread_mutex_t lock_;
};

}// end base

#endif
