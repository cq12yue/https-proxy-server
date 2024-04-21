#include "timeval.h"

static const long ONE_SECOND_IN_USECS = 1000000;

void base::timeval_normalize(timeval &t)
{
	if (t.tv_usec >= ONE_SECOND_IN_USECS){
		do {
			++t.tv_sec;
			t.tv_usec -= ONE_SECOND_IN_USECS;
		}while (t.tv_usec >= ONE_SECOND_IN_USECS);
	}else if (t.tv_usec <= -ONE_SECOND_IN_USECS) {
		do 	{
			--t.tv_sec;
			t.tv_usec += ONE_SECOND_IN_USECS;
		}while (t.tv_usec <= -ONE_SECOND_IN_USECS);
	}

	if (t.tv_sec >= 1 && t.tv_usec < 0)	{
		--t.tv_sec;
		t.tv_usec += ONE_SECOND_IN_USECS;
	}
}

