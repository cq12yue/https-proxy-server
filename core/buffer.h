#ifndef _CORE_BUFFER_H
#define _CORE_BUFFER_H

#include "../mem/mem_pool.h"
#include "../mem/new_policy.h"
#include "../base/if_switch.h"
#include "../base/null_type.h"
#include <cassert>

struct buffer
{
	buffer(size_t buf_size = 1);
	buffer(const void* data,size_t len);

	static void* operator new(size_t size,size_t buf_size = 1);

	static void operator delete(void* ptr);

	///< the size_ member specifies the number of initial memory buffer bytes
	size_t dsize_; 

	/**
	 *	for write,the dlen_ member specifies the number of data to be written total bytes;
	 *  for read,specifies the number of data to be read total bytes.
	 */
	size_t dlen_;

	/**
	 *	for write,the dtran_ member specifies the number of data written total bytes;
	 *  for read,specifies the number of data read total bytes.
	 */
	size_t dtran_; 

	///< the next_ member specifies next buffer
	buffer* next_;

	///< the data_ member specifies initial data
	char data_[1]; 
};

/**
 * @class buffer_queue
 * @brief can be used to recv and send data by FIFO asynchronously
 * 
 * because construct and destroy of this class object with new and delete only can be used for single 
 * thread,so it inherit from class tls_spec_new_policy of namespace memory
 */
template<class LockT>
class buffer_queue : public base::if_then_else<_USE_MEM_POOL,memory::tls_spec_new_policy<buffer_queue<LockT> >,
												base::null_type>::type
{
public:
	enum { default_low_mark=16*1024,default_high_mark=default_low_mark*4 };

public:
	buffer_queue()
		:head_(NULL)
		,tail_(NULL)
		,dsize_(0)
		,dlow_(default_low_mark)
		,dhigh_(default_high_mark)
	{ 
	}

	~buffer_queue()
	{  clear(); }

	void push(buffer* buf) 
	{
		assert(buf);
		base::lock_guard<LockT> guard(lock_);

		if(tail_)
			tail_->next_ = buf;
		else
			head_ = buf;
		tail_ = buf;

		buffer* tmp = buf;
		while(tmp && tmp->next_)
			tmp = tmp->next_;
		////here,tmp must not be null
		tail_ = tmp;

		dsize_ += buf->dlen_;
	}
	
	void push(buffer_queue<LockT>& bufq) 
	{
		buffer* buf;
		while(!bufq.empty()){
			buf = bufq.pop();
			push(buf);
		}
	}

	buffer* pop(bool reduce=true)
	{
		base::lock_guard<LockT> guard(lock_);
	
		buffer* buf = head_;
		if(head_) {
			head_ = head_->next_;
			if(NULL==head_) tail_ = NULL;
		}
		if(buf) { 
			buf->next_ = NULL;
			if(reduce) dsize_ -= buf->dlen_;
		}
		return buf;
	}

	void clear()
	{
		base::lock_guard<LockT> guard(lock_);

		buffer *cur,*next;
		for (cur = head_; cur; cur = next){
			next = cur->next_;
			delete cur;
		}
		head_ = tail_ = NULL;
		dsize_ = 0;
	}
	
	buffer* top() const
	{
		base::lock_guard<LockT> guard(lock_); 
		return head_; 
	}

	bool empty() const
	{ 
		base::lock_guard<LockT> guard(lock_);
		return NULL==head_;
	}

	void consume_size(size_t size)
	{	
		base::lock_guard<LockT> guard(lock_);
		dsize_ -= size;
	}

	size_t size() const 
	{ 
		base::lock_guard<LockT> guard(lock_);
		return dsize_; 
	}

	bool is_low() const
	{
		base::lock_guard<LockT> guard(lock_);
		return dsize_ <= dlow_;
	}

	bool is_high() const
	{
		base::lock_guard<LockT> guard(lock_);
		return dsize_ >= dhigh_;
	}

private:
	mutable LockT lock_;
	buffer* head_;
	buffer* tail_;
	size_t dsize_;
	size_t dlow_,dhigh_;
};

typedef buffer_queue<base::null_mutex> st_buffer_queue;
typedef buffer_queue<base::mutex>      mt_buffer_queue;

#endif
