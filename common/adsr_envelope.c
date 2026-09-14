#include "adsr_envelope.h"

float ads_level(float t, float attack, float attack_start, float decay, float sustain)
{
	attack = MAX(attack, ATTACK_MIN);
	decay = MAX(decay, DECAY_MIN);
	if (t < attack) {
		return MAX(t / attack, attack_start);
	}
	t -= attack;
	if (t < decay) {
		return 1.0 - t / decay * (1.0 - sustain);
	}
	t -= decay;
	return sustain;
}

float r_level(float t, float orig_vol, float release)
{
	release = MAX(release, RELEASE_MIN);
	if (t > release) {
		return 0.f;
	}
	return orig_vol * (1.0 - t / release);
}
