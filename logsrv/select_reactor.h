#ifndef _SELECT_REACTOR_H
#define _SELECT_REACTOR_H

#include "event.h"
#include <sys/select.h>
#include <signal.h>
#include <vector>
#include <map>
#include <stddef.h>

class select_reactor;
typedef void (*reactor_sighandler_t)(int,void *data);

#ifdef _HAS_SIGACTION
	typedef struct sigaction iso_sighandler_t;
#else
	typedef sighandler_t iso_sighandler_t;	
#endif

struct signal_info
{
	signal_info(reactor_sighandler_t fun,void *data,iso_sighandler_t sh)
	:fun_(fun)
	,old_sh_(sh)
	,data_(data)
	{}
	
	void *data_;
	reactor_sighandler_t fun_;
	iso_sighandler_t old_sh_;	
};

class select_reactor
{
public:
	select_reactor();
	~select_reactor();
	
	bool add_signal(int sig,reactor_sighandler_t fun,void *data);
	bool del_signal(int sig);
	
	void add_event(const event& ev);
	void del_event(const event& ev);
	void run();
	
	void stop()
	{ is_stop_ = true;}
	
protected:
	int max_fd() const;
	static void handle_pipe_read(int fd,int flag,void* arg);
	static void handle_signal(int sig);
	
private:
	void handle_pipe_read();
	bool signal_init();
	
private:
	fd_set rset_,wset_;
	std::vector<event> ev_heap_;
	
	int fd_[2];
	event ev_signal_;
	bool is_add_evsignal_;
	std::map<int,signal_info> signals_;
	
	volatile bool is_stop_;
};

#endif
