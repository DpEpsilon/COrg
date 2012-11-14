#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SDL/SDL.h"
#include "SDL/SDL_mixer.h"

#include "organya.h"
 
void create_tone(void *userdata, Uint8 *stream, int len);
int sampler(signed char* samples, int length, double angle);
void read_samples();

#define SINE 0
#define SQUARE 1
#define SAWTOOTH 2
#define SPECIAL 3

#define BEAT_SIZE 35
#define TUNING_NOTE 440

#define A440 45

#define TEMPERAMENT 1.0594630943592953 // pow(2.0,1.0/12.0)
#define PI 3.14159265358979323846264

#define SAMPLE_LENGTH 256
#define SAMPLES       100

signed char *audio_samples[SAMPLES];

organya_t* org;

int main(int argc, char *argv[]) {
    /* Audio Setup */
    SDL_AudioSpec *desired, *obtained;

    read_samples();

    if (argc <= 1) {
        fprintf(stderr, "Must supply filename.\n");
        return 1;
    }        
    
    org = organya_open(argv[1]);
    
    desired = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
    obtained = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
    
    desired->freq=22050;
    desired->format=AUDIO_S16LSB;
//    desired->format=AUDIO_S8;
    desired->channels=0;
    desired->samples=22050*org->wait_value/500;
    desired->callback=create_tone;
    desired->userdata=NULL;
 
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
 
	/* Open the audio device */
    if (SDL_OpenAudio(desired, obtained) < 0){
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        exit(-1);
    }
    /* desired spec is no longer needed */
    free(desired);
    desired=NULL;
 
    SDL_PauseAudio(0);
    getchar();
    SDL_PauseAudio(1);
    
    SDL_Quit();

    return EXIT_SUCCESS;
}

//#include "music.h"

int frequencies[ORG_NUM_TRACKS];
double angles[ORG_NUM_TRACKS];
int resource_upto[ORG_NUM_TRACKS];

unsigned int current_click = 0;

void create_tone(void *userdata, Uint8 *stream, int len) {
    int i, j;
    for (i = 0; i < ORG_NUM_TRACKS/2; i++) {
        track_t* cur_track = &org->tracks[i];

        if (resource_upto[i] < cur_track->num_resources - 1 &&
            current_click >= cur_track->resources[resource_upto[i]+1].start) {
            resource_upto[i]++;
        }
        
        int start = cur_track->resources[resource_upto[i]].start;
        int end = cur_track->resources[resource_upto[i]].start +
            cur_track->resources[resource_upto[i]].duration - 1;

        if (resource_upto[i] < cur_track->num_resources &&
            current_click >= start && current_click <= end) {
            if (cur_track->resources[resource_upto[i]].note != ORG_NO_CHANGE) {
                frequencies[i] = TUNING_NOTE *
                    pow(TEMPERAMENT,
                        (float)(cur_track->resources[resource_upto[i]].note - A440));
            }
        } else {
            frequencies[i] = 0;
        }
    }
    
    for(i=0; i<len; i++) {
        *stream = 0;
        for (j = 0; j < ORG_NUM_TRACKS/2; j++) {
            track_t* cur_track = &org->tracks[j];
            *stream += sampler(audio_samples[cur_track->instrument],
                               SAMPLE_LENGTH, angles[j]) *
                (float)(cur_track->resources[resource_upto[j]].volume)/254.0;
            
            angles[j] += (PI/22050)*frequencies[j]/2;
            if (angles[j] >= 2.0*PI) {
                angles[j] -=  2.0*PI;
                if (angles[j] >= 2.0*PI) {
                    angles[j] = 0; // Terrible hack,
                    // getting huge angle values for some reason
                }
            }
        }
        stream++;
    }

    current_click++;
}

int sampler(signed char* samples, int length, double angle) {
    // Could overflow the array if angle >= (2*PI)
    if ((int)(angle/(2*PI) * length) >= length) {
        return samples[length-1]/2;
    } else {
        return samples[(int)(angle/(2*PI) * length)];
    }
}

void read_samples() {
    int i;
    FILE* samp_file = fopen("orgsamp.dat", "rb");
    fseek(samp_file, 4, SEEK_CUR);
        
    for (i = 0; i < SAMPLES; i++) {
        // + 1 just in case of overflow due to dodgy floats (see sampler)
        audio_samples[i] =
            malloc(SAMPLE_LENGTH * sizeof(signed char) + 1);
        
        fread(audio_samples[i],
              sizeof(signed char), SAMPLE_LENGTH, samp_file);
    }
}
