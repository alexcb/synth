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
#define WAVE_TYPE_RAND 8

uint32_t bad_rand_val = 0.f;
uint32_t bad_rand()
{
	bad_rand_val = bad_rand_val * 1103515245 + 12345;
	return bad_rand_val;
}

uint32_t bad_normal(uint32_t n)
{
	(bad_rand() % n + bad_rand() % n) / 2;
}

// returns a random float between -1 and 1, normally centered around 0
float bad_normalf()
{
	return bad_normal(2000) / 1000.f - 1000.f;
}

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

int parse_wave_type(const char* s)
{
	if (strcasecmp, (s, "none") == 0) {
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
	fprintf(stderr, "unknown wave type: %s\n", s);
	return 0;
}

int parse_osc(const char* s, int* n)
{
	if (strncmp(s, "osc", 3) != 0) {
		return 1;
	}
	s += 3;
	*n = atoi(s);
	return 0;
}

int load_patch(const char* path, struct osc* oscs)
{
	FILE* fp;
	fp = fopen(path, "r");
	if (fp == NULL) {
		return 1;
	}

	int n, m;

	struct osc* osc = NULL;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		int i = 0;
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
			sscanf(s, "osc%d", &i);
			if (i < 1 || i > NUM_OSCS) {
				fprintf(stderr, "err: expected osc number in range 1-10, got %d\n", i);
				return 1;
			}
			osc = &oscs[i - 1];
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
			osc->freq = atof(v);
		} else if (strcmp(buffer, "output") == 0) {
			osc->output_volume = atof(v);
		} else if (strcmp(buffer, "phase_input") == 0) {
			if (parse_osc(v, &i) == 0) {
				osc->phase_input = &oscs[i - 1];
			}
		} else if (strcmp(buffer, "phase_input_m") == 0) {
			osc->phase_input_m = atof(v);
		} else if (strcmp(buffer, "amp_input") == 0) {
			if (parse_osc(v, &i) == 0) {
				osc->amp_input = &oscs[i - 1];
			}
		} else if (strcmp(buffer, "amp_input_m") == 0) {
			osc->amp_input_m = atof(v);
		} else if (strcmp(buffer, "detune") == 0) {
			osc->detune = atof(v);
		} else {
			printf("unhandled line %s\n", buffer);
		}
	}

	fclose(fp);
}

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
	}

	case WAVE_TYPE_PULSE25: {
		if (tt < period / 4.0) {
			osc->output = 1.0;
		} else {
			osc->output = -1.0;
		}
	}

	case WAVE_TYPE_RAND: {
		osc->output = bad_normalf();
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

	load_patch("patch", oscs);

	for (int i = 0; i < NUM_OSCS; i++) {
		if (oscs[i].freq == 0) {
			oscs[i].freq = 440.0f;
		}
	}

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
