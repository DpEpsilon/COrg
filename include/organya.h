#ifndef ORGANYA_H
#define ORGANYA_H

#define ORG_NUM_TRACKS 16
#define ORG_NO_CHANGE 255

typedef struct resource {
    unsigned int start;
    unsigned int note_start;
    unsigned char note;
    unsigned char duration;
    unsigned char volume;
    unsigned char pan;
    unsigned char triggers_note;
} resource_t;

typedef struct track {
    unsigned short frequency;
    unsigned char instrument;
    unsigned char pi;
    unsigned short num_resources;
    unsigned int loop_start_resource;
    resource_t* resources;
} track_t;

typedef struct organya {
    unsigned short wait_value;
    unsigned int loop_start;
    unsigned int loop_end;
    track_t tracks[ORG_NUM_TRACKS];
} organya_t;

typedef struct org_voice {
    double angle;
    double pi_cycles;
    double gain_left;
    double gain_right;
    double last_sample;
    double declick_offset;
} org_voice_t;

typedef struct org_lowpass {
    int enabled;
    double b0, b1, b2, a1, a2;
    double x1[2], x2[2], y1[2], y2[2];
} org_lowpass_t;

typedef struct org_session {
    org_voice_t voices[ORG_NUM_TRACKS];
    org_lowpass_t lowpass;
    unsigned short resource_upto[ORG_NUM_TRACKS];
    unsigned int current_click;
    unsigned int frames_until_click;
    unsigned int click_frame_remainder;
    organya_t* org;
} org_session_t;

organya_t* organya_open(const char* filename);
void organya_delete(organya_t* to_delete);

org_session_t* organya_new_session(organya_t* org);
void organya_delete_session(org_session_t* session);
void organya_click_session(org_session_t* sess);
resource_t* organya_session_get_resource(const org_session_t* sess,
                                         int track);
int organya_session_track_sounding(const org_session_t* sess, int track);

#endif // ORGANYA_H
