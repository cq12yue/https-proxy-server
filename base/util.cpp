#include "util.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <linux/limits.h>
using namespace std;

size_t base::get_digit_len(size_t digit,size_t base /*=10*/)
{
	assert(base);

	size_t cnt = 0;
	do { ++cnt; digit /= base; } while(digit);
	return cnt;
}

bool base::get_exe_path(std::string &str)
{
	size_t size = PATH_MAX+1;
	str.resize(size);

	ssize_t ret = readlink("/proc/self/exe",(char*)str.data(),size);
	if (ret < 0 || ret >= size)
		return false;
	str[ret] = '\0';

	return true;
}

bool base::get_exe_name(std::string &str)
{
	if(!get_exe_path(str))
		return false;

	string::size_type pos = str.rfind('/');
	if(pos != string::npos)
		str.erase(0,pos+1);
	return true;
}

bool base::get_exe_dir(std::string &str,bool except_backslash /*=true*/)
{
	if(!get_exe_path(str))
		return false;

	string::size_type pos = str.rfind('/');
	if(string::npos == pos)
		return false;

	if (!except_backslash) ++pos;
	str.erase(pos);
	
	return true;
}
