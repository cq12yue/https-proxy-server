#ifndef _BASE_NONCOPYABLE_H
#define _BASE_NONCOPYABLE_H

namespace base
{
class noncopyable
{
protected:
	noncopyable() {}
	~noncopyable() {}

private:  
	noncopyable( const noncopyable& );
	const noncopyable& operator=( const noncopyable& );

};
}

#endif