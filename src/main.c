#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "organya.h"

void create_tone(void *userdata, Uint8 *stream, int len);
int sampler(signed char* samples, int length, double angle);
int drum_sampler(signed char* samples, int length, double position);
int read_samples(void);
double melody_frequency(const track_t* track, unsigned char note);
int clamp_sample(int sample);

#define BEAT_SIZE 35
#define TUNING_NOTE 440

#define A440 45

#define SAMPLE_FREQUENCY (22050*2)
#define TEMPERAMENT 1.0594630943592953 /* = 2^(1/12) */
#define PI 3.14159265358979323846264

#define SAMPLE_LENGTH     256
#define SAMPLES           100
#define NUM_DRUM_SAMPLES  28

signed char *audio_samples[SAMPLES];

int drum_sample_lengths[NUM_DRUM_SAMPLES];
signed char *drum_samples[NUM_DRUM_SAMPLES];
int drum_sample_frequency;

organya_t* org;
org_session_t* session;

int main(int argc, char *argv[]) {
    /* Audio Setup */
    unsigned int samples_per_click;
    SDL_AudioSpec desired = {0};
    SDL_AudioSpec obtained = {0};

    if (argc <= 1) {
        fprintf(stderr, "Must supply filename.\n");
        return 1;
    }

    if (read_samples() != 0) {
        return 1;
    }

    org = organya_open(argv[1]);
    if (org == NULL) {
        return 1;
    }
    session = organya_new_session(org);
    if (session == NULL) {
        fprintf(stderr, "Could not allocate playback session.\n");
        return 1;
    }

    samples_per_click =
        (unsigned int)SAMPLE_FREQUENCY * org->wait_value / 1000;
    if (samples_per_click == 0 || samples_per_click > 65535) {
        fprintf(stderr, "Organya wait value is out of range.\n");
        return 1;
    }

    desired.freq = SAMPLE_FREQUENCY;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = samples_per_click;
    desired.callback = create_tone;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

	/* Open the audio device */
    if (SDL_OpenAudio(&desired, &obtained) < 0){
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (obtained.freq != desired.freq ||
        obtained.format != desired.format ||
        obtained.channels != desired.channels) {
        fprintf(stderr, "SDL opened an unsupported audio format.\n");
        SDL_CloseAudio();
        SDL_Quit();
        return 1;
    }
    SDL_PauseAudio(0);
    getchar();
    SDL_PauseAudio(1);

    SDL_Quit();

    return EXIT_SUCCESS;
}

void create_tone(void *userdata, Uint8 *stream, int len) {
    int i, j;
    int frame_count = len / (sizeof(Sint16) * 2);
    double frequencies[8] = {0};
    Sint16 *output = (Sint16 *)stream;

    (void)userdata;

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        track_t* track = &org->tracks[i];
        resource_t* cur_resource =
            organya_session_get_resource(session, i);

        if (cur_resource == NULL) {
            continue;
        }

        if (cur_resource->triggers_note &&
            cur_resource->start == session->current_click) {
            session->angles[i] = 0.0;
            if (i < 8) {
                session->pi_cycles[i] = 0.0;
            }
        }

        if (i < 8 && organya_session_track_sounding(session, i)) {
            frequencies[i] = melody_frequency(track, cur_resource->note);
        }
    }

    for(i = 0; i < frame_count; i++) {
        int mixed_left = 0;
        int mixed_right = 0;

        for (j = 0; j < ORG_NUM_TRACKS; j++) {
            double left_gain, right_gain;
            int track_sample;
            track_t* cur_track = &org->tracks[j];
            resource_t* cur_resource =
                organya_session_get_resource(session, j);
            if (cur_resource == NULL) {
                continue;
            }
            if (j < 8) {
                double pi_limit = 4.0 * (cur_resource->note / 12 + 1);

                if (!organya_session_track_sounding(session, j) ||
                    (cur_track->pi && session->pi_cycles[j] >= pi_limit)) {
                    continue;
                }
                track_sample =
                    sampler(audio_samples[cur_track->instrument],
                            SAMPLE_LENGTH, session->angles[j]);
                session->angles[j] +=
                    (2 * PI / SAMPLE_FREQUENCY) * frequencies[j];
                if (cur_track->pi) {
                    session->pi_cycles[j] += frequencies[j] / SAMPLE_FREQUENCY;
                }

                if (session->angles[j] >= 2.0*PI) {
                    session->angles[j] -=  2.0*PI;
                }

            } else if (cur_resource->start <= session->current_click) {
                track_sample =
                    drum_sampler(drum_samples[cur_track->instrument],
                                 drum_sample_lengths[cur_track->instrument],
                                 session->angles[j]);
                session->angles[j] +=
                    (double)cur_resource->note * drum_sample_frequency /
                    SAMPLE_FREQUENCY;
            } else {
                continue;
            }

            left_gain = cur_resource->pan <= 6
                ? 1.0 : (12 - cur_resource->pan) / 6.0;
            right_gain = cur_resource->pan >= 6
                ? 1.0 : cur_resource->pan / 6.0;
            mixed_left += track_sample * cur_resource->volume /
                254.0 * left_gain;
            mixed_right += track_sample * cur_resource->volume /
                254.0 * right_gain;
        }

        output[i * 2] = clamp_sample(mixed_left) * 256;
        output[i * 2 + 1] = clamp_sample(mixed_right) * 256;
    }

    organya_click_session(session);
}

double melody_frequency(const track_t* track, unsigned char note) {
    static const int wave_sizes[8] = {256, 256, 128, 128, 64, 32, 16, 8};
    double frequency = TUNING_NOTE *
        pow(TEMPERAMENT, (double)(note - A440));

    return frequency +
        ((int)track->frequency - 1000) / (double)wave_sizes[note / 12];
}

int clamp_sample(int sample) {
    if (sample > 127) {
        return 127;
    }
    if (sample < -128) {
        return -128;
    }
    return sample;
}

int sampler(signed char* samples, int length, double angle) {
    int start_sample = (int)(angle/(2*PI) * length);
    int next_sample;
    double leftover = (angle/(2*PI) * length) - start_sample;
    if (start_sample >= length) {
        return 0;
    }

    next_sample = (start_sample + 1) % length;
    return samples[start_sample] +
        (samples[next_sample] - samples[start_sample]) * leftover;
}

int drum_sampler(signed char* samples, int length, double position) {
    int start_sample = (int)position;
    double leftover = position - start_sample;
    if (start_sample >= length) {
        return 0;
    }
    if (start_sample + 1 >= length) {
        return samples[start_sample];
    }
    return samples[start_sample] +
        (samples[start_sample+1] - samples[start_sample]) * leftover;
}

static int read_sample_bytes(FILE* file, void* destination, size_t size) {
    return fread(destination, 1, size, file) == size;
}

static int read_be16(FILE* file, int* value) {
    unsigned char bytes[2];

    if (!read_sample_bytes(file, bytes, sizeof(bytes))) {
        return 0;
    }
    *value = (bytes[0] << 8) | bytes[1];
    return 1;
}

static int read_be24(FILE* file, int* value) {
    unsigned char bytes[3];

    if (!read_sample_bytes(file, bytes, sizeof(bytes))) {
        return 0;
    }
    *value = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    return 1;
}

int read_samples(void) {
    int i, melody_count, melody_length, drum_count;
    FILE* samp_file = fopen("orgsamp.dat", "rb");

    if (samp_file == NULL) {
        perror("orgsamp.dat");
        return -1;
    }
    melody_count = fgetc(samp_file);
    if (melody_count != SAMPLES ||
        !read_be24(samp_file, &melody_length) ||
        melody_length != SAMPLE_LENGTH) {
        goto invalid_file;
    }

    for (i = 0; i < SAMPLES; i++) {
        audio_samples[i] = malloc(SAMPLE_LENGTH * sizeof(*audio_samples[i]));
        if (audio_samples[i] == NULL ||
            !read_sample_bytes(samp_file, audio_samples[i], SAMPLE_LENGTH)) {
            goto invalid_file;
        }
    }

    drum_count = fgetc(samp_file);
    if (drum_count != NUM_DRUM_SAMPLES ||
        !read_be16(samp_file, &drum_sample_frequency) ||
        drum_sample_frequency == 0) {
        goto invalid_file;
    }

    for (i = 0; i < NUM_DRUM_SAMPLES; i++) {
        if (!read_be24(samp_file, &drum_sample_lengths[i]) ||
            drum_sample_lengths[i] == 0) {
            goto invalid_file;
        }
        drum_samples[i] = malloc(drum_sample_lengths[i] *
                                 sizeof(*drum_samples[i]));
        if (drum_samples[i] == NULL ||
            !read_sample_bytes(samp_file, drum_samples[i],
                               drum_sample_lengths[i])) {
            goto invalid_file;
        }
    }
    fclose(samp_file);
    return 0;

invalid_file:
    fprintf(stderr, "Invalid or truncated sample file: orgsamp.dat\n");
    fclose(samp_file);
    for (i = 0; i < SAMPLES; i++) {
        free(audio_samples[i]);
        audio_samples[i] = NULL;
    }
    for (i = 0; i < NUM_DRUM_SAMPLES; i++) {
        free(drum_samples[i]);
        drum_samples[i] = NULL;
    }
    return -1;
}
