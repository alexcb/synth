#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "parser.h"
#include <stdbool.h>

#define WAVE_TYPE_NONE 0
#define WAVE_TYPE_SINE 1
#define WAVE_TYPE_TRIANGLE 2
#define WAVE_TYPE_SAW_UP 3
#define WAVE_TYPE_SAW_DOWN 4
#define WAVE_TYPE_SQUARE 5
#define WAVE_TYPE_PULSE12 6
#define WAVE_TYPE_PULSE25 7
#define WAVE_TYPE_RAND_UNIFORM 8
#define WAVE_TYPE_RAND_NORMAL 9

#define OSC_TYPE_NONE 0
#define OSC_TYPE_VFO 1
#define OSC_TYPE_LFO 2

#define ATTACK_MIN 0.01
#define DECAY_MIN 0.01

// IMPORTANT: leave this as 5, since it's hardcoded in synth.c
#define NUM_OSCS 5

#define MAX_KEYS 8

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// TODO remove this
void foo(char* p);

// parser_state {
//	float m;
//	float* v;
//	float a;
// };

struct osc {
	float wave_pos;
	int wave_type;
	parser_state freq;
	parser_state input;
	parser_state drive;
	parser_state output_volume_m;
	parser_state attack; // time from 0 to 1
	parser_state decay; // time from 1 to sustain level
	parser_state sustain; // level ranging from 0 to 1
	parser_state release; // time from sustain level to 0

	bool active;
	float output_volume; // set by ARSD envolop calcs
	float output_volume_at_release;
	float output_volume_attack_start;
	float output;
	float delta_output; // used by input param
};

struct key {
	float freq;
	float pressed_at;
	float released_at;
	float velocity;
	struct osc* oscs;

	float future_released_at; // only to be used while using computer keyboard trigger
};

struct params {
	float pitch;
	float mod;
	float c1;
	float c2;
	float c3;
	float c4;
};

// struct key_params {
//	float velocity;
//	float freq;
//	float osc1;
//	float osc2;
//	float osc3;
//	float osc4;
//	float osc5;
// };

int synth_new(struct key** keys);
void synth_clear(struct key* keys);

int parse_wave_type(const char* s);
int parse_osc(const char* s, int* n);
int load_patch(char* src, struct osc* oscs, struct params* param_values, struct key* key);
void osc_set_output(struct key* key, struct osc* osc, struct params* params, float t, float dt);
void get_key(struct key* keys, float freq, struct key** key, bool insert);

const char* load_patch_err();

float get_float_param(parser_state* p);
void set_float_param(parser_state* p, float v);

// TODO remove this
int osc_num_to_index(int osc_num);

#ifdef __cplusplus
}
#endif
