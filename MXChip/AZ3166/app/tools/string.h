#ifndef __TOOLS_STRING_H__
#define __TOOLS_STRING_H__

#include <stddef.h>

// convert double to string
//
// d: the value
// precision: precision to be used, may be less (trailing 0 will be removed).
//            If precision is negative, trailing zeroes will be added if neccessary.
// buffer: destination buffer and its size
//
// returns: buffer
//
// TODO: does not yet handle special values (NaN, ...)
char* fk_dtoa( double d, int precision, char* buffer, size_t bufsize );


#endif
