#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "organya.h"

void create_tone(void *userdata, Uint8 *stream, int len);
int sampler(signed char* samples, int length, double angle);
int drum_sampler(signed char* samples, int length, double position);
void read_samples();

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
    SDL_AudioSpec *desired, *obtained;

    read_samples();

    if (argc <= 1) {
        fprintf(stderr, "Must supply filename.\n");
        return 1;
    }

    org = organya_open(argv[1]);
    session = organya_new_session(org);

    desired = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
    obtained = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));

    desired->freq=SAMPLE_FREQUENCY;
    desired->format=AUDIO_S16SYS;
    desired->channels=2;
    desired->samples=SAMPLE_FREQUENCY*org->wait_value/1000;
    desired->callback=create_tone;
    desired->userdata=NULL;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	/* Open the audio device */
    if (SDL_OpenAudio(desired, obtained) < 0){
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
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

void read_samples() {
    int i;
    unsigned char drum_header[3];
    FILE* samp_file = fopen("orgsamp.dat", "rb");
    fseek(samp_file, 4, SEEK_CUR);

    for (i = 0; i < SAMPLES; i++) {
        audio_samples[i] =
            malloc(SAMPLE_LENGTH * sizeof(signed char));

        fread(audio_samples[i], sizeof(signed char),
              SAMPLE_LENGTH, samp_file);
    }
    fread(drum_header, 1, sizeof(drum_header), samp_file);
    drum_sample_frequency = drum_header[1] * 256 + drum_header[2];
    for (i = 0; i < NUM_DRUM_SAMPLES; i++) {

        fread(&drum_sample_lengths[i], 3, 1, samp_file);
        char swapper = *(((char*)&drum_sample_lengths[i]) + 2);

        *(((char*)&drum_sample_lengths[i]) + 2) =
            *((char*)&drum_sample_lengths[i]);

        *((char*)&drum_sample_lengths[i]) = swapper;

        drum_samples[i] = malloc(sizeof(signed char) *
                                 drum_sample_lengths[i]);

        fread(drum_samples[i], sizeof(signed char),
              drum_sample_lengths[i], samp_file);
    }
}
