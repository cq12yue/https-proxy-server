#include "cstring.h"
#include <ctype.h>  //for isspace
#include <stdlib.h> //for strtol,strtoul
#include <cassert>
#include <errno.h>
#include <stdio.h>

bool base::strtol(const char *str, long& val,int base/*=10*/,bool check_safe /*=false*/) 
{
	char *end;
	errno = 0;
	val = ::strtol(str,&end,base);

	if(errno==ERANGE|| (errno!=0&&0==val)) 
		return false;
	if(end==str)
		return false;

	if(check_safe && *end != '\0')
		return false;

	return true;
}

bool base::strtoul(const char *str, unsigned long& val,int base/*=10*/,bool check_safe /*=false*/) 
{
	char *end;
	errno = 0;
	val = ::strtoul(str,&end,base);

	if(errno==ERANGE|| (errno!=0&&0==val)) 
		return false;
	if(end==str)
		return false;

	if(check_safe && *end!='\0')
		return false;

	return true;
}

void base::printf(const char* str,size_t len)
{
	char fmt[32];
	int ret = snprintf(fmt,sizeof(fmt),"%%.%lus\n",len);
	fmt[ret] = '\0';
	::printf(fmt,str);
}

void base::strupr(char* str)
{
	for(char c;c=*str;++str)
	 	*str = toupper(c);
}

void base::strupr(char* str,size_t len)
{
	size_t n = 0;
	for(char c;c=*str && n<len;++str,++n)
		*str = toupper(c);
}

void base::strlwr(char* str)
{
	for(char c;c=*str;++str)
		*str = tolower(c);
}

void base::strlwr(char* str,size_t len)
{
	size_t n = 0;
	for(char c;c=*str && n<len;++str,++n)
		*str = tolower(c);
}
