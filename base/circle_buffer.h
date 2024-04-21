#ifndef _BASE_CIRCLE_BUFFER_H
#define _BASE_CIRCLE_BUFFER_H

#include <assert.h>
#include <memory>

namespace base
{
template<typename T,class Alloc = std::allocator<T> >
class circle_buffer 
{
public:
	explicit circle_buffer(size_t n,const T& val = T(),const Alloc& alloc=Alloc())
		:size_(n)
		,capacity_(n)
		,front_(0)
		,back_(n-1)
		,alloc_(alloc)
	{
		data_ = alloc_.allocate(n);
		std::uninitialized_fill_n(data_,n,val);
	}

    ~circle_buffer()
	{
		if(front_ <= back_){
			std::_Destroy(data_+front_,data_+back_+1);
		}else{
			std::_Destroy(data_,data_+back_+1);
			std::_Destroy(data_+front_,data_+capacity_);
		}
		alloc_.deallocate(data_,capacity_);
	}
	
    void push_back(const T& val)
	{
		if(full()){
			if(empty()) 
			   return;
			data_[front_] = val;
			back_ = front_;
			front_ = index(front_+1);
		}else{
			back_ = index(back_+1);
			data_[back_] = val;
			++size_;
		}
	}
    
	void pop_front()
	{
		assert(!empty());
		front_ = index(front_+1);
		--size_;
	}
	
	void resize(size_t n)
	{

	}

	const T& front() const
	{
		assert(!empty());
		return data_[front_];
	}

	T& front()
	{ 	
		assert(!empty());
		return data_[front_];
	}

	const T& back() const
	{
		assert(!empty());
		return data_[back_];
	}

	T& back()
	{ 	
		assert(!empty());
		return data_[back_];
	}

	void clear()
	{   	
		size_ = 0; front_ = 0; back_ = capacity_-1;
	}

	bool empty() const
	{ return 0==size_ ;	}
	
	bool full() const
	{ return size_==capacity_; }

     size_t size() const
	{ return size_;	}

protected:
	size_t index(size_t i) const
	{ 	return i < capacity_ ? i : 0; }

private:
	Alloc alloc_;
	T* data_;
	size_t front_,back_;
	size_t capacity_;
	size_t size_;
};
}

#endif