/*
 * Simple hashmap implementation using linear probing.
 * 
 * - Key type is unsigned int
 * - Duplicate keys are ignored during insertion
 * - Allows iterating over all elements currently contained in the
 *   hashmap
 * 
 */

#pragma once
#ifndef HASHMAP_H
#define HASHMAP_H

#define HASH_MAP_SIZE (257)  // prime
#define HASH_DEFAULT_KEY (0xFFFFFFFF)

#include <stdbool.h>

typedef struct {
    unsigned int map[HASH_MAP_SIZE];
    short unsigned int index[HASH_MAP_SIZE];
    size_t size;
} HashMap;

// From StackOverflow
unsigned int hash(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

void HashMap_init(HashMap *h) {
    h->size = 0;
    for (unsigned int i = 0; i < HASH_MAP_SIZE; ++i) {
        h->map[i] = HASH_DEFAULT_KEY;
    }
}

size_t HashMap_size(const HashMap *h) {
    return h->size;
}

unsigned int HashMap_get(const HashMap *h, size_t i) {
    return h->map[h->index[i]];
}

int HashMap_insert(HashMap *h, unsigned int key) {
    unsigned int hashIndex = hash(key) % HASH_MAP_SIZE;
    // Insert key in hash map
    if (h->map[hashIndex] == HASH_DEFAULT_KEY) {  // free
        h->map[hashIndex] = key;
        // Successfully inserted key
        h->index[h->size++] = hashIndex;
        return 0;  // ok
    } else if (h->map[hashIndex] != key) {  // occupied
        // Collision handling
        unsigned int i = 1, j;
        do {
            // Linear probing
            j = (hashIndex + i) % HASH_MAP_SIZE;
            if (h->map[j] == key) {
                // Key is already contained; do nothing
                return 0;  // ok
            }
            ++i;
        } while (h->map[j] != HASH_DEFAULT_KEY && i < HASH_MAP_SIZE);
        
        if (i < HASH_MAP_SIZE) {
            // Successfully found location for key
            h->map[j] = key;
            // Update index & size
            h->index[h->size++] = j;
            return 0;  // ok
        } else {
            return -1; // error: Hash map is full
        }
    }
    return 0;  // ok
}

bool HashMap_find(const HashMap *h, unsigned int key) {
    unsigned int hashIndex = hash(key) % HASH_MAP_SIZE;
    unsigned int i = 0, j;
    while (i < HASH_MAP_SIZE) {
        // Linear probing
        j = (hashIndex + i) % HASH_MAP_SIZE;
        if (h->map[j] == HASH_DEFAULT_KEY) {
            // Key was never inserted in the first place
            return false;
        } else if (h->map[j] == key) {
            // Key found
            return true;
        }
        ++i;
    }
    // Hash map is full and does not contain the key
    return false;
}

void HashMap_print_full(const HashMap *h) {
    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        printf("%u\n", h->map[i]);
    }
}

void HashMap_print_keys(const HashMap *h) {
    for (size_t i = 0; i < h->size; ++i) {
        printf("Key: %u, Index: %u\n", h->map[h->index[i]], h->index[i]);
    }
}

#endif /* HASHMAP_H */
