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
        org->tracks[t].resources =
            malloc(sizeof(resource_t) * org->tracks[t].num_resources);
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].start, 4, 1, file);
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
    sess->current_tick = 0;
    
    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        sess->angles[i] = 0;
        sess->resource_upto[i] = 0;
    }
}

void organya_click_session(org_session_t* sess) {
    int i;
    sess->current_click++;
    if (sess->current_click >= sess->org->loop_end) {
        sess->current_click = sess->org->loop_start;
        for (i = 0; i < ORG_NUM_TRACKS/2; i++) {
            sess->resource_upto[i] =
                sess->org->tracks[i].loop_start_resource;
        }
    }
}
