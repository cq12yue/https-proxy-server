#include "ini_file.h"
#include <utility>
#include <iostream>
using namespace std;
using namespace base;

bool ini_file::load(const string& filename)
{
	fstream fs(filename.c_str(),ios::in|ios::binary);
	if (!fs.is_open())
		return false;
		
	bool ret = parsecontent(fs);
	fs.close();
	if (ret)
		filename_ = filename;
		
	return ret;
}

bool ini_file::save()
{
	return save(filename_);	
}

bool ini_file::save(const string& filename)
{
	fstream fs(filename.c_str(),ios::out|ios::trunc);
	if (!fs.is_open())
		return false;
		
	for (sec_iter sit = datas_.begin();sit!=datas_.end();++sit){
		fs << "[" << sit->first << ']' << endl;
		for(key_iter kit = sit->second.begin(); kit!=sit->second.end();++kit)
			fs << kit->first << "=" << kit->second << endl;
		fs << endl;
	}
	fs.close();
	return true;
}

void ini_file::print()
{
	for (sec_iter sit = datas_.begin();sit != datas_.end();++sit){
		std::cout << "[" << sit->first.c_str()<<"]";
		for(key_iter kit = sit->second.begin(); kit != sit->second.end();++kit)
		    cout << kit->first << "=" << kit->second;
		cout << endl;
	}
}

bool ini_file::eof(fstream& fs) const
{
	size_t cur = fs.tellg();
	fs.seekg(0,ios::end);
	size_t end = fs.tellg();
	fs.seekg(cur);
	return cur == end;
}

void ini_file::getline(fstream& fs,string& str)
{
	str.clear();
	for (char ch;fs.get(ch);){
		if (ch == '\r') continue;

		if (ch != '\n') str += ch;
		else break;
	}
}

bool ini_file::parsecontent(fstream& fs)
{
	string line, cursec, curkey, curvalue;

	for(size_t pos;!eof(fs);){
		getline(fs,line);
		if (!line.empty()){
			trim(line);
			char tag = line[0];
			if (tag == '#' || tag == ';') 
				continue;
			if (tag == '['){
				if (*line.rbegin()!=']')
					return false;
				cursec = line.substr(1,line.length()-2);
				if (cursec.empty()) 
					return false;
				trim(cursec);
				datas_.insert(make_pair(cursec,key_value()));
			}else{
				pos = line.find('=');
				if (pos == string::npos)
					return false;	
				curkey = line.substr(0,pos);
				if (curkey.empty())
					return false;
				curvalue = line.substr(pos+1);
				sec_iter iter = datas_.find(cursec);
				if (iter == datas_.end())
					return false;
				trim(curkey);
				trim(curvalue);
				(iter->second).insert(make_pair(curkey,curvalue));
			}
		}
	}
	return true;
}

void ini_file::set_value_string(const string& section,const string& key,const string& val)
{
	string s = trim_copy(section),k = trim_copy(key),v = trim_copy(val);

	sec_iter sit = datas_.find(s);
	if (sit == datas_.end()){
		key_value kv;
		kv.insert(make_pair(k,v));
		datas_.insert(make_pair(s,kv));
	}else{
		key_value& kv = sit->second;
		key_iter kit = kv.find(k);
		if (kit == kv.end())
			kv.insert(make_pair(k,v));
		else
			kit->second = v;
	}
}

bool ini_file::get_value_string(const string& section,const string& key,string& val, const string& default_val /*=""*/) const
{
	string s = trim_copy(section),k = trim_copy(key),d = trim_copy(default_val);

	sec_citer sit = datas_.find(s);
	if (sit == datas_.end()){
		val = d;		
		return false;
	}
		
	const key_value& kv = sit->second;
	key_citer kit = kv.find(k);
	if (kit == kv.end()){
		val = d;	
		return false;
	}

	val = kit->second;	
	return true;
}

void ini_file::del_sec(const string& section)
{
	string s = trim_copy(section);
	sec_iter it = datas_.find(s);
	if (it != datas_.end())
		datas_.erase(it);
}

void ini_file::del_key(const string& section,const string& key)
{
	string s = trim_copy(section),k = trim_copy(key);
		
	sec_iter sit = datas_.find(s);
	if (sit != datas_.end()){
		key_iter kit = sit->second.find(k);
		if (kit != sit->second.end())
			sit->second.erase(kit);
	}
}