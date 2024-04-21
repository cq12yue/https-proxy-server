#ifndef _BASE_TIMEVAL_H
#define _BASE_TIMEVAL_H

#include <sys/time.h>

namespace base
{
void timeval_normalize(timeval &t);

class timeval_t : public ::timeval
{
public:
	timeval_t(long sec = 0,long usec = 0)
	{
		tv_sec = sec, tv_usec = usec;
		timeval_normalize(*this);
	}
	
	timeval_t(const timeval& t)
	{
		if (this != &t){
			tv_sec = t.tv_sec, tv_usec = t.tv_usec;
			timeval_normalize(*this);
		}
	}
	
	timeval_t& operator = (const timeval &t)
	{
		if (this != &t){
			tv_sec = t.tv_sec, tv_usec = t.tv_usec;
			timeval_normalize(*this);
		}
		return *this;
	}
};

inline void operator += (timeval &t1,const timeval &t2)
{
	t1.tv_sec += t2.tv_sec,t1.tv_usec += t2.tv_usec;
	timeval_normalize(t1);
}

inline void operator -= (timeval &t1,const timeval &t2)
{
	t1.tv_sec -= t2.tv_sec, t1.tv_usec -= t2.tv_usec;
	timeval_normalize(t1);
}	

inline timeval operator + (const timeval &t1,const timeval &t2)
{
	return timeval_t(t1.tv_sec+t2.tv_sec,t1.tv_usec+t2.tv_usec);
}

inline timeval operator - (const timeval &t1,const timeval &t2)
{
	return timeval_t(t1.tv_sec-t2.tv_sec,t1.tv_usec-t2.tv_usec);
}

inline bool operator == (const timeval &t1,const timeval &t2)
{ 
	timeval_t t(t1.tv_sec-t2.tv_sec,t1.tv_usec-t2.tv_usec);
	return 0==t.tv_sec&&0==t.tv_usec;
}

inline bool operator != (const timeval &t1,const timeval &t2)
{
	return !(t1 == t2);
}

inline bool operator < (const timeval &t1,const timeval &t2)
{
	timeval t = t1 - t2;
	return t.tv_sec < 0;
}

inline bool operator > (const timeval &t1,const timeval &t2)
{
	timeval t = t1 - t2;
	return 0==t.tv_sec && t.tv_usec>0 || t.tv_sec>0;
}

inline bool operator <= (const timeval &t1,const timeval &t2)
{
	return !(t1 > t2);
}

inline bool operator >= (const timeval &t1,const timeval &t2)
{
	return !(t1 < t2);
}

}	

#endif
