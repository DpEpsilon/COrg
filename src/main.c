#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* Use our own main rather than SDL's, so no SDL2main is needed on Windows. */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "organya.h"

void create_tone(void *userdata, Uint8 *stream, int len);
static void render_frames(org_session_t* session, Sint16 *output,
                          int frame_count);
static void advance_click(org_session_t* session);
static unsigned int next_click_length(org_session_t* session);
static double voice_sample(const org_session_t* session, int track);
static double voice_step(const org_session_t* session, int track);
static int voice_audible(const org_session_t* session, int track);
static void voice_gains(const resource_t* resource,
                        double* left, double* right);
static void advance_voice(org_voice_t* voice, int track, double step);
static void trigger_voice(org_session_t* session, int track);
static void lowpass_init(org_lowpass_t* lowpass, double cutoff);
static double lowpass_process(org_lowpass_t* lowpass, int channel,
                              double input);
double sampler(const signed char* samples, int length, double angle);
double drum_sampler(const signed char* samples, int length,
                    double position);
int read_samples(void);
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
#define MIX_GAIN 0.7

/* Time constant, in seconds, for smoothing voice gains and decaying
   the offsets that hide waveform jumps when a voice is retriggered. */
#define DECLICK_TIME 0.002

/* Cutoff, in Hz, of the optional output low-pass filter. */
#define LOWPASS_FREQUENCY 17000.0

/* The instrument and drum sample bank, built into the binary. */
static const unsigned char orgsamp[] = {
#embed "../orgsamp.dat"
};

/* Pointers into orgsamp, set up by read_samples. */
const signed char *audio_samples[SAMPLES];

int drum_sample_lengths[NUM_DRUM_SAMPLES];
const signed char *drum_samples[NUM_DRUM_SAMPLES];
int drum_sample_frequency;

int main(int argc, char *argv[]) {
    /* Audio Setup */
    organya_t* org = NULL;
    org_session_t* session = NULL;
    const char* filename = NULL;
    int lowpass = 0;
    int i;
    int audio_open = 0;
    int sdl_initialized = 0;
    int status = EXIT_FAILURE;
    SDL_AudioSpec desired = {0};

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 ||
            strcmp(argv[i], "--lowpass") == 0) {
            lowpass = 1;
        } else if (argv[i][0] == '-' || filename != NULL) {
            filename = NULL;
            break;
        } else {
            filename = argv[i];
        }
    }
    if (filename == NULL) {
        fprintf(stderr, "Usage: %s [-l|--lowpass] FILE.org\n", argv[0]);
        goto cleanup;
    }

    if (read_samples() != 0) {
        goto cleanup;
    }

    org = organya_open(filename);
    if (org == NULL) {
        goto cleanup;
    }
    session = organya_new_session(org);
    if (session == NULL) {
        fprintf(stderr, "Could not allocate playback session.\n");
        goto cleanup;
    }
    session->frames_until_click = next_click_length(session);
    if (lowpass) {
        lowpass_init(&session->lowpass, LOWPASS_FREQUENCY);
    }

    desired.freq = SAMPLE_FREQUENCY;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = AUDIO_BUFFER_FRAMES;
    desired.callback = create_tone;
    desired.userdata = session;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = 1;

	/* Open the audio device. With no obtained spec, SDL converts from
	   the desired format to whatever the device actually uses. */
    if (SDL_OpenAudio(&desired, NULL) < 0){
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        goto cleanup;
    }
    audio_open = 1;
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

    return status;
}

void create_tone(void *userdata, Uint8 *stream, int len) {
    org_session_t* session = userdata;
    int frame_offset = 0;
    int frame_count = len / (sizeof(Sint16) * 2);
    Sint16 *output = (Sint16 *)stream;

    while (frame_offset < frame_count) {
        int frames_to_render = frame_count - frame_offset;

        if (frames_to_render > (int)session->frames_until_click) {
            frames_to_render = session->frames_until_click;
        }
        render_frames(session, output + frame_offset * 2, frames_to_render);
        frame_offset += frames_to_render;
        session->frames_until_click -= frames_to_render;
        if (session->frames_until_click == 0) {
            advance_click(session);
        }
    }
}

static void render_frames(org_session_t* session, Sint16 *output,
                          int frame_count) {
    const double decay = exp(-1.0 / (DECLICK_TIME * SAMPLE_FREQUENCY));
    double steps[ORG_NUM_TRACKS];
    double target_left[ORG_NUM_TRACKS];
    double target_right[ORG_NUM_TRACKS];
    int i, j;

    for (j = 0; j < ORG_NUM_TRACKS; j++) {
        resource_t* resource = organya_session_get_resource(session, j);

        steps[j] = target_left[j] = target_right[j] = 0.0;
        if (resource != NULL) {
            steps[j] = voice_step(session, j);
            voice_gains(resource, &target_left[j], &target_right[j]);
        }
    }

    for (i = 0; i < frame_count; i++) {
        double mixed_left = 0.0;
        double mixed_right = 0.0;

        for (j = 0; j < ORG_NUM_TRACKS; j++) {
            org_voice_t* voice = &session->voices[j];
            int audible;
            double sample;

            if (organya_session_get_resource(session, j) == NULL) {
                continue;
            }
            audible = voice_audible(session, j);
            voice->gain_left += (1.0 - decay) *
                ((audible ? target_left[j] : 0.0) - voice->gain_left);
            voice->gain_right += (1.0 - decay) *
                ((audible ? target_right[j] : 0.0) - voice->gain_right);

            sample = voice_sample(session, j) + voice->declick_offset;
            voice->declick_offset *= decay;
            voice->last_sample = sample;
            advance_voice(voice, j, steps[j]);

            mixed_left += sample * 256.0 * voice->gain_left;
            mixed_right += sample * 256.0 * voice->gain_right;
        }

        if (session->lowpass.enabled) {
            mixed_left = lowpass_process(&session->lowpass, 0, mixed_left);
            mixed_right = lowpass_process(&session->lowpass, 1, mixed_right);
        }
        output[i * 2] = clamp_sample(mixed_left * MIX_GAIN);
        output[i * 2 + 1] = clamp_sample(mixed_right * MIX_GAIN);
    }
}

static void advance_click(org_session_t* session) {
    int i;

    organya_click_session(session);
    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        resource_t* resource = organya_session_get_resource(session, i);

        if (resource != NULL && resource->triggers_note &&
            resource->start == session->current_click) {
            trigger_voice(session, i);
        }
    }
    session->frames_until_click = next_click_length(session);
}

static unsigned int next_click_length(org_session_t* session) {
    unsigned long numerator =
        (unsigned long)SAMPLE_FREQUENCY * session->org->wait_value;
    unsigned int frames = numerator / 1000;

    session->click_frame_remainder += numerator % 1000;
    if (session->click_frame_remainder >= 1000) {
        frames++;
        session->click_frame_remainder -= 1000;
    }
    return frames;
}

/* The track's waveform at the voice's current position, before gain and
   declick offset. */
static double voice_sample(const org_session_t* session, int track) {
    int instrument = session->org->tracks[track].instrument;
    double angle = session->voices[track].angle;

    if (track < 8) {
        return sampler(audio_samples[instrument], SAMPLE_LENGTH, angle);
    }
    return drum_sampler(drum_samples[instrument],
                        drum_sample_lengths[instrument], angle);
}

/* How far the voice's position moves per output frame: radians for
   melody tracks, drum sample positions for drum tracks. */
static double voice_step(const org_session_t* session, int track) {
    const track_t* cur_track = &session->org->tracks[track];
    const resource_t* resource = organya_session_get_resource(session, track);

    if (track < 8) {
        return (2 * PI / SAMPLE_FREQUENCY) *
            melody_frequency(cur_track, resource->note);
    }
    return (double)resource->note * drum_sample_frequency / SAMPLE_FREQUENCY;
}

static int voice_audible(const org_session_t* session, int track) {
    const resource_t* resource = organya_session_get_resource(session, track);

    if (track < 8) {
        double pi_limit = 4.0 * (resource->note / 12 + 1);

        return organya_session_track_sounding(session, track) &&
            !(session->org->tracks[track].pi &&
              session->voices[track].pi_cycles >= pi_limit);
    }
    return resource->start <= session->current_click;
}

/* Left and right gains for a resource's volume and pan. */
static void voice_gains(const resource_t* resource,
                        double* left, double* right) {
    double volume = resource->volume / 254.0;

    *left = volume * (resource->pan <= 6 ? 1.0 : (12 - resource->pan) / 6.0);
    *right = volume * (resource->pan >= 6 ? 1.0 : resource->pan / 6.0);
}

static void advance_voice(org_voice_t* voice, int track, double step) {
    voice->angle += step;
    if (track < 8) {
        voice->pi_cycles += step / (2.0 * PI);
        if (voice->angle >= 2.0 * PI) {
            voice->angle -= 2.0 * PI;
        }
    }
}

/* Restart the voice's waveform, carrying the jump between its last output
   and the new waveform's first sample as an offset that decays away. */
static void trigger_voice(org_session_t* session, int track) {
    org_voice_t* voice = &session->voices[track];

    voice->angle = 0.0;
    voice->pi_cycles = 0.0;
    voice->declick_offset = voice->last_sample - voice_sample(session, track);
}

/* Second-order Butterworth low-pass (RBJ cookbook biquad, Q = 1/sqrt(2)). */
static void lowpass_init(org_lowpass_t* lowpass, double cutoff) {
    double w0 = 2.0 * PI * cutoff / SAMPLE_FREQUENCY;
    double alpha = sin(w0) / sqrt(2.0);
    double a0 = 1.0 + alpha;

    memset(lowpass, 0, sizeof(*lowpass));
    lowpass->enabled = 1;
    lowpass->b0 = (1.0 - cos(w0)) / 2.0 / a0;
    lowpass->b1 = (1.0 - cos(w0)) / a0;
    lowpass->b2 = lowpass->b0;
    lowpass->a1 = -2.0 * cos(w0) / a0;
    lowpass->a2 = (1.0 - alpha) / a0;
}

static double lowpass_process(org_lowpass_t* lowpass, int channel,
                              double input) {
    double output = lowpass->b0 * input +
        lowpass->b1 * lowpass->x1[channel] +
        lowpass->b2 * lowpass->x2[channel] -
        lowpass->a1 * lowpass->y1[channel] -
        lowpass->a2 * lowpass->y2[channel];

    lowpass->x2[channel] = lowpass->x1[channel];
    lowpass->x1[channel] = input;
    lowpass->y2[channel] = lowpass->y1[channel];
    lowpass->y1[channel] = output;
    return output;
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

double sampler(const signed char* samples, int length, double angle) {
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

double drum_sampler(const signed char* samples, int length,
                    double position) {
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

/* Returns the next size bytes of orgsamp after *offset and advances
   *offset past them, or returns NULL if there are not enough left. */
static const unsigned char* take_bytes(size_t* offset, size_t size) {
    const unsigned char* bytes = orgsamp + *offset;

    if (size > sizeof(orgsamp) - *offset) {
        return NULL;
    }
    *offset += size;
    return bytes;
}

static int take_u8(size_t* offset, int* value) {
    const unsigned char* bytes = take_bytes(offset, 1);

    if (bytes == NULL) {
        return 0;
    }
    *value = bytes[0];
    return 1;
}

static int take_be16(size_t* offset, int* value) {
    const unsigned char* bytes = take_bytes(offset, 2);

    if (bytes == NULL) {
        return 0;
    }
    *value = (bytes[0] << 8) | bytes[1];
    return 1;
}

static int take_be24(size_t* offset, int* value) {
    const unsigned char* bytes = take_bytes(offset, 3);

    if (bytes == NULL) {
        return 0;
    }
    *value = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    return 1;
}

int read_samples(void) {
    int i, melody_count, melody_length, drum_count;
    size_t offset = 0;

    if (!take_u8(&offset, &melody_count) || melody_count != SAMPLES ||
        !take_be24(&offset, &melody_length) ||
        melody_length != SAMPLE_LENGTH) {
        goto invalid_data;
    }

    for (i = 0; i < SAMPLES; i++) {
        audio_samples[i] =
            (const signed char*)take_bytes(&offset, SAMPLE_LENGTH);
        if (audio_samples[i] == NULL) {
            goto invalid_data;
        }
    }

    if (!take_u8(&offset, &drum_count) || drum_count != NUM_DRUM_SAMPLES ||
        !take_be16(&offset, &drum_sample_frequency) ||
        drum_sample_frequency == 0) {
        goto invalid_data;
    }

    for (i = 0; i < NUM_DRUM_SAMPLES; i++) {
        if (!take_be24(&offset, &drum_sample_lengths[i]) ||
            drum_sample_lengths[i] == 0) {
            goto invalid_data;
        }
        drum_samples[i] =
            (const signed char*)take_bytes(&offset, drum_sample_lengths[i]);
        if (drum_samples[i] == NULL) {
            goto invalid_data;
        }
    }
    return 0;

invalid_data:
    fprintf(stderr, "Invalid or truncated built-in sample data.\n");
    return -1;
}
