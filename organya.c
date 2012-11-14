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
            if (org->tracks[t].resources[r].note == 255) {
                fseek(file, 1, SEEK_CUR);
                org->tracks[t].resources[r].duration =
                    org->tracks[t].resources[r-1].duration;
            } else {
                fread(&org->tracks[t].resources[r].duration, 1, 1, file);
            }
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].volume, 1, 1, file);
        }
        for (r = 0; r < org->tracks[t].num_resources; r++) {
            fread(&org->tracks[t].resources[r].pan, 1, 1, file);
        }
    }
    
    return org;
}
