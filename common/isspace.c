#include "isspace.h"

int isspace(char c)
{
	if (c == ' ' || // Space (0x20)
	    c == '\t' || // Horizontal Tab (0x09)
	    c == '\n' || // Newline (0x0A)
	    c == '\v' || // Vertical Tab (0x0B)
	    c == '\f' || // Form Feed (0x0C)
	    c == '\r') { // Carriage Return (0x0D)
		return 1;
	}
	return 0;
}
