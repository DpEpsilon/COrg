#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "organya.h"

void create_tone(void *userdata, Uint8 *stream, int len);
static void render_frames(Sint16 *output, int frame_count);
static void start_click(void);
static unsigned int next_click_length(void);
int sampler(signed char* samples, int length, double angle);
int drum_sampler(signed char* samples, int length, double position);
int read_samples(void);
void free_samples(void);
double melody_frequency(const track_t* track, unsigned char note);
Sint16 clamp_sample(double sample);

#define TUNING_NOTE 440

#define A440 45

#define SAMPLE_FREQUENCY (22050*2)
#define TEMPERAMENT 1.0594630943592953 /* = 2^(1/12) */
#define PI 3.14159265358979323846264

#define SAMPLE_LENGTH     256
#define SAMPLES           100
#define NUM_DRUM_SAMPLES  28
#define AUDIO_BUFFER_FRAMES  1024

/* Master gain applied to the 16-bit mix before clamping. */
#define MIX_GAIN 1.0

signed char *audio_samples[SAMPLES];

int drum_sample_lengths[NUM_DRUM_SAMPLES];
signed char *drum_samples[NUM_DRUM_SAMPLES];
int drum_sample_frequency;

organya_t* org;
org_session_t* session;
static unsigned int frames_until_click;
static unsigned int click_frame_remainder;
static int click_started;

int main(int argc, char *argv[]) {
    /* Audio Setup */
    int audio_open = 0;
    int sdl_initialized = 0;
    int status = EXIT_FAILURE;
    SDL_AudioSpec desired = {0};
    SDL_AudioSpec obtained = {0};

    if (argc <= 1) {
        fprintf(stderr, "Must supply filename.\n");
        goto cleanup;
    }

    if (read_samples() != 0) {
        goto cleanup;
    }

    org = organya_open(argv[1]);
    if (org == NULL) {
        goto cleanup;
    }
    session = organya_new_session(org);
    if (session == NULL) {
        fprintf(stderr, "Could not allocate playback session.\n");
        goto cleanup;
    }

    desired.freq = SAMPLE_FREQUENCY;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = AUDIO_BUFFER_FRAMES;
    desired.callback = create_tone;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = 1;

	/* Open the audio device */
    if (SDL_OpenAudio(&desired, &obtained) < 0){
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        goto cleanup;
    }
    audio_open = 1;
    if (obtained.freq != desired.freq ||
        obtained.format != desired.format ||
        obtained.channels != desired.channels) {
        fprintf(stderr, "SDL opened an unsupported audio format.\n");
        goto cleanup;
    }
    SDL_PauseAudio(0);
    getchar();
    SDL_PauseAudio(1);
    status = EXIT_SUCCESS;

cleanup:
    if (audio_open) {
        SDL_CloseAudio();
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
    organya_delete_session(session);
    session = NULL;
    organya_delete(org);
    org = NULL;
    free_samples();

    return status;
}

void create_tone(void *userdata, Uint8 *stream, int len) {
    int frame_offset = 0;
    int frame_count = len / (sizeof(Sint16) * 2);
    Sint16 *output = (Sint16 *)stream;

    (void)userdata;

    while (frame_offset < frame_count) {
        int frames_to_render;

        if (frames_until_click == 0) {
            start_click();
            frames_until_click = next_click_length();
        }
        frames_to_render = frame_count - frame_offset;
        if (frames_to_render > (int)frames_until_click) {
            frames_to_render = frames_until_click;
        }

        render_frames(output + frame_offset * 2, frames_to_render);
        frame_offset += frames_to_render;
        frames_until_click -= frames_to_render;
    }
}

static void render_frames(Sint16 *output, int frame_count) {
    int i, j;
    double frequencies[8] = {0};

    for (i = 0; i < 8; i++) {
        track_t* track = &org->tracks[i];
        resource_t* cur_resource =
            organya_session_get_resource(session, i);

        if (cur_resource != NULL &&
            organya_session_track_sounding(session, i)) {
            frequencies[i] = melody_frequency(track, cur_resource->note);
        }
    }

    for(i = 0; i < frame_count; i++) {
        double mixed_left = 0.0;
        double mixed_right = 0.0;

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
            mixed_left += track_sample * 256.0 * cur_resource->volume /
                254.0 * left_gain;
            mixed_right += track_sample * 256.0 * cur_resource->volume /
                254.0 * right_gain;
        }

        output[i * 2] = clamp_sample(mixed_left * MIX_GAIN);
        output[i * 2 + 1] = clamp_sample(mixed_right * MIX_GAIN);
    }
}

static void start_click(void) {
    int i;

    if (click_started) {
        organya_click_session(session);
    } else {
        click_started = 1;
    }

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        resource_t* cur_resource =
            organya_session_get_resource(session, i);

        if (cur_resource != NULL && cur_resource->triggers_note &&
            cur_resource->start == session->current_click) {
            session->angles[i] = 0.0;
            if (i < 8) {
                session->pi_cycles[i] = 0.0;
            }
        }
    }
}

static unsigned int next_click_length(void) {
    unsigned long numerator =
        (unsigned long)SAMPLE_FREQUENCY * org->wait_value;
    unsigned int frames = numerator / 1000;

    click_frame_remainder += numerator % 1000;
    if (click_frame_remainder >= 1000) {
        frames++;
        click_frame_remainder -= 1000;
    }
    return frames;
}

double melody_frequency(const track_t* track, unsigned char note) {
    static const int wave_sizes[8] = {256, 256, 128, 128, 64, 32, 16, 8};
    double frequency = TUNING_NOTE *
        pow(TEMPERAMENT, (double)(note - A440));

    return frequency +
        ((int)track->frequency - 1000) / (double)wave_sizes[note / 12];
}

Sint16 clamp_sample(double sample) {
    if (sample > 32767.0) {
        return 32767;
    }
    if (sample < -32768.0) {
        return -32768;
    }
    return (Sint16)lround(sample);
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
    free_samples();
    return -1;
}

void free_samples(void) {
    int i;

    for (i = 0; i < SAMPLES; i++) {
        free(audio_samples[i]);
        audio_samples[i] = NULL;
    }
    for (i = 0; i < NUM_DRUM_SAMPLES; i++) {
        free(drum_samples[i]);
        drum_samples[i] = NULL;
        drum_sample_lengths[i] = 0;
    }
    drum_sample_frequency = 0;
}
