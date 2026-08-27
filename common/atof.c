#define true 1
#define false 0
#define bool int

#include <stddef.h>

float acb_strtof(const char* s, const char** remaining)
{
	float result = 0.0f;
	float factor = 1.0f;
	bool decimal = false;
	bool negative = false;
	bool valid = false;

	if (*s == '-') {
		negative = true;
		s++;
	}

	while (*s) {
		if (*s == '.') {
			decimal = true;
			s++;
			continue;
		}

		if (*s < '0' || *s > '9') {
			break;
		}

		if (decimal) {
			factor *= 0.1f;
			result += (*s - '0') * factor;
		} else {
			result = result * 10.0f + (*s - '0');
			valid = true;
		}
		s++;
	}

	if( remaining != NULL && valid ) {
		*remaining = s;
	}

	return negative ? -result : result;
}

float atof(const char* s)
{
	return acb_strtof(s, NULL);
}
