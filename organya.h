#ifndef ORGANYA_H
#define ORGANYA_H

#define ORG_NUM_TRACKS 16
#define ORG_NO_CHANGE 255

typedef struct resource {
    unsigned int start;
    unsigned char note;
    unsigned char duration;
    unsigned char volume;
    unsigned char pan;
} resource_t;

typedef struct track {
    unsigned char instrument;
    unsigned short num_resources;
    unsigned int loop_start_resource; // Not assigned yet
    resource_t* resources;
} track_t;

typedef struct organya {
    unsigned short wait_value;
    unsigned int loop_start;
    unsigned int loop_end;
    track_t tracks[ORG_NUM_TRACKS];
} organya_t;

typedef struct org_session {
    double angles[ORG_NUM_TRACKS];
    unsigned short resource_upto[ORG_NUM_TRACKS];
    unsigned int current_click;
    organya_t* org;
} org_session_t;

organya_t* organya_open(const char* filename);
void organya_delete(organya_t* to_delete);

org_session_t* organya_new_session(organya_t* org);
void organya_click_session(org_session_t* sess);

#endif // ORGANYA_H
