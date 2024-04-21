#ifndef _BASE_UTIL_H
#define _BASE_UTIL_H

#include <stddef.h>
#include <string>

namespace base
{
	#define NUM_ELEMENTS(x) (sizeof((x))/sizeof(0[(x)])) 

	size_t get_digit_len(size_t digit,size_t base = 10);

	bool get_exe_path(std::string &str);

	bool get_exe_name(std::string &str);

	bool get_exe_dir(std::string &str,bool except_backslash = true);
}

#endif
