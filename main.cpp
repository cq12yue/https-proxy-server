#include "server.h"
#include "global.h"
#include "ssl.h"
#include "../log/log_send.h"
#include "../loki/ScopeGuard.h"
#include "../base/util.h"
#include "../base/ini_file.h"
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
using namespace base;

static void usage()
{
	printf("MiST Proxy Usage [option]...\n\n"
		"-d run as a daemon\n"
		"-f known fifo path\n"
		"-4 --ipv4	only for IPv4\n"
		"-6 --ipv6	only for IPv6\n"
		"-h --help	display this help and exit\n"
		"-v --version	output version information and exit\n"
		);
}

static bool parse_cmdline(int argc,char **argv,bool &is_daemon,bool &is_ipv4, string_t &fifo_path)
{
	struct option opts[] = {
		{"daemon",0,0,'d'},
		{"fifo",1,0,'f'},
		{"ipv4",0,0,'4'},
		{"ipv6",0,0,'6'},		
		{"help",0,0,'h'},
		{"version",0,0,'v'},
		{0,0,0,0}
	};

	int c,index,size, ipver = 0;
	is_daemon = false;
	is_ipv4 = true;

	while(true){
		c = getopt_long(argc,argv,"d46hvf:",opts,&index);
		if(-1==c) break;

		switch(c){
			case 'd': 
				is_daemon = true; 
				break;

			case 'f':
				fifo_path = optarg;
				break;

			case '4': 
			case '6':
				if(ipver){
					fprintf(stderr,"only use one kind of ip protocol,either ipv4 or ipv6\n");
					return false;
				}else{
					ipver = ('4'==c ? 1 : 2);
					is_ipv4 = ('4'==c ? true: false);
				}
				break;

			case '?': 
				return false;

			case 'h': 
				usage(); 
				return false;

			case 'v': 
				printf("MiST Proxy: version 2.0\n");
				return false;
		}
	}
	if(fifo_path.empty()){
		fprintf(stderr,"the known fifo path must be not empty\n");
		return false;
	}
	return true;
}

void handle_config_changed(config *conf,uint32_t mask,void *arg)
{
}

static bool get_file_data(const char *filename,string_t &data)
{
	int fd = open(filename,O_RDONLY,0);
	if(-1==fd){
		log_error("open file %s fail: errno=%d",filename,errno);
		return false;
	}

	char buf[1024];
	for(ssize_t ret;;){
		ret = read(fd,buf,sizeof(buf));
		if(ret<=0) break;
		data.append(buf,ret);
	}
	close(fd);

	return true;
}

static bool configure()
{
	if(!base::get_exe_dir(g_conf_filename)){
		log_fatal("get_exe_dir fail");
		return false;
	}
	if(-1==chdir(g_conf_filename.c_str())){
		log_fatal("chdir %s: errno=%d",g_conf_filename.c_str(),errno);
		return false;
	}
	g_conf_filename.append("/server.cfg");

	ini_file ini;
	if(!ini.load(g_conf_filename.c_str())){
		log_error("load server configuration file fail");
		return false;
	}

	if(!g_conf.set_all(ini,false))
		return false;
	g_conf.print();

	return true;
}

static void reconfigure()
{
	ini_file ini;
	if(!ini.load(g_conf_filename)){
		log_error("reload server configuration file fail");
		return;
	}
	//only handle the follow six configuration variables currently
	uint32_t mask = config::MASK_BIT_PUB_ADDR|config::MASK_BIT_TIMEOUT_IDLE|config::MASK_BIT_TIMEOUT_RESPOND
		|config::MASK_BIT_INTERVAL_REPORT|config::MASK_BIT_LIMIT_FLOW|config::MASK_BIT_LIMIT_RATE;
	g_conf.set(ini,mask,true);
	g_conf.print();
}

static void handle_signal(int sig)
{
	log_debug("handle signal %d",sig);

	switch(sig){
		case SIGTERM: 
		case SIGINT:
			server::instance()->stop(); 
			break;

		case SIGHUP: 
			reconfigure();
			break;
	}
}

static bool init_signals()
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGPIPE);
	if(sigprocmask(SIG_BLOCK,&mask,NULL)){
		log_fatal("sigprocmask fail");	
		return false;
	}

	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sigfillset(&sa.sa_mask);	

	if(-1==sigaction(SIGINT,&sa,NULL)){
		log_fatal("sigaction SIGINT fail");	
		return false;
	}
	
	if(-1==sigaction(SIGTERM,&sa,NULL)){
		log_fatal("sigaction SIGTERM fail");	
		return false;
	}

	sa.sa_flags = SA_RESTART;
	if(-1==sigaction(SIGHUP,&sa,NULL)){
		log_fatal("sigaction SIGHUP fail");	
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char** argv)
{
	bool is_loginit = false;

	try{
		bool is_daemon, is_ipv4;
		string_t fifo_path;

		if (!parse_cmdline(argc,argv,is_daemon,is_ipv4,fifo_path))
			return 1;

		if(is_daemon && -1==daemon(1,0)){
			fprintf(stderr,"main daemon fail: errno=%d",errno);		
			return 1;
		}

		if(!(is_loginit=log_init(fifo_path.c_str()))){
			fprintf(stderr,"log init %s fail",fifo_path.c_str());	
			return 1;
		}
#if USE_SSL
		if(!ssl_init()){
			log_error("ssl_init fail");
			return 1;
		}
		if(!ssl_thread_setup()){
			log_fatal("ssl_thread_setup fail");
			return 1;
		}
		LOKI_ON_BLOCK_EXIT(ssl_thread_cleanup);
#endif
		if(!configure())
			return 1;

		if(!get_file_data(g_conf.cross_domain_file_.c_str(),g_cross_domain_data))
			return 1;

		if(!init_signals())
			return 1;

		if(!server::instance()->init(is_ipv4))
			return 1;
		server::instance()->run();

	}catch(std::exception &se){
		if(is_loginit)
			log_fatal("main %s",se.what());
		else
			fprintf(stderr,"main %s\n",se.what());
	}catch(...){
		if(is_loginit)
			log_fatal("main unknown exception");
		else
			fprintf(stderr,"main unknown exception\n");
	}
	/*
	 * call pthread_exit in order to create the opportunity to delete main thread's fifo,
     * otherwise main thread's fifo will not be deleted. it should not be called inside exception handle,
	 * e.g. try{} catch{}.
	 */
	pthread_exit(NULL); 

	return 1;
}
