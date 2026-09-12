#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#define MAX_COMB_FILTER_BUCKETS 9600

#include "parser.h"
#include <stdbool.h>

struct comb_filter {
	float buffer[MAX_COMB_FILTER_BUCKETS];
	int index;
	parser_state delay_pitch;
	parser_state feedback_phase;
	parser_state feedback_gain;

	parser_state attack; // time from 0 to 1
	parser_state decay; // time from 1 to sustain level
	parser_state sustain; // level ranging from 0 to 1
	parser_state release; // time from sustain level to 0

	bool active;
	float output_volume_at_release;
	float output_volume;
};

struct key;

float iir_comb_process(struct comb_filter* cf, float input, float t, float dt, struct key* key);

#ifdef __cplusplus
}
#endif
