#include <stdio.h>
#include <azure/core/az_span.h>

#include "string.h"

char* fk_dtoa( double d, int precision, char* buffer, size_t bufsize )
{
    // TODO: check if az_span-functions are used correctly
    az_span sbuf = az_span_create(( uint8_t* ) buffer, bufsize );
    az_span sout;

    int abs_precision = precision < 0 ? -precision : precision;

    if( az_span_dtoa( sbuf, d, abs_precision, &sout ) == AZ_OK ) {
        *sout._internal.ptr = 0;
    } else {
        *buffer = 0;
    }

    if( precision < 0 ) {
        // Ensure, that the exact number of digits is used.

        // Count current number of digits after dot
        size_t pos = 0;
        while( buffer[pos] && buffer[pos] != '.' )
            pos++;

        int digits = 0;
        if( !buffer[pos] ) {
            // no dot at all
            if( pos + 1 < bufsize )
                buffer[pos++] = '.';
        } else {
            // count number of digits after dot
            pos++;
            while( buffer[pos] ) {
                pos++;
                digits++;
            }
        }

        // add remaining zeros
        while( pos + 1 < bufsize && digits < abs_precision ) {
            buffer[pos++] = '0';
            digits++;
        }
        buffer[pos] = 0;
    }

    return buffer;
}
