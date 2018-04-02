#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SDL/SDL.h"
#include "SDL/SDL_mixer.h"

#include "organya.h"

void create_tone(void *userdata, Uint8 *stream, int len);
int sampler(signed char* samples, int length, double angle);
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
    desired->format=AUDIO_S16LSB;
    desired->channels=0;
    desired->samples=SAMPLE_FREQUENCY*org->wait_value/500;
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
    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        resource_t* cur_resource =
            organya_session_get_resource(session, i);

        if (i >= 8 && cur_resource->start == session->current_click) {
            angles[i] = 0.0;
        }

        if (organya_session_track_sounding(session, i) || (i >= 8 && cur_resource->start <= session->current_click)) {
            frequencies[i] = TUNING_NOTE *
                pow(TEMPERAMENT, (float)(cur_resource->note - A440 + (i >= 8 ? DRUM_PITCH_OFFSET : 0)));
        } else {
            frequencies[i] = 0;
        }
    }

    for(i=0; i<len; i++) {
        *stream = 0;
        for (j = 0; j < ORG_NUM_TRACKS; j++) {
            track_t* cur_track = &org->tracks[j];
            resource_t* cur_resource =
                organya_session_get_resource(session, j);
            if (j < 8) {
                int new_value = (signed char)(*stream) +
                    sampler(audio_samples[cur_track->instrument],
                            SAMPLE_LENGTH, angles[j]) *
                    (float)(cur_resource->volume)/254.0;
                if (new_value > 127) {
                    new_value = 127;
                } else if (new_value <= -128) {
                    new_value = -128;
                }

                *stream = new_value;
                angles[j] += (PI/SAMPLE_FREQUENCY)*frequencies[j]/2;

                if (angles[j] >= 2.0*PI) {
                    angles[j] -=  2.0*PI;
                }

            } else {
                int new_value = (signed char)(*stream) +
                    sampler(drum_samples[cur_track->instrument],
                            drum_sample_lengths[cur_track->instrument],
                            angles[j]) *
                    (float)(cur_resource->volume)/254.0;

                if (new_value > 127) {
                    new_value = 127;
                } else if (new_value <= -128) {
                    new_value = -128;
                }

                *stream = new_value;

                angles[j] += (PI/SAMPLE_FREQUENCY)*frequencies[j]/128;

            }
        }
        stream++;
    }

    organya_click_session(session);
}

int sampler(signed char* samples, int length, double angle) {
    int start_sample = (int)(angle/(2*PI) * length);
    double leftover = (angle/(2*PI) * length) - start_sample;
    if (start_sample >= length) {
        return 0;
    }

    double interpolated_sample;
    if (start_sample + 1 == length) {
        interpolated_sample = samples[start_sample] +
            (samples[start_sample+1]-samples[start_sample])*leftover;
    } else {
        interpolated_sample = samples[start_sample];
    }

    return (int)(interpolated_sample);
}

void read_samples() {
    int i;
    FILE* samp_file = fopen("orgsamp.dat", "rb");
    fseek(samp_file, 4, SEEK_CUR);

    for (i = 0; i < SAMPLES; i++) {
        audio_samples[i] =
            malloc(SAMPLE_LENGTH * sizeof(signed char));

        fread(audio_samples[i], sizeof(signed char),
              SAMPLE_LENGTH, samp_file);
    }
    fseek(samp_file, 3, SEEK_CUR);
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
