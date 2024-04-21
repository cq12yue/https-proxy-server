#ifndef _BASE_INI_FILE_H
#define _BASE_INI_FILE_H

#include "cstring.h"
#include <fstream>
#include <map>

namespace base 
{
class ini_file
{
public:
	bool load(const std::string& filename);
	bool save();
	bool save(const std::string& filename);
	void print();

public:
	void del_sec(const std::string& section);
	void del_key(const std::string& section,const std::string& key);
	
	void set_value_string(const std::string& section,const std::string& key,const std::string &val);
	bool get_value_string(const std::string& section,const std::string& key,std::string &val,const std::string& default_val = "") const;	

	template<typename T>
	void set_value_number(const std::string &section,const std::string &key,T val)
	{
		std::string str = base::to_string<char>(val,std::dec);
		set_value_string(section,key,str);
	}

	template<typename T>
	bool get_value_number(const std::string &section,const std::string &key,T &val,T default_val=(T)0) const
	{
		std::string str;
		if(!get_value_string(section,key,str)){
			val = default_val;
			return false;
		}
		if(!base::from_string(val,str,std::dec)){
			val = default_val;
			return false;
		}
		return true;
	}

protected:
	bool eof(std::fstream& fs) const;
	void getline(std::fstream& fs,std::string& str);
	bool parsecontent(std::fstream& fs);

private:
	std::string  filename_;
	typedef std::map<std::string,std::string> key_value;
	typedef key_value::iterator key_iter;
	typedef key_value::const_iterator key_citer;
	typedef std::map<std::string,key_value>::iterator sec_iter;
	typedef std::map<std::string,key_value>::const_iterator sec_citer;
	std::map<std::string, key_value> datas_;
};
}

#endif
