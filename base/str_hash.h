#ifndef _BASE_STR_HASH_H
#define _BASE_STR_HASH_H

namespace base 
{
	inline int default_char_conv(int c)
	{  return c; }

	template<typename function>
	unsigned int simple_hash(const char *str,function func)
	{
		unsigned int hash = 0;

		while(*str)
			hash = 31 * hash + func(*str++);

		return (hash & 0x7fffffff);
	}
	inline unsigned int simple_hash(const char* str)
	{ return simple_hash(str,default_char_conv); }

	template<typename function>
	unsigned int rs_hash(const char *str,function func)
	{
		 unsigned int b = 378551;
		 unsigned int a = 63689;
		 register unsigned int hash = 0;

		 while (*str){
			 hash = hash * a + func(*str++);
			 a *= b;
		 }

		 return (hash & 0x7fffffff);
	}
	inline unsigned int rs_hash(const char *str)
	{ return rs_hash(str,default_char_conv); }

	template<typename function>
	unsigned int js_hash(const char *str,function func)
	{
		 register unsigned int hash = 1315423911;

		 while (*str)
			 hash ^= ((hash << 5) + func(*str++) + (hash >> 2));
	    
		 return (hash & 0x7fffffff);
	}
	inline unsigned int js_hash(const char *str)
	{ return js_hash(str,default_char_conv); }

	template<typename function>
	unsigned int pjw_hash(const char *str,function func)
	{
		 unsigned int bitsinunignedint = (unsigned int)(sizeof(unsigned int) * 8);
		 unsigned int threequarters     = (unsigned int)((bitsinunignedint   * 3) / 4);
		 unsigned int oneeighth         = (unsigned int)(bitsinunignedint / 8);

		 unsigned int highbits          = (unsigned int)(0xffffffff) << (bitsinunignedint - oneeighth);
		 unsigned int hash              = 0;
		 unsigned int test              = 0;

		 while (*str){
			hash = (hash << oneeighth) + func(*str++);
			if ((test = hash & highbits) != 0)
			  hash = ((hash ^ (test >> threequarters)) & (~highbits));
		 }

		 return (hash & 0x7fffffff);
	}
	inline unsigned int pjw_hash(const char *str)
	{ return pjw_hash(str,default_char_conv); }

	template<typename function>
	unsigned int elf_hash(const char *str,function func)
	{
		 unsigned int hash = 0;
		 unsigned int x    = 0;

		 while (*str) {
			 hash = (hash << 4) + func(*str++);
			 if ((x = hash & 0xf0000000l) != 0) {
				 hash ^= (x >> 24);
				 hash &= ~x;
			 }
		 }

		 return (hash & 0x7fffffff);
	}
	inline unsigned int elf_hash(const char *str)
	{ return elf_hash(str,default_char_conv); }

	template<typename function>
	unsigned int bkdr_hash(const char *str,function func)
	{
		 unsigned int hash = 0;

		 while (*str)
		   hash = hash * 131 + func(*str++);

		 return (hash & 0x7fffffff);
	}
	inline unsigned int bkdr_hash(const char *str)
	{ return bkdr_hash(str,default_char_conv); }

	template<typename function>
	unsigned int sdbm_hash(const char *str,function func)
	{
		 unsigned int hash = 0;

		 while (*str)
			hash = func(*str++) + (hash << 6) + (hash << 16) - hash;

		 return (hash & 0x7fffffff);
	}
	inline unsigned int sdbm_hash(const char *str)
	{ return sdbm_hash(str,default_char_conv); }

	template<typename function>
	unsigned int djb_hash(const char *str,function func)
	{
		 unsigned int hash = 5381;

		 while (*str)
		   hash += (hash << 5) + func(*str++);

		 return (hash & 0x7fffffff);
	}
	inline unsigned int djb_hash(const char *str)
	{ return djb_hash(str,default_char_conv); }
	
	template<typename function>
	unsigned int ap_hash(const char *str,function func)
	{
		 unsigned int hash = 0;

		 for (int i=0; *str; i++){
			 if ((i & 1) == 0)
			   hash ^= ((hash << 7) ^ func(*str++) ^ (hash >> 3));
			 else
			   hash ^= (~((hash << 11) ^ func(*str++) ^ (hash >> 5)));
		 }

		 return (hash & 0x7fffffff);
	}
	inline unsigned int ap_hash(const char *str)
	{ return ap_hash(str,default_char_conv); }
	
	unsigned int crc_hash(const char *str);
}

#endif
