#ifndef _LOKI_TYPELIST_EX_H
#define _LOKI_TYPELIST_EX_H

#include "Typelist.h"

namespace Loki
{
	namespace TL
	{
		#define LOKI_TYPELIST_METHOD_DEF(name)\
		template <class TList>\
		struct name;\

		//////////////////////////////////////////////////////////////////////////
		// Get max size of all types in one type list
		#define LOKI_TYPELIST_SIZE_SPEC_DEF0(name)\
		template<>\
		struct name##Size<NullType>\
		{\
			enum { value = 0 };\
		};\

		#define LOKI_TYPELIST_SIZE_SPEC_DEF1(name)\
		template<class T>\
		struct name##Size<Typelist<T,NullType> >\
		{\
			enum { value = sizeof(T) };\
		};\

		#define LOKI_TYPELIST_SIZE_SPEC_DEF2(name,b)\
		template<class T,class U>\
		struct name##Size<Typelist<T,U> >\
		{\
			enum { tmp = name##Size<U>::value };\
			enum { value = (b ? sizeof(T) > tmp : sizeof(T) < tmp) ? sizeof(T) : tmp };\
		};\

		LOKI_TYPELIST_METHOD_DEF(MaxSize)
		LOKI_TYPELIST_METHOD_DEF(MinSize)
		LOKI_TYPELIST_SIZE_SPEC_DEF0(Max)
		LOKI_TYPELIST_SIZE_SPEC_DEF0(Min)
		LOKI_TYPELIST_SIZE_SPEC_DEF1(Max)
		LOKI_TYPELIST_SIZE_SPEC_DEF1(Min)
		LOKI_TYPELIST_SIZE_SPEC_DEF2(Max,true)
		LOKI_TYPELIST_SIZE_SPEC_DEF2(Min,false)

		#undef LOKI_TYPELIST_SIZE_SPEC_DEF0
		#undef LOKI_TYPELIST_SIZE_SPEC_DEF1
		#undef LOKI_TYPELIST_SIZE_SPEC_DEF2

		////////////////////////////////////////////////////////////////////////////////
		// Get the type that its size is max in one typelist
		#define LOKI_TYPELIST_TYPE_SPEC_DEF0(name)\
		template<>\
		struct name##Type<NullType>\
		{\
			typedef NullType Result;\
		};\

		#define LOKI_TYPELIST_TYPE_SPEC_DEF1(name)\
		template<class T>\
		struct name##Type<Typelist<T,NullType> >\
		{\
			typedef T Result;\
		};\

		#define LOKI_TYPELIST_TYPE_SPEC_DEF2(name,b)\
		template<class T,class U>\
		struct name##Type<Typelist<T,U> >\
		{\
			typedef typename name##Type<U>::Result R;\
			typedef typename Select< b ? (sizeof(T)>sizeof(R)) : (sizeof(T)<sizeof(R)),T,R>::Result Result;\
		};\

		LOKI_TYPELIST_METHOD_DEF(MaxType)
		LOKI_TYPELIST_METHOD_DEF(MinType)
		LOKI_TYPELIST_TYPE_SPEC_DEF0(Max)
		LOKI_TYPELIST_TYPE_SPEC_DEF0(Min)
		LOKI_TYPELIST_TYPE_SPEC_DEF1(Max)
		LOKI_TYPELIST_TYPE_SPEC_DEF1(Min)
		LOKI_TYPELIST_TYPE_SPEC_DEF2(Max,true)
		LOKI_TYPELIST_TYPE_SPEC_DEF2(Min,false)

		#undef LOKI_TYPELIST_TYPE_SPEC_DEF0
		#undef LOKI_TYPELIST_TYPE_SPEC_DEF1
		#undef LOKI_TYPELIST_TYPE_SPEC_DEF2

		//////////////////////////////////////////////////////////////////////////
		//add other some methods

		#undef LOKI_TYPELIST_METHOD_DEF
	}
}

#endif


