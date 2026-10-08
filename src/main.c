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

#define BEAT_SIZE 35
#define TUNING_NOTE 440

#define A440 45

#define SAMPLE_FREQUENCY (22050*2)
#define TEMPERAMENT 1.0594630943592953 /* = 2^(1/12) */
#define PI 3.14159265358979323846264

#define SAMPLE_LENGTH     256
#define SAMPLES           100
#define NUM_DRUM_SAMPLES  28

#define DRUM_PITCH_OFFSET (0)

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

int frequencies[ORG_NUM_TRACKS];
double angles[ORG_NUM_TRACKS];

unsigned int current_click = 0;

void create_tone(void *userdata, Uint8 *stream, int len) {
    int i, j;
    int frame_count = len / (sizeof(Sint16) * 2);
    Sint16 *output = (Sint16 *)stream;

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        resource_t* cur_resource =
            organya_session_get_resource(session, i);

        if (cur_resource == NULL) {
            frequencies[i] = 0;
            continue;
        }

        if (i >= 8 && cur_resource->note != ORG_NO_CHANGE &&
            cur_resource->start == session->current_click) {
            angles[i] = 0.0;
        }

        if (organya_session_track_sounding(session, i) || (i >= 8 && cur_resource->start <= session->current_click)) {
            frequencies[i] = TUNING_NOTE *
                pow(TEMPERAMENT, (float)(cur_resource->note - A440 + (i >= 8 ? DRUM_PITCH_OFFSET : 0)));
        } else {
            frequencies[i] = 0;
        }
    }

    for(i = 0; i < frame_count; i++) {
        int mixed_sample = 0;

        for (j = 0; j < ORG_NUM_TRACKS; j++) {
            int track_sample;
            track_t* cur_track = &org->tracks[j];
            resource_t* cur_resource =
                organya_session_get_resource(session, j);
            if (cur_resource == NULL) {
                continue;
            }
            if (j < 8) {
                track_sample =
                    sampler(audio_samples[cur_track->instrument],
                            SAMPLE_LENGTH, angles[j]);
                angles[j] += (2 * PI / SAMPLE_FREQUENCY) * frequencies[j];

                if (angles[j] >= 2.0*PI) {
                    angles[j] -=  2.0*PI;
                }

            } else if (cur_resource->start <= session->current_click) {
                track_sample =
                    drum_sampler(drum_samples[cur_track->instrument],
                                 drum_sample_lengths[cur_track->instrument],
                                 angles[j]);
                angles[j] +=
                    (double)cur_resource->note * drum_sample_frequency /
                    SAMPLE_FREQUENCY;
            } else {
                continue;
            }

            mixed_sample +=
                track_sample * (float)(cur_resource->volume) / 254.0;
            if (mixed_sample > 127) {
                mixed_sample = 127;
            } else if (mixed_sample <= -128) {
                mixed_sample = -128;
            }
        }

        output[i * 2] = mixed_sample * 256;
        output[i * 2 + 1] = mixed_sample * 256;
    }

    organya_click_session(session);
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
