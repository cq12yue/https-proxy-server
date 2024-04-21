#include "io_ext.h"
#include <limits.h>
#include <sys/uio.h>
#include <assert.h>
#include <errno.h>

/*************************************************************************************************************
 	In the following four functions,the optional parameter tran indicate the number of bytes read or written,
 or -1 if an error occurred.On success,return true indicate all data had been read or written successfully,
 otherwise in other cases,return false indicate error or partial success.
 ************************************************************************************************************/
 
bool ipc::readn(int fd,void* buf,size_t cnt,ssize_t* tran/*=NULL*/)
{
	size_t left = cnt;
	ssize_t ret;
	char* ptr = (char*)buf;
	
	while(left > 0){
		ret = read(fd,ptr,left);
		if(ret > 0){
			left -= ret;
			ptr += ret;
		}else if(0==ret || left != cnt)
			break;
		else{
			if(tran) *tran = -1;
			return false;		
		}
	}
	if(tran) *tran = cnt-left;
	return 0==left;
}

bool ipc::writen(int fd,const void* buf,size_t cnt,ssize_t* tran/*=NULL*/)
{
	size_t left = cnt;
	ssize_t ret;
	char* ptr = (char*)buf;

	while(left > 0){
		ret = write(fd,ptr,left);
		if(ret > 0){
			left -= ret;
			ptr += ret;
		}else if(0==ret || left != cnt)
			break;
		else
			if(tran) *tran = -1;
			return false;		
	}
	if(tran) *tran = cnt-left;
	return 0==left;
}

static int get_iov_tran_index(const struct iovec* iov,int iovcnt,size_t trans,size_t& tran)
{
	size_t cnt = 0;  int i;

	for(i=0;i < iovcnt;++i){
		cnt += iov[i].iov_len;
		if(trans < cnt){
			tran = iov[i].iov_len - (cnt - trans);
			break;
		}
	}
	return i;
}

bool ipc::readvn(int fd,const struct iovec* iov,int iovcnt,ssize_t* tran/*=NULL*/)
{
	if(iovcnt > IOV_MAX){
		if(tran) *tran = -1;
		errno = EINVAL;	
		return false;
	}
	size_t all_cnt = 0,all_tran = 0,one_tran;
	ssize_t ret;

	struct iovec _iov[IOV_MAX];
	int i;
	for(i=0;i < iovcnt;++i){ 
		_iov[i] = iov[i];	
		all_cnt += iov[i].iov_len;
	}

	i = 0;
	do{
		ret = readv(fd,&_iov[i],iovcnt-i);
		if(ret > 0){
			all_tran += ret;	
			if(all_tran==all_cnt)
				break;

			i = get_iov_tran_index(iov,iovcnt,all_tran,one_tran);
			assert(i < iovcnt);
			_iov[i].iov_base = iov[i].iov_base + one_tran;				
			_iov[i].iov_len  = iov[i].iov_len - one_tran;

		}else if(0==ret)
			break;
		else{
			if(tran) *tran = -1;
			return false;
		}
	}while(all_tran < all_cnt);

	if(tran) *tran = all_tran;
	return all_tran==all_cnt;
}


bool ipc::writevn(int fd,const struct iovec* iov,int iovcnt,ssize_t* tran/*=NULL*/)
{
	if(iovcnt > IOV_MAX){
		if(tran) *tran = -1;
		errno = EINVAL;	
		return false;
	}
	size_t all_cnt = 0,all_tran = 0,one_tran;
	ssize_t ret;

	struct iovec _iov[IOV_MAX];
	int i;
	for(i=0;i < iovcnt;++i){ 
		_iov[i] = iov[i];	
		all_cnt += iov[i].iov_len;
	}

	i = 0;
	do{
		ret = writev(fd,&_iov[i],iovcnt-i);
		if(ret > 0){
			all_tran += ret;	
			if(all_tran==all_cnt)
				break;

			i = get_iov_tran_index(iov,iovcnt,all_tran,one_tran);
			assert(i < iovcnt);
			_iov[i].iov_base = iov[i].iov_base + one_tran;				
			_iov[i].iov_len  = iov[i].iov_len - one_tran;

		}else if(0==ret)
			break;
		else{
			if(tran) *tran = -1;
			return false;
		}
	}while(all_tran < all_cnt);

	if(tran) *tran = all_tran;
	return all_tran==all_cnt;
}
