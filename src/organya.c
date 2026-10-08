#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "organya.h"

static int read_bytes(FILE* file, void* destination, size_t size) {
    return fread(destination, 1, size, file) == size;
}

static int read_u8(FILE* file, unsigned char* value) {
    return read_bytes(file, value, sizeof(*value));
}

static int read_le16(FILE* file, unsigned short* value) {
    unsigned char bytes[2];

    if (!read_bytes(file, bytes, sizeof(bytes))) {
        return 0;
    }
    *value = (unsigned short)(bytes[0] | (bytes[1] << 8));
    return 1;
}

static int read_le32(FILE* file, unsigned int* value) {
    unsigned char bytes[4];

    if (!read_bytes(file, bytes, sizeof(bytes))) {
        return 0;
    }
    *value = (unsigned int)bytes[0] |
        ((unsigned int)bytes[1] << 8) |
        ((unsigned int)bytes[2] << 16) |
        ((unsigned int)bytes[3] << 24);
    return 1;
}

organya_t* organya_open(const char* filename) {
    char signature[6];
    int t, r;
    unsigned char ignored_u8;
    FILE* file = fopen(filename, "rb");
    organya_t* org;

    if (file == NULL) {
        perror(filename);
        return NULL;
    }

    org = calloc(1, sizeof(*org));
    if (org == NULL) {
        fclose(file);
        return NULL;
    }

    if (!read_bytes(file, signature, sizeof(signature)) ||
        memcmp(signature, "Org-0", 5) != 0 ||
        signature[5] < '1' || signature[5] > '3' ||
        !read_le16(file, &org->wait_value) ||
        !read_u8(file, &ignored_u8) ||
        !read_u8(file, &ignored_u8) ||
        !read_le32(file, &org->loop_start) ||
        !read_le32(file, &org->loop_end) ||
        org->wait_value == 0 || org->loop_start >= org->loop_end) {
        goto invalid_file;
    }

    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        track_t* track = &org->tracks[t];

        if (!read_le16(file, &track->frequency) ||
            !read_u8(file, &track->instrument) ||
            !read_u8(file, &track->pi) ||
            !read_le16(file, &track->num_resources) ||
            track->frequency < 100 || track->frequency > 1900 ||
            track->pi > 1 ||
            (t < 8 && track->instrument >= 100) ||
            (t >= 8 && track->instrument >= 28)) {
            goto invalid_file;
        }
    }

    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        track_t* track = &org->tracks[t];

        track->loop_start_resource = 0;
        if (track->num_resources > 0) {
            track->resources = calloc(track->num_resources,
                                      sizeof(*track->resources));
            if (track->resources == NULL) {
                goto invalid_file;
            }
        }

        for (r = 0; r < track->num_resources; r++) {
            resource_t* resource = &track->resources[r];

            if (!read_le32(file, &resource->start) ||
                (r > 0 && resource->start < track->resources[r-1].start)) {
                goto invalid_file;
            }
            if (resource->start <= org->loop_start) {
                track->loop_start_resource = r;
            }
        }

        for (r = 0; r < track->num_resources; r++) {
            if (!read_u8(file, &track->resources[r].note) ||
                (track->resources[r].note > 95 &&
                 track->resources[r].note != ORG_NO_CHANGE)) {
                goto invalid_file;
            }
            track->resources[r].triggers_note =
                track->resources[r].note != ORG_NO_CHANGE;
            track->resources[r].note_start =
                track->resources[r].triggers_note || r == 0
                ? track->resources[r].start
                : track->resources[r-1].note_start;
        }

        for (r = 0; r < track->num_resources; r++) {
            unsigned char duration;

            if (!read_u8(file, &duration)) {
                goto invalid_file;
            }
            track->resources[r].duration =
                track->resources[r].note == ORG_NO_CHANGE
                ? (r > 0 ? track->resources[r-1].duration : 0)
                : duration;
        }

        for (r = 0; r < track->num_resources; r++) {
            resource_t* resource = &track->resources[r];

            if (!read_u8(file, &resource->volume)) {
                goto invalid_file;
            }
            if (resource->volume == ORG_NO_CHANGE) {
                resource->volume = r > 0 ? track->resources[r-1].volume : 0;
            }
        }

        for (r = 0; r < track->num_resources; r++) {
            resource_t* resource = &track->resources[r];

            if (!read_u8(file, &resource->pan)) {
                goto invalid_file;
            }
            if (resource->pan == ORG_NO_CHANGE) {
                resource->pan = r > 0 ? track->resources[r-1].pan : 6;
            } else if (resource->pan > 12) {
                goto invalid_file;
            }
        }

        for (r = 0; r < track->num_resources; r++) {
            if (track->resources[r].note == ORG_NO_CHANGE) {
                track->resources[r].note = r > 0
                    ? track->resources[r-1].note
                    : 0;
            }
        }
    }

    fclose(file);
    return org;

invalid_file:
    fprintf(stderr, "Invalid or truncated Organya file: %s\n", filename);
    fclose(file);
    organya_delete(org);
    return NULL;
}

void organya_delete(organya_t* to_delete) {
    int t;

    if (to_delete == NULL) {
        return;
    }
    for (t = 0; t < ORG_NUM_TRACKS; t++) {
        free(to_delete->tracks[t].resources);
    }
    free(to_delete);
}

org_session_t* organya_new_session(organya_t* org) {
    int i;
    org_session_t* sess = malloc(sizeof(*sess));

    if (sess == NULL) {
        return NULL;
    }
    sess->org = org;
    sess->current_click = 0;

    for (i = 0; i < ORG_NUM_TRACKS; i++) {
        sess->angles[i] = 0;
        sess->resource_upto[i] = 0;
        if (i < 8) {
            sess->pi_cycles[i] = 0;
        }
    }

    return sess;
}

void organya_click_session(org_session_t* sess) {
    int i;
    sess->current_click++;
    if (sess->current_click >= sess->org->loop_end) {
        sess->current_click = sess->org->loop_start;
        for (i = 0; i < ORG_NUM_TRACKS; i++) {
            if (sess->org->tracks[i].num_resources > 0) {
                sess->resource_upto[i] =
                    sess->org->tracks[i].loop_start_resource;
            } else {
                sess->resource_upto[i] = 0;
            }
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
    if (sess->resource_upto[track] >=
        sess->org->tracks[track].num_resources) {
        return NULL;
    }
    return &(sess->org->tracks[track].resources[sess->resource_upto[track]]);
}

int organya_session_track_sounding(org_session_t* sess, int track) {
    resource_t* cur_resource = organya_session_get_resource(sess, track);
    unsigned int end;

    if (cur_resource == NULL || cur_resource->duration == 0) {
        return 0;
    }
    end = cur_resource->note_start + cur_resource->duration - 1;
    return sess->current_click >= cur_resource->note_start &&
        sess->current_click <= end;
}
