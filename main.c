#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <mpg123.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "sine_table.h"

#define RATE 44100

#define NUM_OSCS 10

#define WAVE_TYPE_NONE 0
#define WAVE_TYPE_SINE 1
#define WAVE_TYPE_TRIANGLE 2
#define WAVE_TYPE_SAW_UP 3
#define WAVE_TYPE_SAW_DOWN 4
#define WAVE_TYPE_SQUARE 5
#define WAVE_TYPE_PULSE12 6
#define WAVE_TYPE_PULSE25 7

struct osc {
	float freq;
	float detune;
	int wave_type;
	float phase_input_m;
	struct osc* phase_input;
	float amp_input_m;
	struct osc* amp_input;
	float output;
	float output_volume;
};

void osc_set_output(struct osc* osc, float t)
{
	if (osc->wave_type == WAVE_TYPE_NONE) {
		return;
	}

	float freq = osc->freq;
	freq = exp2f(log2f(freq) + osc->detune);
	if (osc->phase_input && osc->phase_input->wave_type) {
		freq += osc->phase_input->output * osc->phase_input_m;
	}

	float period = 1.0 / freq;
	float tt = fmod(t, period);

	if (osc->wave_type == WAVE_TYPE_TRIANGLE) {
		printf("WAVE_TYPE_TRIANGLE TODO\n");
	}

	if (osc->wave_type == WAVE_TYPE_SAW_UP) {
		osc->output = -1.0 + (2.0 * tt) / period;
		// printf("%f\n", osc->output);
	}

	if (osc->wave_type == WAVE_TYPE_SAW_DOWN) {
		osc->output = 1.0 - (2.0 * tt) / period;
	}

	if (osc->wave_type == WAVE_TYPE_SINE) {
		int i = (tt * SINE_POINTS) / period;
		if (i > SINE_POINTS || i < 0) {
			i = 0;
		}
		osc->output = sine_table[i];
	}

	if (osc->wave_type == WAVE_TYPE_SQUARE) {
		if (tt < period / 2.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
	}

	if (osc->wave_type == WAVE_TYPE_PULSE25) {
		if (tt < period / 4.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
	}

	if (osc->wave_type == WAVE_TYPE_PULSE12) {
		if (tt < period / 8.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
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
}

int main(int argc, char** argv, char** env)
{
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE, .rate = RATE, .channels = 1
	};

	int error;
	pa_simple* pa_handle = pa_simple_new(NULL, "synthtestbed", PA_STREAM_PLAYBACK, NULL, "playback",
	    &ss, NULL, NULL, &error);
	if (pa_handle == NULL) {
		fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n",
		    pa_strerror(error));
		assert(0);
	}
	size_t buf_seconds = 60;

	size_t buf_num_samples = buf_seconds * RATE;
	char* buf = malloc(sizeof(int16_t) * buf_num_samples);
	memset(buf, 0, sizeof(int16_t) * buf_num_samples);

	size_t n = sizeof(struct osc) * NUM_OSCS;
	struct osc* oscs = malloc(n);
	memset(oscs, 0, n);

	oscs[0].freq = 440.0f;
	oscs[0].wave_type = WAVE_TYPE_SQUARE;
	oscs[0].output_volume = 1.0f;
	oscs[0].phase_input = &oscs[0];
	oscs[0].phase_input_m = 1.1;
	oscs[0].amp_input = &oscs[1];
	oscs[0].amp_input_m = 0.5;

	oscs[1].freq = 10.0f;
	oscs[1].detune = 0.0f;
	oscs[1].wave_type = WAVE_TYPE_SINE;
	oscs[1].phase_input = &oscs[2];
	oscs[1].phase_input_m = 1.3;
	oscs[1].amp_input = &oscs[3];
	oscs[1].amp_input_m = 4.0;

	oscs[2].freq = 0.3f;
	oscs[2].wave_type = WAVE_TYPE_SQUARE;

	oscs[3].freq = 0.3f;
	oscs[3].wave_type = WAVE_TYPE_SAW_UP;

	// oscs[3].freq = 410.0f;
	// oscs[3].wave_type = WAVE_TYPE_SQUARE;
	// oscs[3].output_volume = 1.0f;

	uint32_t buf_valid_len = 0;
	for (uint32_t i = 0; i < buf_num_samples; i++) {
		float t = (i * 1.f) / RATE;

		float output = 0.0f;
		for (int i = 0; i < NUM_OSCS; i++) {
			osc_set_output(&oscs[i], t);
			output += oscs[i].output * oscs[i].output_volume;
		}

		if (output > 1.0f) {
			output = 1.0f;
		} else if (output < -1.0f) {
			output = -1.0f;
		}

		int16_t data = output * 32700;

		((int16_t*)buf)[i] = data;
		buf_valid_len = i;
	}

	int num_repeat_plays = 1;
	for (int i = 0; i < num_repeat_plays; i++) {
		int n = 2 * buf_valid_len;
		int res = pa_simple_write(pa_handle, buf, n, &error);
		if (res < 0) {
			fprintf(stderr, "res=%d err=%d pa_simple_write failed", res, error);
		}
	}

	free(buf);

	return 0;
}
