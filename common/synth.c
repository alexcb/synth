#include "synth.h"
#include "bad_rand.h"
#include "parser.h"
#include "sine_table.h"

#ifdef __circle__
#include "acb_atof.h"
#include "acb_isspace.h"
#include <circle/alloc.h>
#include <circle/util.h>

// these are in util.h
// int strcmp (const char *pString1, const char *pString2);
// int strcasecmp (const char *pString1, const char *pString2);
// int strncmp (const char *pString1, const char *pString2, size_t nMaxLen);
// int strncasecmp (const char *pString1, const char *pString2, size_t nMaxLen);
#define uint32_t unsigned
#define NULL 0
#else
#include <assert.h>
#include <stdlib.h>
#include <string.h>
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
	size_t osc_bytes = sizeof(struct osc) * NUM_OSCS;
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
		struct osc* p = keys[i].oscs;
		memset(&(keys[i]), 0, sizeof(struct key));
		keys[i].oscs = p;
		for (int j = 0; j < NUM_OSCS; j++) {
			parser_cleanup(&p[j].freq);
			parser_cleanup(&p[j].input);
			parser_cleanup(&p[j].drive);
			parser_cleanup(&p[j].output_volume_m);
			parser_cleanup(&p[j].attack);
			parser_cleanup(&p[j].decay);
			parser_cleanup(&p[j].sustain);
			parser_cleanup(&p[j].release);
		}
		memset(p, 0, sizeof(struct osc) * NUM_OSCS);
	}
}

// set the param to a static value
void set_float_param(parser_state* p, float v)
{
	parser_cleanup(p);
	if (parser("1.0", p, NULL)) {
		assert(0);
	}
	assert(p->ast_root->type == AST_FLOAT);
	p->ast_root->f = v;
	//(**node).type = AST_FLOAT;
	//(**node).f = t.f;

	// p->m = v;
	// p->v = NULL;
}

int parse_float_param(const char* s, parser_state* p, struct osc* osc, struct params* param_values, struct key* key_param_values, bool use_delta)
{

	variable_pointers vars = {
		.velocity = &(key_param_values->velocity),
		.key_freq = &(key_param_values->freq),
		.pitch = &(param_values->pitch),
		.mod = &(param_values->mod),
		.c1 = &(param_values->c1),
		.c2 = &(param_values->c2),
		.c3 = &(param_values->c3),
		.c4 = &(param_values->c4),
	};
	if (use_delta) {
		vars.osc1 = &(key_param_values->oscs[0].delta_output);
		vars.osc2 = &(key_param_values->oscs[1].delta_output);
		vars.osc3 = &(key_param_values->oscs[2].delta_output);
		vars.osc4 = &(key_param_values->oscs[3].delta_output);
		vars.osc5 = &(key_param_values->oscs[4].delta_output);
	} else {
		vars.osc1 = &(key_param_values->oscs[0].output);
		vars.osc2 = &(key_param_values->oscs[1].output);
		vars.osc3 = &(key_param_values->oscs[2].output);
		vars.osc4 = &(key_param_values->oscs[3].output);
		vars.osc5 = &(key_param_values->oscs[4].output);
	}

	if (parser(s, p, &vars)) {
		return 1;
	}
	return 0;
}

float get_float_param(parser_state* p)
{
	if (p == NULL || p->ast_root == NULL) {
		return 0.0f;
	}
	return eval(p->ast_root);
	// if (p->v) {
	//	return *(p->v) * p->m + p->a;
	// }
	// return p->m + p->a;
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
	if (strcasecmp(s, "random_uniform") == 0) {
		return WAVE_TYPE_RAND_UNIFORM;
	}
	if (strcasecmp(s, "random_normal") == 0) {
		return WAVE_TYPE_RAND_NORMAL;
	}
	// fprintf(stderr, "unknown wave type: %s\n", s);
	return 0;
}

int parse_osc(const char* s, int* n)
{
	if (strlen(s) < 4) {
		return 1;
	}
	if (strncmp(s, "osc", 3) != 0) {
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
	release = MAX(release, 0.03); // TODO there's a bug where keys stop working when this is 0
	if (t > release) {
		return 0.f;
	}
	return orig_vol * (1.0 - t / release);
}

// osc_num is from 1 to NUM_OSCS (not 0-indexed)
int osc_num_to_index(int osc_num)
{
	if (osc_num < 1 || osc_num > NUM_OSCS) {
		assert(0);
	}
	return (osc_num - 1);
}

#define MAX_LINE 1024

int load_patch(char* src, struct osc* oscs, struct params* param_values, struct key* key_param_values)
{
	synth_error_message[0] = '\0';

	int osc_num;

	char line[MAX_LINE];
	char key[MAX_LINE];
	char value[MAX_LINE];

	struct osc* osc = NULL;
	struct comb_filter* combfilter = NULL;
	while (*src) {
		size_t n = 0;
		char* eol = strchr(src, '\n');
		if (eol) {
			n = eol - src;
		} else {
			if (strlen(src)) {
				strcpy(synth_error_message, "patch must end with a newline");
				return 1;
			}
			break;
		}
		if (n >= (MAX_LINE - 1)) {
			strcpy(synth_error_message, "line too long");
			return 1;
		}
		memcpy(line, src, n);
		line[n] = '\0';
		src += n + 1;

		if (!*line) {
			// blank line
			continue;
		}
		if (line[0] == '#') {
			// ignore comment
			continue;
		}

		n = strlen(line);
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

			osc = NULL;
			combfilter = NULL;
			if (parse_osc(s, &osc_num) == 0) {
				if (osc_num < 1 || osc_num > NUM_OSCS) {
					strcpy(synth_error_message, "expected osc number in range 1-10 while parsing ");
					strcpy(synth_error_message + strlen(synth_error_message), s);
					return 1;
				}
				osc = &oscs[osc_num_to_index(osc_num)];
				if (osc->wave_type != WAVE_TYPE_NONE) {
					// TODO I need a sprintf to make these errors better
					strcpy(synth_error_message, "osc already defined ");
					strcpy(synth_error_message + strlen(synth_error_message), s);
					return 1;
				}

				// init defaults
				if (parse_float_param("key_freq", &osc->freq, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to init osc freq value ");
					return 1;
				}
				set_float_param(&osc->input, 0.0);
				set_float_param(&osc->drive, 1.0);
				set_float_param(&osc->attack, ATTACK_MIN);
				set_float_param(&osc->sustain, 1.0);
				set_float_param(&osc->decay, DECAY_MIN);
				set_float_param(&osc->release, 0.0);
				set_float_param(&osc->output_volume_m, 1.0);
				osc->wave_type = WAVE_TYPE_SINE;

			} else if (strcmp(s, "combfilter") == 0) {
				combfilter = &(key_param_values->comb_filter);
				set_float_param(&combfilter->attack, ATTACK_MIN);
				set_float_param(&combfilter->sustain, 1.0);
				set_float_param(&combfilter->decay, DECAY_MIN);
				set_float_param(&combfilter->release, 0.0);
			} else {
				strcpy(synth_error_message, "failed to parse section header ");
				strcpy(synth_error_message + strlen(synth_error_message), s);
				return 1;
			}

			continue;
		}
		// printf("here2 with line %s\n", line);
		char* v = strstr(line, "=");
		if (v == NULL) {
			// printf("failed %s\n", line);
			strcpy(synth_error_message, "failed to parse key=value pair ");
			strcpy(synth_error_message + strlen(synth_error_message), line);
			return 1;
		}
		n = v - line;
		memcpy(key, line, n);
		key[n] = '\0';
		strcpy(value, v + 1);

		for (v = value; *v; v++) {
			if (*v == '#') {
				*v = '\0';
				v--;
				break;
			}
		}
		while (v >= value) {
			if (*v == ' ') {
				*v = '\0';
				v--;
			} else {
				break;
			}
		}

		if (osc != NULL) {
			if (strcmp(key, "type") == 0) {
				osc->wave_type = parse_wave_type(value);
			} else if (strcmp(key, "freq") == 0) {
				if (parse_float_param(value, &osc->freq, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse freq ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "input") == 0) {
				if (parse_float_param(value, &osc->input, osc, param_values, key_param_values, true)) {
					strcpy(synth_error_message, "failed to parse input ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "output") == 0) {
				if (parse_float_param(value, &osc->output_volume_m, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse output ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "drive") == 0) {
				if (parse_float_param(value, &osc->drive, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse drive ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "attack") == 0) {
				if (parse_float_param(value, &osc->attack, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse attack ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "decay") == 0) {
				if (parse_float_param(value, &osc->decay, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse decay ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "sustain") == 0) {
				if (parse_float_param(value, &osc->sustain, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse sustain ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "release") == 0) {
				if (parse_float_param(value, &osc->release, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse release ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else {
				// printf("unhandled line %s\n", key);
			}
		} else if (combfilter != NULL) {
			if (strcmp(key, "feedback") == 0) {
				if (parse_float_param(value, &combfilter->feedback_gain, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse feedback ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "pitch") == 0) {
				if (parse_float_param(value, &combfilter->delay_pitch, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse pitch ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "attack") == 0) {
				if (parse_float_param(value, &combfilter->attack, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse attack ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "decay") == 0) {
				if (parse_float_param(value, &combfilter->decay, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse decay ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "sustain") == 0) {
				if (parse_float_param(value, &combfilter->sustain, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse sustain ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			} else if (strcmp(key, "release") == 0) {
				if (parse_float_param(value, &combfilter->release, osc, param_values, key_param_values, false)) {
					strcpy(synth_error_message, "failed to parse release ");
					strcpy(synth_error_message + strlen(synth_error_message), value);
					return 1;
				}
			}
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

	float last_output = osc->output;

	float freq = get_float_param(&osc->freq);
	if (freq <= 0.0) {
		osc->output = 0.0f;
		return;
	}

	float input = get_float_param(&osc->input);

	osc->wave_pos = fmod(osc->wave_pos + dt * freq + input, 1.f);
	if (osc->wave_pos < 0.0f) {
		osc->wave_pos += 1.f;
	}

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

	case WAVE_TYPE_RAND_UNIFORM: {
		osc->output = bad_randf() * 2.0 - 1.0;
		break;
	}
	case WAVE_TYPE_RAND_NORMAL: {
		osc->output = bad_normalf();
		break;
	}
	}

	osc->output *= get_float_param(&osc->drive);

	if (osc->output > 1.0) {
		osc->output = 1.0;
	} else if (osc->output < -1.0) {
		osc->output = -1.0;
	}

	// ASDR filtering
	if (key->pressed_at > key->released_at) {
		float time_since_press = t - key->pressed_at;
		// FIXME osc->output_volume_attack_start isn't set anywhere, it's always 0. should it be the previous output volume?
		osc->output_volume = ads_level(time_since_press, get_float_param(&osc->attack), osc->output_volume_attack_start, get_float_param(&osc->decay), get_float_param(&osc->sustain));
		osc->output_volume_at_release = osc->output_volume;
	} else if (key->released_at > key->pressed_at) {
		float time_since_release = t - key->released_at;
		osc->output_volume = r_level(time_since_release, osc->output_volume_at_release, get_float_param(&osc->release));
		if (osc->output_volume == 0.f) {
			osc->active = false;
		}
	}
	osc->output *= osc->output_volume;
	osc->delta_output = osc->output - last_output;
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

	// next look for keys that are no long active
	for (int i = 0; i < MAX_KEYS; i++) {
		if (keys[i].freq == 0.0) {
			*key = &keys[i];
			return;
		}
	}

	// next look for keys that have been released, and pick the oldest
	int oldest_i = -1;
	float oldest_released = 0.0;
	for (int i = 0; i < MAX_KEYS; i++) {
		if (keys[i].released_at > 0.0 && keys[i].released_at > oldest_released) {
			oldest_released = keys[i].released_at;
			oldest_i = i;
		}
	}
	if (oldest_i < 0) {
		// if they are all being held, look for the oldest pressed
		float oldest_pressed = 0.0;
		for (int i = 0; i < MAX_KEYS; i++) {
			if (keys[i].pressed_at > 0.0 && (oldest_pressed == 0.0 || keys[i].pressed_at < oldest_pressed)) {
				oldest_pressed = keys[i].pressed_at;
				oldest_i = i;
			}
		}
	}

	if (oldest_i < 0) {
		// give up and use the first one
		oldest_i = 0;
	}

	*key = &keys[oldest_i];
}
