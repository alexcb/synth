#define true 1
#define false 0
#define bool int

float atof(const char* s)
{
	float result = 0.0f;
	float factor = 1.0f;
	bool decimal = false;
	bool negative = false;

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
		}
		s++;
	}

	return negative ? -result : result;
}
