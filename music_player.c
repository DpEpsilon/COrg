#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SDL/SDL.h"
#include "SDL/SDL_mixer.h"
 
void create_tone(void *userdata, Uint8 *stream, int len);
int sampler(signed char* samples, int length, double angle);
void read_samples();

#define SINE 0
#define SQUARE 1
#define SAWTOOTH 2
#define SPECIAL 3

#define BEAT_SIZE 35
//#define TUNING_NOTE 440
#define TUNING_NOTE 220
#define TEMPERAMENT 1.029302236643492 // pow(2.0,1.0/24.0)
#define PI 3.14159265358979323846264

#define SAMPLE_LENGTH 256
#define SAMPLES       100

signed char *audio_samples[SAMPLES];

int main(int argc, char *argv[]) {
    /* Audio Setup */
    SDL_AudioSpec *desired, *obtained;

    read_samples();
    
    desired = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
    obtained = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
    
    desired->freq=22050;
    desired->format=AUDIO_S16LSB;
//    desired->format=AUDIO_S8;
    desired->channels=0;
//    desired->samples=8192;
    desired->samples=2048;
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

#include "music.h"

int frequencies[NUM_TRACKS];
double angles[NUM_TRACKS];
int beat_upto[NUM_TRACKS];
int note_upto[NUM_TRACKS];

void create_tone(void *userdata, Uint8 *stream, int len) {
    int i, j;
    for (i = 0; i < NUM_TRACKS; i++) {
        
        frequencies[i] = soundPitches[c_lengths[i] + note_upto[i]] != -128 ?
            TUNING_NOTE *
            pow(TEMPERAMENT,(float)soundPitches[c_lengths[i] + note_upto[i]])
            : 0;

        beat_upto[i] += 1;
        if (beat_upto[i] >= soundDurations[c_lengths[i] + note_upto[i]]) {
            beat_upto[i] = 0;
            note_upto[i]++;
            if (note_upto[i] >= lengths[i])
                note_upto[i] = 0;
        }
    }
    
    for(i=0; i<len; i++) {
        *stream = 0;
        for (j = 0; j < NUM_TRACKS; j++) {
            /*
            switch (instrument[j]) {
            default:
            case SINE:
                *stream += 64*cos(angles[j]);
                break;
            case SQUARE:
                *stream += (angles[j] > PI ? 32 : -32);
                break;
            case SAWTOOTH:
                *stream += angles[j] / (2.0*PI) * 128 - 64;
                break;
            case SPECIAL:
                *stream += sampler(audio_samples, SAMPLE_LENGTH, angles[j]);
                break;
            }
            */

            *stream += sampler(audio_samples[instrument[j]],
                               SAMPLE_LENGTH, angles[j]);
            
            angles[j] += (PI/22050)*frequencies[j];
            if(angles[j] >= 2.0*PI) {
                angles[j] -= 2.0*PI;
            }
        }
        stream++;
    }
}

int sampler(signed char* samples, int length, double angle) {
    // Could overflow the array if angle >= (2*PI)
    return samples[(int)(angle/(2*PI) * SAMPLE_LENGTH)];
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
