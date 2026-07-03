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
#include <circle/alloc.h>

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
#include <assert.h>
#endif

#include <math.h>

#define MAX_SYNTH_ERR_MSG_SIZE 1024
char synth_error_message[MAX_SYNTH_ERR_MSG_SIZE];


const char* load_patch_err()
{
	return synth_error_message;
}

int synth_new(struct key** keys)
{
	size_t key_bytes = sizeof(struct key) * MAX_KEYS;
	size_t osc_bytes = sizeof(struct osc) * NUM_OSCS * NUM_OSC_TYPES;
	*keys = malloc(key_bytes); // static_cast<struct key*>(::operator new(key_bytes));
	memset(*keys, 0, key_bytes);
	for (size_t i = 0; i < MAX_KEYS; i++) {
		(*keys)[i].oscs = malloc(osc_bytes); // static_cast<struct osc*>(::operator new(osc_bytes));
		memset((*keys)[i].oscs, 0, osc_bytes);
	}
	return 0;
}

void synth_clear(struct key* keys)
{
	for (size_t i = 0; i < MAX_KEYS; i++) {
		struct osc *p = keys[i].oscs;
		memset(&(keys[i]), 0, sizeof(struct key));
		keys[i].oscs = p;
		memset(p, 0, sizeof(struct osc)*NUM_OSCS * NUM_OSC_TYPES);
	}
}

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
	if (*n == 0 || *n > NUM_OSCS) {
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

// osc_num is from 1 to NUM_OSCS (not 0-indexed)
int osc_num_to_index(int osc_num, int osc_type)
{
	if (osc_num < 1 || osc_num > NUM_OSCS) {
		assert(0);
	}
	int i = (osc_num - 1);
	if (osc_type == OSC_TYPE_LFO) {
		i += NUM_OSCS;
	}
	if (i >= NUM_OSCS * NUM_OSC_TYPES) {
		assert(0);
	}
	return i;
}

#define MAX_LINE 1024

int load_patch(char* src, struct osc* oscs)
{
	synth_error_message[0] = '\0';
	int n;

	int osc_num;
	int osc_type;

	char line[MAX_LINE];
	char key[MAX_LINE];
	char value[MAX_LINE];

	bool eof = false;
	struct osc* osc = NULL;
	while(*src) {
		size_t n = 0;
		char *l = 0;
		char* eol = strchr(src, '\n');
		if( eol ) {
			n = eol - src;
		} else {
			if( strlen(src) ) {
				strcpy(synth_error_message, "patch must end with a newline");
				return 1;
			}
			break;
		}
		if( n >= (MAX_LINE-1) ) {
			strcpy(synth_error_message, "line too long");
			return 1;
		}
		memcpy(line, src, n);
		line[n] = '\0';
		src += n+1;

		if( !*line ) {
			// blank line
			continue;
		}
		if( line[0] == '#' ) {
			// ignore comment
			continue;
		}

		n = strlen(line);
		//if (buffer[0] == '#') {
		//	// TODO something here isn't working; because I had to change the encode_patch.py script to ignore these lines, and then the sound changed
		//	continue;
		//}
		for (int j = n - 1; j >= 0; j--) {
			if (line[j] == '\n') {
				line[j] = '\0';
				n--;
			}
		}
		// printf("here with line %s\n", line);
		if (line[0] == '[' && line[n - 1] == ']') {
			line[n - 1] = '\0';
			char* s = &line[1];

			if (parse_osc(s, &osc_type, &osc_num) != 0) {
				// circle doesnt have sprintf
				strcpy(synth_error_message, "failed to parse ");
				strcpy(synth_error_message+strlen(synth_error_message), s);
				return 1;
			}

			if (osc_num < 1 || osc_num > NUM_OSCS) {
				// circle doesnt have sprintf
				strcpy(synth_error_message, "expected osc number in range 1-10 while parsing ");
				strcpy(synth_error_message+strlen(synth_error_message), s);
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
		// printf("here2 with line %s\n", line);
		char* v = strstr(line, "=");
		if (v == NULL) {
			// printf("failed %s\n", line);
			strcpy(synth_error_message, "failed to parse key=value pair ");
			strcpy(synth_error_message+strlen(synth_error_message), line);
			return 1;
		}
		n = v-line;
		memcpy(key, line, n);
		key[n] = '\0';
		strcpy(value, v+1);
		
		for( v = value; *v; v++ ) {
			if( *v == '#' ) {
				*v = '\0';
				v--;
				break;
			}
		}
		while(v >= value ) {
			if( *v == ' ' ) {
				*v = '\0';
				v--;
			} else {
				break;
			}
		}

		 // printf("got key: %s; value: %s\n", key, value);


		if (strcmp(key, "type") == 0) {
			osc->wave_type = parse_wave_type(value);
		} else if (strcmp(key, "freq") == 0) {
			if (strcmp(value, "sync") == 0) {
				osc->freq_sync = true;
			} else {
				osc->freq = atof(value);
			}
		} else if (strcmp(key, "freq_m") == 0) {
			osc->freq_m = atof(value);
		} else if (strcmp(key, "detune") == 0) {
			osc->detune = atof(value);
		} else if (strcmp(key, "output") == 0) {
			osc->output_volume_m = atof(value);
		} else if (strcmp(key, "phase_input") == 0) {
			if (parse_osc(value, &osc_type, &osc_num) != 0) {
				strcpy(synth_error_message, "failed to parse phase_input ");
				strcpy(synth_error_message+strlen(synth_error_message), value);
				return 1;
			}
			int osc_i = osc_num_to_index(osc_num, osc_type);
			if (osc_i < 0) {
				strcpy(synth_error_message, "failed to convert phase_input ");
				strcpy(synth_error_message+strlen(synth_error_message), value);
				return 1;
			}
			osc->phase_input = &oscs[osc_i];
		} else if (strcmp(key, "phase_input_m") == 0) {
			osc->phase_input_m = atof(value);
		} else if (strcmp(key, "amp_input") == 0) {
			if (parse_osc(value, &osc_type, &osc_num) != 0) {
				strcpy(synth_error_message, "failed to parse amp_input ");
				strcpy(synth_error_message+strlen(synth_error_message), value);
				return 1;
			}
			int osc_i = osc_num_to_index(osc_num, osc_type);
			if (osc_i < 0) {
				strcpy(synth_error_message, "failed to convert amp_input ");
				strcpy(synth_error_message+strlen(synth_error_message), value);
				return 1;
			}
			osc->amp_input = &oscs[osc_i];
		} else if (strcmp(key, "amp_input_m") == 0) {
			osc->amp_input_m = atof(value);
		} else if (strcmp(key, "attack") == 0) {
			osc->attack = MAX(atof(value), ATTACK_MIN);
		} else if (strcmp(key, "decay") == 0) {
			osc->decay = MAX(atof(value), DECAY_MIN);
		} else if (strcmp(key, "sustain") == 0) {
			osc->sustain = atof(value);
		} else if (strcmp(key, "release") == 0) {
			osc->release = atof(value);
		} else if (strcmp(key, "pitch_m") == 0) {
			osc->pitch_m = atof(value);
		} else if (strcmp(key, "mod_freq_m") == 0) {
			osc->mod_freq_m = atof(value);
		} else if (strcmp(key, "mod_output_m") == 0) {
			osc->mod_output_m = atof(value);
		} else {
			// printf("unhandled line %s\n", key);
		}
	}

	return 0;
}

void osc_set_output(struct key* key, struct osc* osc, struct params* params, float t, float dt)
{
	if (osc->wave_type == WAVE_TYPE_NONE) {
		assert(osc->output == 0.0f);
		return;
	}

	float freq = osc->freq * osc->freq_m;
	if (freq <= 0.0) {
		osc->output = 0.0f;
		return;
	}

	freq = exp2f(log2f(freq) + params->pitch * osc->pitch_m + params->mod * osc->mod_freq_m + osc->detune);

	if (osc->phase_input && osc->phase_input->wave_type) {
		freq += osc->phase_input->output * osc->phase_input_m;
	}

	osc->wave_pos = fmod(osc->wave_pos + dt * freq, 1.f);

	switch (osc->wave_type) {

	case WAVE_TYPE_TRIANGLE: {
		if (osc->wave_pos < 0.5f) {
			osc->output = -1.0 + (4.0 * osc->wave_pos);
		} else {
			osc->output = 1.0 - (2.0 * (osc->wave_pos * 2.f - 1.f));
		}
		break;
	}

	case WAVE_TYPE_SAW_UP: {
		osc->output = -1.0 + (2.0 * osc->wave_pos);
		break;
	}

	case WAVE_TYPE_SAW_DOWN: {
		osc->output = 1.0 - (2.0 * osc->wave_pos);
		break;
	}

	case WAVE_TYPE_SINE: {
		int i = (osc->wave_pos * SINE_POINTS);
		if (i > SINE_POINTS || i < 0) {
			i = 0;
		}
		osc->output = sine_table[i];
		break;
	}

	case WAVE_TYPE_SQUARE: {
		if (osc->wave_pos < 0.5f) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_PULSE12: {
		if (osc->wave_pos < 0.125f) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_PULSE25: {
		if (osc->wave_pos < 0.25f) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
		break;
	}

	case WAVE_TYPE_RAND: {
		// osc->output = bad_normalf();
		osc->output = bad_randf();
		break;
	}
	}

	if (osc->amp_input && osc->amp_input->wave_type) {
		osc->output *= (osc->amp_input->output + 1.0) / 2.0 * osc->amp_input_m;
	}

	if (osc->mod_output_m > 0.0f) {
		osc->output *= osc->mod_output_m * params->mod;
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
		if (keys[i].freq == 0.0) {
			*key = &keys[i];
			return;
		}
		if (keys[i].pressed_at > 0.0 && (oldest_pressed == 0.0 || keys[i].pressed_at < oldest_pressed)) {
			oldest_pressed = keys[i].pressed_at;
			oldest_i = i;
		}
	}
	*key = &keys[oldest_i];
}
