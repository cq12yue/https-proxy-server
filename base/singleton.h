#ifndef _BASE_SINGLETON_H
#define _BASE_SINGLETON_H

#include "noncopyable.h"
#include "lock_guard.h"
#include "null_mutex.h"
#include <stdlib.h>  //for atexit

namespace base
{
template<typename T,class LockT = null_mutex,bool dynamic = true>
class singleton : noncopyable
{
public:
	static T* instance(bool bAutoDel = true)
	{
	    if (NULL == pT_) {
			lock_guard<LockT> guard(lock_);
			if (NULL == pT_) {
				pT_ = new T();
				autoDel_ = bAutoDel;
				if (autoDel_) 
					::atexit(destroy);
			}
		}
		return pT_;
	}
    static void destroy()
	{
		if (pT_) {
			lock_guard<LockT> guard(lock_);
		    delete pT_; 
			pT_ = NULL;
		}
	}

protected:
	singleton() {}
	~singleton() {}

private:
	static LockT lock_;
	static T* pT_;
	static bool autoDel_;
};

template<typename T,typename LockT,bool dynamic>
LockT singleton<T,LockT,dynamic>::lock_;

template<typename T,typename LockT,bool dynamic>
T* singleton<T,LockT,dynamic>::pT_ = NULL;

template<typename T,typename LockT,bool dynamic>
bool singleton<T,LockT,dynamic>::autoDel_ = true;

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename T,class LockT>
class singleton<T,LockT,false> : noncopyable
{
public:
	static T* instance()
	{
		if(NULL==pT_) {
			lock_guard<LockT> guard(lock_);
			if (NULL==pT_) {
				static T t;
				pT_ = &t;
			}
		}
		return pT_;
	}
	
protected:
	singleton() {}
	~singleton() {}

private:
	static LockT lock_;
	static T* pT_;
};
template<typename T,typename LockT>
LockT singleton<T,LockT,false>::lock_;

template<typename T,typename LockT>
T* singleton<T,LockT,false>::pT_ = NULL;

#define  SINGLETON_DECLARE(className,lock,dynamic) \
protected:\
	friend class base::singleton<className,lock,dynamic>;\
	className();\
	~className();\

} //end base

#endif
