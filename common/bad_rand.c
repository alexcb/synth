#include "bad_rand.h"

uint32_t bad_rand_val = 0.f;
uint32_t bad_rand()
{
	bad_rand_val = bad_rand_val * 1103515245 + 12345;
	return bad_rand_val;
}

// returns a random unsigned int between 0 and n, normally centered around n/2
uint32_t bad_normal(uint32_t n)
{
	return (bad_rand() % n + bad_rand() % n + bad_rand() % n + bad_rand() % n + bad_rand() % n) / 5;
}

// returns a random float between -1 and 1, normally centered around 0
float bad_normalf()
{
	return bad_normal(2000) / 1000.f - 1.f;
}

// returns a random float between 0 and 1
float bad_randf()
{
	float f = (float)bad_rand() / 0xFFFFFFFF;
	if (f > 1.0f) {
		f = 1.0f;
	}
	return f;
}
