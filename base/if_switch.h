#ifndef _BASE_IF_SWITCH_H
#define _BASE_IF_SWITCH_H

namespace base
{
	//////////////////////////////////////////////////////////////////////////////////////////
	//if-then-else
	template<bool b,typename T,typename U>
	struct if_then_else
	{
		typedef T type;
	};
	template<typename T,typename U>
	struct if_then_else<false,T,U>
	{
		typedef U type;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//if-elif-else
	template<typename T> struct if_true;
	struct if_false;

	template<bool b>
	struct if_
	{
		template<typename T>
		struct then : if_then_else<b,if_true<T>,if_false>::type
		{
		};
	};

	template<typename T>
	struct if_true
	{
		template<bool b>
		struct elif_
		{
			template<typename U>
			struct then : if_true<T>
			{
			};
		};
		template<typename U>
		struct else_
		{
			typedef T type;
		};
	};

	struct if_false
	{
		template<bool b>
		struct elif_ : if_<b>
		{
		};
		template<typename T>
		struct else_
		{
			typedef T type;
		};
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//switch case
	template<typename T> struct case_true;
	template<int N> struct case_false;

	template<int M>
	struct switch_
	{
		template<int N>
		struct case_
		{
			template<typename T>
			struct then : if_then_else<M==N,case_true<T>,case_false<M> >::type
			{
			};
		};
	};

	template<typename T>
	struct case_true
	{
		template<int N>
		struct case_
		{
			template<typename U>
			struct then : case_true<T>
			{
			};
		};
		template<typename V>
		struct default_
		{
			typedef T type;
		};
	};

	template<int M>
	struct case_false : switch_<M>
	{
		template<typename U>
		struct default_
		{
			typedef U type;
		};
	};
}

#endif