#ifndef _BASE_CONTIDION_H
#define _BASE_CONTIDION_H

#include "mutex.h"

namespace base
{
class condition : noncopyable
{
public:
	condition(mutex& m) : mutex_(m)
	{ pthread_cond_init(&cond_, NULL);	}

	~condition()
	{ pthread_cond_destroy(&cond_); }

	void wait()
	{ pthread_cond_wait(&cond_, mutex_);}

	void notify()
	{ pthread_cond_signal(&cond_); }

	void notifyAll()
	{ pthread_cond_broadcast(&cond_); }

private:
	mutex& mutex_;
	pthread_cond_t cond_;
};
}

#endif
