#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#define ATTACK_MIN 0.01
#define DECAY_MIN 0.01
#define RELEASE_MIN 0.01

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

float ads_level(float t, float attack, float attack_start, float decay, float sustain);
float r_level(float t, float orig_vol, float release);

inline float clamp_output(float f)
{
	if (f > 1.0f) {
		return 1.0f;
	}
	if (f < -1.0f) {
		return -1.0f;
	}
	return f;
}

#ifdef __cplusplus
}
#endif
