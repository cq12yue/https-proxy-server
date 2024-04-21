#include "select_reactor.h"
#include <algorithm>
#include <stdexcept>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;

struct is_same_fd
{
	explicit is_same_fd(int fd)
	:fd_(fd)
	{	}
	
	bool operator()(const event& ev)
	{ return ev.fd_==fd_; }

private:
	int fd_;	
};

struct event_comp
{
	bool operator()(const event& left,const event& right) const
	{ return left.fd_ < right.fd_; }	
};

static int s_pipe_wfd = -1;

select_reactor::select_reactor()
:is_add_evsignal_(false)
,is_stop_(false)
{
	FD_ZERO(&rset_);
	FD_ZERO(&wset_);
//	make_heap(ev_heap_.begin(),ev_heap_.end(),event_comp());
	
	if(!signal_init())
		throw runtime_error("select_reactor signal_init fail");
}

select_reactor::~select_reactor()
{
	
}
	
bool select_reactor::add_signal(int sig,reactor_sighandler_t fun,void *data)
{
	map<int,signal_info>::iterator it = signals_.lower_bound(sig);
		
	if(it==signals_.end() || signals_.key_comp()(sig,it->first)){
#ifdef _HAS_SIGACTION	
		struct sigaction sa,old_sh;
		sa.sa_handler = handle_signal;
		sa.sa_flags = SA_RESTART;
		sigfillset(&sa.sa_mask);	
		if(-1==sigaction(sig,&sa,&old_sh))
			return false;
#else
		sighandler_t old_sh;
		if(SIG_ERR==(old_sh = signal(sig,handle_signal)))
			return false;
#endif
		if(!is_add_evsignal_){
			add_event(ev_signal_);
			is_add_evsignal_ = true;
			s_pipe_wfd = fd_[1];
		}
		typedef typename map<int,signal_info>::value_type value_type;
		signals_.insert(it,value_type(sig,signal_info(fun,data,old_sh)));
	}else{
		it->second.data_ = data;
		it->second.fun_  = fun;
	}
		
	return true;
}

bool select_reactor::del_signal(int sig)
{
	map<int,signal_info>::iterator it = signals_.find(sig);
	if(it == signals_.end())
		return false;

#ifdef _HAS_SIGACTION
	if(-1==sigaction(sig,&it->second.old_sh_,NULL))
		return false;
#else
	if(SIG_ERR==signal(sig,it->second.old_sh_))
		return false;
#endif

	signals_.erase(it);
	return true;
}
	
void select_reactor::add_event(const event& ev)
{
	if(ev.flag_&io_read)
		FD_SET(ev.fd_,&rset_);
	if(ev.flag_&io_write)
		FD_SET(ev.fd_,&wset_);
			
	ev_heap_.push_back(ev);
	push_heap(ev_heap_.begin(),ev_heap_.end(),event_comp());
}

void select_reactor::del_event(const event& ev)
{
	vector<event>::iterator it = find_if(ev_heap_.begin(),ev_heap_.end(),is_same_fd(ev.fd_));
	if(it==ev_heap_.end()) return;
		
	if(ev.flag_&io_read)
		FD_CLR(ev.fd_,&rset_);
	if(ev.flag_&io_write)
		FD_CLR(ev.fd_,&wset_);	
		
	ev_heap_.erase(it);
	make_heap(ev_heap_.begin(),ev_heap_.end(),event_comp());
}

void select_reactor::run()
{
	fd_set rset,wset;
	size_t num,i;
	int flag;
	
	for(;!is_stop_;){
		rset = rset_, wset = wset_;
		if(select(max_fd()+1,&rset,&wset,NULL,NULL)>0){
			num = ev_heap_.size();
			for(i=0; i<num; ++i){
				const event& ev = ev_heap_[i];
				flag = 0;
				if(FD_ISSET(ev.fd_,&rset))
					flag |= io_read;
				if(FD_ISSET(ev.fd_,&wset))
					flag |= io_write;
				if(flag && ev.fun_)
					ev.fun_(ev.fd_,flag,ev.arg_); 

				if(is_stop_) break;
			}	
		}else	
			if(errno!=EINTR) break;
	}
}

bool select_reactor::signal_init()
{
	if(pipe(fd_)) return false;
		
	int opts = fcntl(fd_[0],F_GETFL);
	if (opts < 0)
		return false;
	opts |= O_NONBLOCK;
	if(-1 == fcntl(fd_[0],F_SETFL,opts))
		return false;
	
	ev_signal_.set(fd_[0],io_read,handle_pipe_read,this);
	return true;
}
	
inline int select_reactor::max_fd() const
{
	return !ev_heap_.empty() ? ev_heap_[0].fd_ : -1;
}

void select_reactor::handle_pipe_read()
{
	char buf[1024];
	ssize_t ret;	
	unsigned int ncaught[NSIG] = {0},ncall,i;

	for(;;){
		ret = read(fd_[0],buf,sizeof(buf));
		if(ret > 0){
			int sig;
			for(i=0;i<ret;++i){
				sig = buf[i];
				if(sig < NSIG)
					++ncaught[sig];
			}
		}else 
			break;	
	}
	for(map<int,signal_info>::iterator it = signals_.begin();it != signals_.end();++it){
		ncall = ncaught[it->first];
		for(i=0;i<ncall;++i){
			signal_info &si = it->second;		
			si.fun_(it->first,si.data_);
		}
	}			
}
	
void select_reactor::handle_pipe_read(int fd,int flag,void* arg)
{
	static_cast<select_reactor*>(arg)->handle_pipe_read();
}

void select_reactor::handle_signal(int sig)
{
#ifndef _HAS_SIGACTION
	signal(sig,handle_signal);
#endif	
	write(s_pipe_wfd,(char*)&sig,1);
}
