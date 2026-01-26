#ifdef __cplusplus
extern "C" {
#endif

#ifdef __circle__
#define uint32_t unsigned
#else
#include <stdint.h>
#endif

uint32_t bad_rand();
uint32_t bad_normal(uint32_t n);

// returns a random float between -1 and 1, normally centered around 0
float bad_normalf();

#ifdef __cplusplus
}
#endif
