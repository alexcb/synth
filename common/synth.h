#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <stdbool.h>

#define WAVE_TYPE_NONE 0
#define WAVE_TYPE_SINE 1
#define WAVE_TYPE_TRIANGLE 2
#define WAVE_TYPE_SAW_UP 3
#define WAVE_TYPE_SAW_DOWN 4
#define WAVE_TYPE_SQUARE 5
#define WAVE_TYPE_PULSE12 6
#define WAVE_TYPE_PULSE25 7
#define WAVE_TYPE_RAND 8

#define OSC_TYPE_VFO 1
#define OSC_TYPE_LFO 2

#define ATTACK_MIN 0.01
#define DECAY_MIN 0.01

#define NUM_OSCS 5
#define NUM_OSC_TYPES 2

#define MAX_KEYS 3

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// TODO remove this
void foo(char* p);

struct osc {
	float freq;
	bool freq_sync;
	float freq_m;
	float detune;
	int osc_type;
	int wave_type;
	float phase_input_m;
	struct osc* phase_input;
	float amp_input_m;
	struct osc* amp_input;
	float output_volume_m;
	float attack; // time from 0 to 1
	float decay; // time from 1 to sustain level
	float sustain; // level ranging from 0 to 1
	float release; // time from sustain level to 0

	// internal values
	// float pressed_at; // TODO remove these
	// float released_at;
	// float velocity;

	float output_volume; // set by ARSD envolop calcs
	float output_volume_at_release;
	float output_volume_attack_start;
	float output;
};

struct key {
	float freq;
	float pressed_at;
	float released_at;
	float velocity;
	struct osc* oscs;

	float future_released_at; // only to be used while using computer keyboard trigger
};

int parse_wave_type(const char* s);
int parse_osc(const char* s, int* osc_type, int* n);
int load_patch(char* path, struct osc* oscs);
void osc_set_output(struct key* key, struct osc* osc, float t);
void get_key(struct key* keys, float freq, struct key** key, bool insert);

#ifdef __cplusplus
}
#endif
