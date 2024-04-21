#ifndef _BASE_CSTRING_H
#define _BASE_CSTRING_H

#include <cstddef>
#include <sstream>

namespace base
{
	/**
	 Safe convert from str to long number
		
	 If and only if str include digital char, result int convert successfully.

	 @param str source str to be convert 
	 @param out store the result of convert
	 @return if successful then true,otherwise false
	*/
	bool strtol(const char *str,long& val,int base = 10,bool check_safe = false);

	bool strtoul(const char *str,unsigned long& val,int base = 10,bool check_safe = false);

	void printf(const char* str,size_t len);
	
	void strupr(char* str);
	void strupr(char* str,size_t len);

	void strlwr(char* str);
	void strlwr(char* str,size_t len);

	template<typename T>
	inline void trim_left(std::basic_string<T>& str)
	{
		for(typename std::basic_string<T>::iterator it = str.begin(); it != str.end(); ){
			if (*it == (T)' '||*it == (T)'\t'||*it == (T)'\r'
				||*it == (T)'\n'||*it==(T)'\f'||*it==(T)'\v')
				it = str.erase(it);
			else
				break; 
		}
	}

	template<typename T>
	inline void trim_right(std::basic_string<T>& str)
	{
		for(typename std::basic_string<T>::reverse_iterator it = str.rbegin(); it != str.rend(); ){
			if (*it == (T)' '||*it == (T)'\t'||*it == (T)'\r'
				||*it == (T)'\n'||*it==(T)'\f'||*it==(T)'\v')
				it = typename std::basic_string<T>::reverse_iterator(str.erase(it.base() - 1));
			else
				break; 
		}
	}

	template<typename T>
	inline void trim(std::basic_string<T> &str)
	{
		trim_left(str); 
		trim_right(str);
	}
	
	template<typename T>
	inline std::basic_string<T> trim_copy(const std::basic_string<T> &str)
	{
		std::basic_string<T> s = str;
		trim(s);
		return s;
	}

	template <typename T,typename U>
	inline bool from_string(T &t, const std::basic_string<U> &s, std::ios_base & (*f)(std::ios_base&))
	{
		std::basic_istringstream<U> iss(s);
		return !(iss>>f>>t).fail();
	}

	template <typename U,typename T>
	inline std::basic_string<U> to_string(T t, std::ios_base & (*f)(std::ios_base&))
	{
		std::basic_ostringstream<U> oss; 
		oss << f << t;
		return oss.str();
	}
}

#endif
