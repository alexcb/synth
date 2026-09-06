#include "structured_stream.h"

#ifdef __circle__
#include "atof.h"
#include <circle/util.h>
#define uint32_t unsigned
#define NULL 0
#else
#include <stdlib.h>
#include <string.h>
#endif

int read_u8(char** data, size_t* data_len, u8* x)
{
	if (*data_len >= sizeof(u8)) {
		*x = (u8) * *data;
		*data += sizeof(u8);
		*data_len -= sizeof(u8);
		return 0;
	}
	return 1;
}

int find_start_of_stream(char** data, size_t* data_len, const char* magic_stream_header)
{
	// TODO
	return 0;
}
