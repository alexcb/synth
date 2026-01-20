// Copyright (C) 2017-2023  R. Stange <rsta2@o2online.de>
// Copyright (C) 2025  Alex Couture-Beil <alex@mofo.ca>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <mpg123.h>
#include <pthread.h>
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

#include <ncurses.h>

#include "../common/sine_table.h"

#define RATE 44100

#define NUM_OSCS 10
#define NUM_OSC_TYPES 2

#define MAX_KEYS 10

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

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

FILE* wave_fp = NULL;

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

FILE* log_file = NULL;
void debuglog(const char* s)
{
	if (log_file == NULL) {
		log_file = fopen("/tmp/synth.log", "w");
		if (log_file == NULL) {
			fprintf(stderr, "failed to open logfile");
			assert(log_file);
		}
	}
	fprintf(log_file, "%s", s);
	fflush(log_file);
}

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

int osc_num_to_index(int osc_num, int osc_type)
{
	int i = (osc_num - 1);
	if (osc_type == OSC_TYPE_LFO) {
		i += NUM_OSCS;
	}
	return i;
}

int load_patch(const char* path, struct osc* oscs)
{
	FILE* fp;
	fp = fopen(path, "r");
	if (fp == NULL) {
		return 1;
	}

	int n, m;

	int osc_num;
	int osc_type;

	struct osc* osc = NULL;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
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
				fprintf(stderr, "err: failed to parse %s\n", s);
				return 1;
			}

			if (osc_num < 1 || osc_num > NUM_OSCS) {
				fprintf(stderr, "err: expected osc number in range 1-10, got %d\n", osc_num);
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
			printf("unhandled line %s\n", buffer);
		}
	}

	fclose(fp);
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

float get_freq(const char c)
{
	switch (c) {
	case 'z':
		return 261.626;
	case 'x':
		return 277.183;
	case 'c':
		return 293.665;
	case 'v':
		return 311.127;
	case 'b':
		return 329.628;
	case 'n':
		return 349.228;
	case 'm':
		return 369.994;
	case ',':
		return 391.995;
	case '.':
		return 415.305;
	case '/':
		return 440.000;
	default:
		return 0.0f;
	}
}

void get_key(struct key* keys, float freq, struct key** key)
{
	for (int i = 0; i < MAX_KEYS; i++) {
		if (keys[i].freq == freq) {
			*key = &keys[i];
			return;
		}
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

struct wav_header {
	char riff[4]; /* "RIFF"                                  */
	int32_t flength; /* file length in bytes                    */
	char wave[4]; /* "WAVE"                                  */
	char fmt[4]; /* "fmt "                                  */
	int32_t chunk_size; /* size of FMT chunk in bytes (usually 16) */
	int16_t format_tag; /* 1=PCM, 257=Mu-Law, 258=A-Law, 259=ADPCM */
	int16_t num_chans; /* 1=mono, 2=stereo                        */
	int32_t srate; /* Sampling rate in samples per second     */
	int32_t bytes_per_sec; /* bytes per second = srate*bytes_per_samp */
	int16_t bytes_per_samp; /* 2=16-bit mono, 4=16-bit stereo          */
	int16_t bits_per_samp; /* Number of bits per sample               */
	// char data[4]; /* "data"                                  */
	// int32_t dlength; /* data length in bytes (filelength - 44)  */
};

void write_wav_header(int32_t sample_rate)
{
	struct wav_header wavh;
	strncpy(wavh.riff, "RIFF", 4);
	strncpy(wavh.wave, "WAVE", 4);
	strncpy(wavh.fmt, "fmt ", 4);

	wavh.chunk_size = 16;
	wavh.format_tag = 1;
	wavh.num_chans = 1;
	wavh.srate = sample_rate;
	wavh.bits_per_samp = 16;
	wavh.bytes_per_sec = wavh.srate * wavh.bits_per_samp / 8 * wavh.num_chans;
	wavh.bytes_per_samp = wavh.bits_per_samp / 8 * wavh.num_chans;

	const int header_length = sizeof(struct wav_header);
	fwrite(&wavh, 1, header_length, wave_fp);
}

void write_wave_data(uint32_t length, char* buffer)
{
	fwrite("data", 1, 4, wave_fp);
	fwrite(&length, 4, 1, wave_fp); // Data size
	fwrite(buffer, 1, length, wave_fp);
}

uint32_t rate_increment = 1e9 / RATE;

pa_simple* pa_handle;

bool do_shutdown = false;
pthread_mutex_t the_lock;
pthread_cond_t the_cond;
char the_key = '\0';

size_t buf_num_samples;
char* buf[2];
int which_buf;
bool buf_full;
struct key* keys;

void* producer(void* param)
{
	char pressed_key = '\0';
	float t = 0.f;
uint32_t tt = 0;
	for (;;) {

		for (uint32_t i = 0; i < buf_num_samples; i++) {
			//t += 1.f / RATE;
			tt += rate_increment;
			t = tt / (float)1e9;

			float freq = get_freq(pressed_key);
			if (freq > 0.f) {
				struct key* k;
				get_key(keys, freq, &k);
				// simulate a key press
				// printf("pressed at %f\n", t);
				k->future_released_at = t + 10.3; // hold for some extra time (only while using computer keyboard)

				bool keep_output = (k->released_at > 0.0f && k->freq == freq);
				k->freq = freq;
				k->velocity = 1.0f;
				k->pressed_at = t;
				k->released_at = 0.0f;
				for (int i = 0; i < NUM_OSCS; i++) {
					struct osc* osc = &k->oscs[i];
					osc->freq = freq;
					if (keep_output) {
						osc->output_volume_attack_start = osc->output_volume;
					} else {
						osc->output_volume_attack_start = 0;
					}
				}
				for (int i = 0; i < NUM_OSCS; i++) {
					struct osc* osc = &k->oscs[i + NUM_OSCS]; // LFOs are in the second set
					if (osc->freq_sync) {
						osc->freq = freq;
					}
				}
			}

			for (int i = 0; i < MAX_KEYS; i++) {
				if (keys[i].future_released_at != 0.f && keys[i].future_released_at < t) {
					keys[i].future_released_at = 0.f;
					keys[i].released_at = t;
					debuglog("released\n");
				}
			}

			float output = 0.0f;
			for (int i = 0; i < MAX_KEYS; i++) {
				bool done = true;
				for (int j = 0; j < NUM_OSCS * NUM_OSC_TYPES; j++) {
					struct osc* osc = &keys[i].oscs[j];
					osc_set_output(&keys[i], osc, t);
					if (osc->osc_type == OSC_TYPE_VFO) {
						output += osc->output * osc->output_volume * osc->output_volume_m;
						if (osc->output_volume > 0.0 || keys[i].released_at == 0.0) {
							done = false;
						}
					}
				}
				if (done) {
					keys[i].pressed_at = 0.f;
					keys[i].released_at = 0.f;
				}
			}

			if (output > 1.0f) {
				output = 1.0f;
			} else if (output < -1.0f) {
				output = -1.0f;
			}

			int16_t data = output * 32700;

			((int16_t*)buf[which_buf])[i] = data;
			// if (wave_i < wave_file_samples) {
			//	((int16_t*)wave_file_data)[wave_i++] = data;
			// }
		}

		pthread_mutex_lock(&the_lock);
		// char sss[1024];
		// sprintf(sss, "buf full for %d\n", which_buf);
		// debuglog(sss);
		buf_full = true;
		pthread_mutex_unlock(&the_lock);

		bool ready_for_more = false;
		while (!ready_for_more) {
			pthread_mutex_lock(&the_lock);
			if (buf_full) {
				pthread_cond_wait(&the_cond, &the_lock);
			}
			ready_for_more = !buf_full || do_shutdown;
			if (ready_for_more) {
				pressed_key = the_key;
				the_key = '\0';
			}
			pthread_mutex_unlock(&the_lock);
		}

		if (do_shutdown) {
			debuglog("producer exit\n");
			return NULL;
		}

		// int num_repeat_plays = 1;
		// for (int i = 0; i < num_repeat_plays; i++) {
		//	int n = 2 * buf_num_samples;
		//	int res = pa_simple_write(pa_handle, buf, n, &error);
		//	if (res < 0) {
		//		fprintf(stderr, "res=%d err=%d pa_simple_write failed", res, error);
		//		debuglog("pa_simple_write failed\n");
		//	}
		// }

		// if (write_wave_file && wave_i == wave_file_samples) {
		//	break;
		// }
	}
}

void* consumer(void* param)
{
	int error;
	char* b = NULL;
	for (;;) {
		pthread_mutex_lock(&the_lock);
		if (buf_full) {
			b = buf[which_buf];
			buf_full = false;
			which_buf = (which_buf + 1) % 2;
		}
		pthread_mutex_unlock(&the_lock);
		pthread_cond_signal(&the_cond);

		if (b) {
			int n = 2 * buf_num_samples;
			int res = pa_simple_write(pa_handle, b, n, &error);
			if (res < 0) {
				fprintf(stderr, "res=%d err=%d pa_simple_write failed", res, error);
				debuglog("pa_simple_write failed\n");
			}
		}

		if (do_shutdown) {
			pa_simple_drain(pa_handle, &error);
			debuglog("consumer exit\n");
			return NULL;
		}
	}
}

int main(int argc, char** argv, char** env)
{

	bool interactive = true;
	bool write_wave_file = false;

	size_t wave_file_samples = RATE * 60; // 1 minute
	char* wave_file_data = NULL;
	size_t wave_i = 0;

	if (write_wave_file) {
		wave_fp = fopen("out.wav", "w");
		wave_file_data = malloc(wave_file_samples * sizeof(int16_t));
		write_wav_header(RATE);
	} else {
		wave_file_samples = 0;
	}

	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE, .rate = RATE, .channels = 1
	};

	int error;
	pa_handle = pa_simple_new(NULL, "synthtestbed", PA_STREAM_PLAYBACK, NULL, "playback",
	    &ss, NULL, NULL, &error);
	if (pa_handle == NULL) {
		fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n",
		    pa_strerror(error));
		assert(0);
	}

	// size_t buf_seconds = 60;

	size_t chunk = 1;

	// assert(RATE % chunk == 0);

	buf_num_samples = 10000;
	for (int i = 0; i < 2; i++) {
		buf[i] = malloc(sizeof(int16_t) * buf_num_samples);
		memset(buf[i], 0, sizeof(int16_t) * buf_num_samples);
	}
	which_buf = 0;
	buf_full = false;

	size_t key_bytes = sizeof(struct key) * MAX_KEYS;
	size_t osc_bytes = sizeof(struct osc) * NUM_OSCS * NUM_OSC_TYPES;
	keys = malloc(key_bytes);
	memset(keys, 0, key_bytes);
	for (size_t i = 0; i < MAX_KEYS; i++) {
		keys[i].oscs = malloc(osc_bytes);
	}

	memset(keys[0].oscs, 0, osc_bytes);
	if (load_patch("patch", keys[0].oscs) != 0) {
		goto shutdown;
	}
	for (size_t i = 1; i < MAX_KEYS; i++) {
		memcpy(keys[i].oscs, keys[0].oscs, osc_bytes);
	}

	if (interactive) {
		initscr();
		cbreak();
		noecho();
		keypad(stdscr, TRUE);
		// nodelay(stdscr, TRUE);
	}

	pthread_mutex_init(&the_lock, NULL);
	pthread_cond_init(&the_cond, NULL);

	pthread_t tid1, tid2;
	int i;

	the_key = 'b'; // start with a note immediately

	/* create the threads; may be any number, in general */
	if (pthread_create(&tid1, NULL, producer, NULL) != 0) {
		fprintf(stderr, "Unable to create producer thread\n");
		goto shutdown;
	}
	if (pthread_create(&tid2, NULL, consumer, NULL) != 0) {
		fprintf(stderr, "Unable to create consumer thread\n");
		goto shutdown;
	}

	// TODO call these on shutdown
	// pthread_join(tid1,NULL);
	// pthread_join(tid2,NULL);

	for (;;) {
		char ch = getch();
		if (ch == 'q') {
			goto shutdown;
		}
		pthread_mutex_lock(&the_lock);
		the_key = ch;
		pthread_mutex_unlock(&the_lock);
	}

shutdown:
	if (interactive) {
		endwin();
	}
	if (wave_fp != NULL) {
		write_wave_data(wave_file_samples * 2, wave_file_data);
		fclose(wave_fp);
		free(wave_file_data);
	}

	pthread_mutex_lock(&the_lock);
	do_shutdown = true;
	pthread_mutex_unlock(&the_lock);
	pthread_cond_signal(&the_cond);

	printf("waiting for threads before exiting\n");

	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

error:

	free(buf[0]);
	free(buf[1]);

	return 0;
}
