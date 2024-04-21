#include "fifo_srv.h"
#include "../log/log_file.h"
#include "../base/cstring.h"
#include "../loki/ScopeGuard.h"
#include "select_reactor.h"
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <string>
using namespace std;

struct config
{
	bool daemon_;
	string log_file_;
	string fifo_path_;
	size_t rbsize_; 

	config()
	:daemon_(false)
	,rbsize_(1024*1024)
	{
	}
};

logfile* g_lf;

static void handle_signal(int sig,void *data)
{
	log_printf(g_lf,"catch signal %d\n",sig);
	static_cast<select_reactor*>(data)->stop();
}

static bool parse_cmdline(int argc,char* argv[],config &conf)
{
	struct option opts[] = {
		{"daemon",0,0,'d'},
		{"outfile",1,0,'o'},
		{"fifo",1,0,'f'},
		{"rbsize",1,0,'s'},
		{"help",0,0,'h'},
		{"version",0,0,'v'},
		{0,0,0,0}
	};

	int c,index,size;

	while(true){
		c = getopt_long(argc,argv,"do:f:s:hv",opts,&index);
		if(-1==c) break;

		switch(c){
			case 'd': 
				conf.daemon_ = true;
				break;

			case 'o':
				conf.log_file_ = optarg;
				break;

			case 'f':
				conf.fifo_path_ = optarg;
				break;
			
			case 's':
				if(!base::strtoul(optarg,(unsigned long&)conf.rbsize_)){
					fprintf(stderr,"the -s option argument invalid\n");
					return false;
				}
				break;

			case ':': 
				fprintf(stderr,"missing argument\n");
				return false;

			case '?': 
				fprintf(stderr,"unknown option\n");
				return false;

			case 'h': 
				printf("logsrv usage [option]...\n\n"
					"-d	run as a daemon\n"
					"-o <name> log file name"
					"-p <name> fifo path"
					"-s <number> file rollback size"
					"-h --help	display this help and exit\n"
					"-v --version	output version information and exit\n"
					);
				return false;

			case 'v': 
				printf("log server: version 1.0\n");
				return false;
		}
	}
	if(conf.log_file_.empty()){
		fprintf(stderr,"log filename must be not empty\n");
		return false;
	}

	if(conf.fifo_path_.empty()){
		fprintf(stderr,"fifo path must be not empty\n");
		return false;
	}

	return true;
}

int main(int argc,char* argv[])
{
	try{
		config conf;
		if(!parse_cmdline(argc,argv,conf))
			return 1;
		
		g_lf = log_open(conf.log_file_.c_str());
		if(NULL==g_lf){
			fprintf(stderr,"log open %s fail: errno=%d\n",conf.log_file_.c_str(),errno);
			return 1;
		}
	
		LOKI_ON_BLOCK_EXIT(log_close,g_lf);
	
		log_set_rbsize(g_lf,conf.rbsize_);
	
		if(conf.daemon_){
			sigignore(SIGHUP);
			if (-1 == daemon(1,0)){
				log_printf(g_lf,"daemon fail: errno=%d\n",errno);		
				return 1;
			}
		}

		select_reactor reactor;
		reactor.add_signal(SIGINT,handle_signal,&reactor);
		reactor.add_signal(SIGTERM,handle_signal,&reactor);
		
		fifo_acceptor acceptor(&reactor);
		acceptor.start(conf.fifo_path_.c_str());
	
		reactor.run();
	}catch(std::exception& se){
		log_printf(g_lf,"main std exception: %s\n",se.what());
		return 1;
	}
	
	return 0;
}
