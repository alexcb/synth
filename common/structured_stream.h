#ifdef __cplusplus
extern "C" {
#endif

#ifndef __circle__
#include <stddef.h>
typedef unsigned char u8; // TODO is there an include to use instead?
#endif

int read_u8(char** data, size_t* data_len, u8* x);

int find_start_of_stream(char** data, size_t* data_len, const char* magic_stream_header);

#ifdef __cplusplus
}
#endif
