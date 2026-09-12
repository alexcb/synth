#include "comb_filter.h"
#include "synth.h"
#include <assert.h>

// TODO get_float_param is in synth.c, but really needs to be moved elsewhere
float get_float_param_needs_help(parser_state* p)
{
	if (p == NULL || p->ast_root == NULL) {
		return 0.0f;
	}
	return eval(p->ast_root);
}

// TODO move theses into an envolope.h
float ads_level(float t, float attack, float attack_start, float decay, float sustain);
float r_level(float t, float orig_vol, float release);

// Process a single audio sample
float iir_comb_process(struct comb_filter* cf, float input, float t, float dt, struct key* key)
{
	// Read the delayed sample from the circular buffer
	float delayed = cf->buffer[cf->index];

	float feedback_gain = get_float_param_needs_help(&cf->feedback_gain);

	if (feedback_gain == 0) {
		// filter is not enabled
		return input;
	}

	// Calculate output for feedback comb filter: y[n] = x[n] + g * y[n-M]
	// Wait, standard Schroeder comb filter: y[n] = input + g * delayed, and buffer stores y[n] or x[n]
	float output = input + (feedback_gain * delayed);

	// float feedback_phase = get_float_param_needs_help(&cf->feedback_phase);
	// int index = ((feedback_phase + 1.0f) / 2.0f) * (cf->delay_length - 1);
	// if (index < 0 || index >= cf->delay_length) {
	//	index = 0;
	// }

	// Write current output back into the delay line
	cf->buffer[cf->index] = output;

	cf->index++;

	float delay_pitch = get_float_param_needs_help(&cf->delay_pitch);
	float delay_buckets = 1.0 / (dt * delay_pitch);

	// delay_buckets = MAX_COMB_FILTER_BUCKETS;

	if (cf->index >= delay_buckets) {
		cf->index = 0;
	}
	if (cf->index >= MAX_COMB_FILTER_BUCKETS) {
		// TODO issue a warning?
		cf->index = 0;
	}

	// ASDR filtering
	if (key->pressed_at > key->released_at) {
		float time_since_press = t - key->pressed_at;
		cf->output_volume = ads_level(time_since_press, get_float_param(&cf->attack), 0.0f, get_float_param(&cf->decay), get_float_param(&cf->sustain));
		cf->output_volume_at_release = cf->output_volume;
	} else if (key->released_at > key->pressed_at) {
		float time_since_release = t - key->released_at;
		cf->output_volume = r_level(time_since_release, cf->output_volume_at_release, get_float_param(&cf->release));
		if (cf->output_volume == 0.f) {
			cf->active = false;
		}
	}

	return output * cf->output_volume;
}
