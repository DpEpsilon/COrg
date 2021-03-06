#include <stdio.h>
#include <stdlib.h>
#include "organya.h"

organya_t* organya_open(const char* filename) {
    int t, r;
    FILE* file = fopen(filename, "rb");

    organya_t *org = malloc(sizeof(organya_t));

    fseek(file, 6, SEEK_CUR);
    fread(&org->wait_value, 2, 1, file);
    fseek(file, 2, SEEK_CUR);
    fread(&org->loop_start, 4, 1, file);
    fread(&org->loop_end, 4, 1, file);

    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        fseek(file, 2, SEEK_CUR);
        fread(&org->tracks[t].instrument, 1, 1, file);
        fseek(file, 1, SEEK_CUR);
        fread(&org->tracks[t].num_resources, 2, 1, file);
    }

    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        int loop_start_assigned = 0;
        org->tracks[t].resources =
            malloc(sizeof(resource_t) * org->tracks[t].num_resources);
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].start, 4, 1, file);
            if (!loop_start_assigned &&
                org->tracks[t].resources[r].start >= org->loop_start) {
                org->tracks[t].loop_start_resource = r;
                loop_start_assigned = 1;
            }
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].note, 1, 1, file);
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            if (org->tracks[t].resources[r].note == ORG_NO_CHANGE) {
                fseek(file, 1, SEEK_CUR);
                org->tracks[t].resources[r].duration =
                    org->tracks[t].resources[r-1].duration;
            } else {
                fread(&org->tracks[t].resources[r].duration, 1, 1, file);
            }
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].volume, 1, 1, file);
            if (org->tracks[t].resources[r].volume == ORG_NO_CHANGE) {
                org->tracks[t].resources[r].volume =
                    org->tracks[t].resources[r-1].volume;
            }
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].pan, 1, 1, file);
            if (org->tracks[t].resources[r].pan == ORG_NO_CHANGE) {
                org->tracks[t].resources[r].pan =
                    org->tracks[t].resources[r-1].pan;
            }
        }

        for (r = 0; r < org->tracks[t].num_resources; r++) {
            if (org->tracks[t].resources[r].note == ORG_NO_CHANGE) {
                org->tracks[t].resources[r].note =
                    org->tracks[t].resources[r-1].note;
            }
        }
    }
    fclose(file);
    return org;
}

void organya_delete(organya_t* to_delete) {
    int t;
    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        free(to_delete->tracks[t].resources);
    }
    free(to_delete);
}


org_session_t* organya_new_session(organya_t* org) {
    int i;
    org_session_t* sess = malloc(sizeof(org_session_t));
    sess->org = org;
    sess->current_click = 0;

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        sess->angles[i] = 0;
        sess->resource_upto[i] = 0;
    }

    return sess;
}

void organya_click_session(org_session_t* sess) {
    int i;
    sess->current_click++;
    if (sess->current_click >= sess->org->loop_end) {
        sess->current_click = sess->org->loop_start;
        for (i = 0; i < ORG_NUM_TRACKS; i++) {
            sess->resource_upto[i] =
                sess->org->tracks[i].loop_start_resource;
        }
    }

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        if (sess->resource_upto[i] <
            sess->org->tracks[i].num_resources - 1 &&
            sess->current_click >=
            sess->org->tracks[i].resources[sess->resource_upto[i]+1].start) {
            sess->resource_upto[i]++;
        }
    }
}

resource_t* organya_session_get_resource(org_session_t* sess, int track) {
    return &(sess->org->tracks[track].resources[sess->resource_upto[track]]);
}

int organya_session_track_sounding(org_session_t* sess, int track) {
    resource_t* cur_resource = organya_session_get_resource(sess, track);
    int start = cur_resource->start;
    int end = cur_resource->start + cur_resource->duration - 1;
    return sess->resource_upto[track] < sess->org->tracks[track].num_resources &&
        sess->current_click >= start && sess->current_click <= end;
}
