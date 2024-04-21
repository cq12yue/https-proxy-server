#ifndef _BASE_NULL_TYPE_H
#define _BASE_NULL_TYPE_H

namespace base
{
class null_type{};

inline bool operator==(const null_type&, const null_type&)
{
	return true;
}   

inline bool operator<(const null_type&, const null_type&)
{
	return false;
}

inline bool operator>(const null_type&, const null_type&)
{
	return false; 
}

}

#endif
