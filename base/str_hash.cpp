#include "str_hash.h"
#include <string.h>

/* crc hash function */
unsigned int base::crc_hash(const char *str)
{
    unsigned int        nleft   = strlen(str);
    unsigned long long  sum     = 0;
    unsigned short int *w       = (unsigned short int *)str;
    unsigned short int  answer  = 0;

    /*
     * our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while ( nleft > 1 ) {
        sum += *w++;
        nleft -= 2;
    }
    /*
     * mop up an odd byte, if necessary
     */
    if ( 1 == nleft ) {
        *( unsigned char * )( &answer ) = *( unsigned char * )w ;
        sum += answer;
    }
    /*
     * add back carry outs from top 16 bits to low 16 bits
     * add hi 16 to low 16
     */
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    /* add carry */
    sum += ( sum >> 16 );
    /* truncate to 16 bits */
    answer = ~sum;

    return (answer & 0xffffffff);
}
