/*
 * Encapsulates name of location as well as its corresponding vertex
 * number. Provides methods for quickly finding a location given its name,
 * as well reading them from file.
 * 
 */

#pragma once
#ifndef LOCATION_H
#define LOCATION_H

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MAX_LOCATION_LEN (40)
#define LOCATION_BUF_SIZE (128)
#define LOC_DIMS (2)

// Encapsulate location information (vertex of board)
typedef struct {
    char name[MAX_LOCATION_LEN];
    float pos[LOC_DIMS];
    unsigned int index;
} Location_t;

// Location comparison function
int location_cmp(const void *a, const void *b) {
    const Location_t *loc_a = (const Location_t *) a;
    const Location_t *loc_b = (const Location_t *) b;
    return strncmp(loc_a->name, loc_b->name, MAX_LOCATION_LEN * sizeof(char));
}

// Compute similarity of two strings (location names)
unsigned int location_sim(const char *a, const char *b) {
    unsigned int sim = 0, i = 0;
    while (i < MAX_LOCATION_LEN && a[i] != '\0' && b[i] != '\0') {
        if (a[i] == b[i])
            ++sim;
        ++i;
    }
    return sim;
}

// Using sorted array of locations, find closest matching location given
// a name, using binary search
unsigned int location_binsearch(const Location_t *locations_sorted,
                                    const char *location, unsigned int n) {
    int l = 0;
    int r = n - 1;
    
    unsigned int best_index = 0;
    unsigned int best_sim = 0;
    int middle;
    while (l <= r) {
        middle = (l + r) / 2;
        // Compare given location name with location in the middle
        const char *current_loc = locations_sorted[middle].name;
        unsigned int current_index = locations_sorted[middle].index;
        
        int cmp = strncmp(location, current_loc, MAX_LOCATION_LEN * sizeof(char));
        if (cmp < 0) {
            // Go left
            r = middle - 1;
        } else if (cmp > 0) {
            // Go right
            l = middle + 1;
        } else {
            // Found exact match
            return current_index;
        }
        // Update best match if current location is more 'similar'
        unsigned int sim = location_sim(location, current_loc);
        if (sim >= best_sim) {
            best_sim = sim;
            best_index = current_index;
        }
    }
    
    return best_index;
}

// Read locations from file (initialize arrays)
void read_locations(FILE *fp, Location_t *locations, 
                    Location_t *locations_sorted, unsigned int nLoc) {
    assert(fp != NULL);

    unsigned int i = 0;
    Location_t loc;

    char locationBuf[LOCATION_BUF_SIZE];
    while (fgets(locationBuf, LOCATION_BUF_SIZE, fp)) {
        
        if (i == nLoc) {
            fprintf(stderr, "Too many locations (lines) in file.\n");
            exit(EXIT_FAILURE);
        }
        loc.index = i;      
        // Parse location
        unsigned int newline = strcspn(locationBuf, "\n");
        if (newline >= strlen(locationBuf)) {
            fprintf(stderr, "Line %u is too long for buffer of size: %d.\n",
                    i+1, LOCATION_BUF_SIZE);
            exit(EXIT_FAILURE);
        }
        locationBuf[newline] = '\0';
        
        // Parse location name
        const char *val = strtok(locationBuf, ",");  assert(val);
        // Store location name
        if (strlen(val) >= MAX_LOCATION_LEN) {
            fprintf(stderr, "Location name '%s' longer than max.: %d.\n",
                    val, MAX_LOCATION_LEN);
            exit(EXIT_FAILURE);
        }
        strncpy(loc.name, val, MAX_LOCATION_LEN);
       
        // Parse position
        val = strtok(NULL, ","); assert(val);
        loc.pos[0] = atof(val);
        val = strtok(NULL, ","); assert(val);
        loc.pos[1] = atof(val);
        
        // Store location
        locations[i] = loc;
        locations_sorted[i] = loc;
        
        ++i;
    }
    
    if (i != nLoc) {
        fprintf(stderr, "Too few locations (lines) in file.\n");
        exit(EXIT_FAILURE);
    }
}

#endif /* LOCATION_H */
