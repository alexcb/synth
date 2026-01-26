#include "synth.h"
#include "bad_rand.h"
#include "sine_table.h"

void foo(char* p)
{
	*p = 'f';
}

#ifdef __circle__
#include "atof.h"
#include <circle/util.h>

// these are in util.h
// int strcmp (const char *pString1, const char *pString2);
// int strcasecmp (const char *pString1, const char *pString2);
// int strncmp (const char *pString1, const char *pString2, size_t nMaxLen);
// int strncasecmp (const char *pString1, const char *pString2, size_t nMaxLen);
#define uint32_t unsigned
#define NULL 0
#else
#include <stdlib.h>
#include <string.h>
#endif

#include <math.h>

int parse_wave_type(const char* s)
{
	if (strcasecmp(s, "none") == 0) {
		return WAVE_TYPE_NONE;
	}
	if (strcasecmp(s, "sine") == 0) {
		return WAVE_TYPE_SINE;
	}
	if (strcasecmp(s, "triangle") == 0) {
		return WAVE_TYPE_TRIANGLE;
	}
	if (strcasecmp(s, "saw_up") == 0) {
		return WAVE_TYPE_SAW_UP;
	}
	if (strcasecmp(s, "saw_down") == 0) {
		return WAVE_TYPE_SAW_DOWN;
	}
	if (strcasecmp(s, "square") == 0) {
		return WAVE_TYPE_SQUARE;
	}
	if (strcasecmp(s, "pulse12") == 0) {
		return WAVE_TYPE_PULSE12;
	}
	if (strcasecmp(s, "pulse25") == 0) {
		return WAVE_TYPE_PULSE25;
	}
	if (strcasecmp(s, "random") == 0) {
		return WAVE_TYPE_RAND;
	}
	// fprintf(stderr, "unknown wave type: %s\n", s);
	return 0;
}

int parse_osc(const char* s, int* osc_type, int* n)
{
	if (strlen(s) < 4) {
		return 1;
	}
	if (strncmp(s, "vfo", 3) == 0) {
		*osc_type = OSC_TYPE_VFO;
	} else if (strncmp(s, "lfo", 3) == 0) {
		*osc_type = OSC_TYPE_LFO;
	} else {
		return 1;
	}
	s += 3;
	*n = atoi(s);
	if (*n == 0) {
		return 1;
	}
	return 0;
}

float ads_level(float t, float attack, float attack_start, float decay, float sustain)
{
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
	if (t > release) {
		return 0.f;
	}
	return orig_vol * (1.0 - t / release);
}

int osc_num_to_index(int osc_num, int osc_type)
{
	int i = (osc_num - 1);
	if (osc_type == OSC_TYPE_LFO) {
		i += NUM_OSCS;
	}
	return i;
}

int load_patch(char* src, struct osc* oscs)
{
	// FILE* fp;
	// fp = fopen(path, "r");
	// if (fp == NULL) {
	//	return 1;
	// }

	int n, m;

	int osc_num;
	int osc_type;

	int first_call = 1;
	char* save_ptr = NULL;

	struct osc* osc = NULL;
	for (;;) {
		char* buffer = strtok_r(first_call ? src : 0, " \n", &save_ptr);
		first_call = 0;
		if (buffer == NULL) {
			break;
		}
		n = strlen(buffer);
		if (buffer[0] == '#') {
			continue;
		}
		for (int j = n - 1; j >= 0; j--) {
			if (buffer[j] == '\n') {
				buffer[j] = '\0';
				n--;
			}
		}
		if (buffer[0] == '[' && buffer[n - 1] == ']') {
			buffer[n - 1] = '\0';
			char* s = &buffer[1];

			if (parse_osc(s, &osc_type, &osc_num) != 0) {
				// fprintf(stderr, "err: failed to parse %s\n", s);
				return 1;
			}

			if (osc_num < 1 || osc_num > NUM_OSCS) {
				// fprintf(stderr, "err: expected osc number in range 1-10, got %d\n", osc_num);
				return 1;
			}
			osc = &oscs[osc_num_to_index(osc_num, osc_type)];
			osc->osc_type = osc_type;

			// init defaults
			osc->freq_m = 1.0;
			osc->attack = ATTACK_MIN;
			osc->sustain = 1.0;
			osc->decay = DECAY_MIN;
			osc->phase_input_m = 1.0;
			osc->amp_input_m = 1.0;
			if (osc->osc_type == OSC_TYPE_VFO) {
				osc->freq_m = 1.0;
				osc->output_volume_m = 1.0;
				osc->osc_type = WAVE_TYPE_SINE;
			} else if (osc->osc_type == OSC_TYPE_LFO) {
				osc->freq = 1.0;
			}

			continue;
		}
		if (osc == NULL) {
			continue;
		}
		char* v = strstr(buffer, "=");
		if (v == NULL) {
			continue;
		}
		m = v - buffer - 1;
		*v = '\0';
		v++;
		if (strcmp(buffer, "type") == 0) {
			osc->wave_type = parse_wave_type(v);
		} else if (strcmp(buffer, "freq") == 0) {
			if (strcmp(v, "sync") == 0) {
				osc->freq_sync = true;
			} else {
				osc->freq = atof(v);
			}
		} else if (strcmp(buffer, "freq_m") == 0) {
			osc->freq_m = atof(v);
		} else if (strcmp(buffer, "detune") == 0) {
			osc->detune = atof(v);
		} else if (strcmp(buffer, "output") == 0) {
			osc->output_volume_m = atof(v);
		} else if (strcmp(buffer, "phase_input") == 0) {
			if (parse_osc(v, &osc_type, &osc_num) != 0) {
				return 1;
			}
			osc->phase_input = &oscs[osc_num_to_index(osc_num, osc_type)];
		} else if (strcmp(buffer, "phase_input_m") == 0) {
			osc->phase_input_m = atof(v);
		} else if (strcmp(buffer, "amp_input") == 0) {
			if (parse_osc(v, &osc_type, &osc_num) != 0) {
				return 1;
			}
			osc->amp_input = &oscs[osc_num_to_index(osc_num, osc_type)];
		} else if (strcmp(buffer, "amp_input_m") == 0) {
			osc->amp_input_m = atof(v);
		} else if (strcmp(buffer, "attack") == 0) {
			osc->attack = MAX(atof(v), ATTACK_MIN);
		} else if (strcmp(buffer, "decay") == 0) {
			osc->decay = MAX(atof(v), DECAY_MIN);
		} else if (strcmp(buffer, "sustain") == 0) {
			osc->sustain = atof(v);
		} else if (strcmp(buffer, "release") == 0) {
			osc->release = atof(v);
		} else {
			// printf("unhandled line %s\n", buffer);
		}
	}

	return 0;
}

void osc_set_output(struct key* key, struct osc* osc, float t)
{
	if (osc->wave_type == WAVE_TYPE_NONE) {
		return;
	}

	float freq = osc->freq * osc->freq_m;
	freq = exp2f(log2f(freq) + osc->detune);
	if (osc->phase_input && osc->phase_input->wave_type) {
		freq += osc->phase_input->output * osc->phase_input_m;
	}
	if (freq < 0.0) {
		return;
	}

	float period = 1.0 / freq;
	float tt = fmod(t, period);

	switch (osc->wave_type) {

	case WAVE_TYPE_TRIANGLE: {
		if (tt < period / 2.0) {
			osc->output = -1.0 + (2.0 * tt * 2) / period;
		} else {
			osc->output = 1.0 - (2.0 * (tt * 2 - period)) / period;
		}
		break;
	}

	case WAVE_TYPE_SAW_UP: {
		osc->output = -1.0 + (2.0 * tt) / period;
		break;
	}

	case WAVE_TYPE_SAW_DOWN: {
		osc->output = 1.0 - (2.0 * tt) / period;
		break;
	}

	case WAVE_TYPE_SINE: {
		int i = (tt * SINE_POINTS) / period;
		if (i > SINE_POINTS || i < 0) {
			i = 0;
		}
		osc->output = sine_table[i];
		break;
	}

	case WAVE_TYPE_SQUARE: {
		if (tt < period / 2.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_PULSE12: {
		if (tt < period / 8.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_PULSE25: {
		if (tt < period / 4.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_RAND: {
		osc->output = bad_normalf();
		break;
	}
	}

	if (osc->amp_input && osc->amp_input->wave_type) {
		osc->output *= (osc->amp_input->output + 1.0) / 2.0 * osc->amp_input_m;
	}

	if (osc->output > 1.0) {
		osc->output = 1.0;
	} else if (osc->output < -1.0) {
		osc->output = -1.0;
	}

	// ASDR filtering
	if (key->pressed_at > key->released_at) {
		float time_since_press = t - key->pressed_at;
		osc->output_volume = ads_level(time_since_press, osc->attack, osc->output_volume_attack_start, osc->decay, osc->sustain);
		osc->output_volume_at_release = osc->output_volume;
	} else if (key->released_at > key->pressed_at) {
		float time_since_release = t - key->released_at;
		osc->output_volume = r_level(time_since_release, osc->output_volume_at_release, osc->decay);
	}
}

void get_key(struct key* keys, float freq, struct key** key, bool insert)
{
	for (int i = 0; i < MAX_KEYS; i++) {
		if (keys[i].freq == freq) {
			*key = &keys[i];
			return;
		}
	}
	if (!insert) {
		// key was not found, don't insert one
		return;
	}
	float oldest_pressed = 0.0;
	int oldest_i = 0;
	for (int i = 0; i < MAX_KEYS; i++) {
		if (keys[i].pressed_at == 0.0 && keys[i].released_at == 0.0) {
			*key = &keys[i];
			return;
		}
		if (keys[i].pressed_at > 0.0 && (oldest_pressed == 0 || keys[i].pressed_at < oldest_pressed)) {
			oldest_pressed = keys[i].pressed_at;
			oldest_i = i;
		}
	}
	*key = &keys[oldest_i];
}
