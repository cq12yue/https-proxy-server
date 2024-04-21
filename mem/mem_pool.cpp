#include "mem_pool.h"
#include <pthread.h>
using namespace std;
using namespace memory;

static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static pthread_key_t s_key;
static int s_errno;

static void delete_key(void* ptr)
{
	smart_pool* pool = static_cast<smart_pool*>(ptr);
	delete pool;
}

static void create_key()
{
	s_errno = pthread_key_create(&s_key,delete_key);
}

//If fail,then throw std::bad_alloc exception
smart_pool* tls_mem_pool::get_cur_tls_pool()
{
	pthread_once(&key_once,create_key);
	if(s_errno) throw bad_alloc();

	smart_pool* pool = static_cast<smart_pool*>(pthread_getspecific(s_key));
	if(NULL==pool){
		pool = new smart_pool;
		if(pthread_setspecific(s_key,pool)) { 
			delete pool; 
			throw bad_alloc();
		}
	}
	return pool;
}
