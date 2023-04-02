#ifndef SEARCH_MAP_H
#define SEARCH_MAP_H

#define SM_TABLE_SIZE 23u
#define SM_DEFAULT_KEY 0xFFFFFFFFu

// Error codes
#define SM_OK 0x0
#define E_SM_ALREADY_FULL 0x1

#include <stdint.h>
#include <assert.h>

#include "bitset.h"

typedef struct {
    BitSet bs;
    uint32_t key;
} SearchMapEntry;

typedef struct {
    SearchMapEntry _map[SM_TABLE_SIZE];
    uint8_t _indices[SM_TABLE_SIZE];
    uint32_t size;
} SearchMap;

uint32_t _hash(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45D9F3Bu;
    x = ((x >> 16) ^ x) * 0x45D9F3Bu;
    x = (x >> 16) ^ x;
    return x;
}

void SM_init(SearchMap *sm)
{
    for (uint32_t i = 0; i < SM_TABLE_SIZE; ++i) {
        sm->_map[i].key = SM_DEFAULT_KEY;
        sm->_indices[i] = 0;
        BS_init(&sm->_map[i].bs);
    }
    sm->size = 0;
}

int32_t SM_insert(SearchMap *sm, const uint32_t key, const uint8_t elem)
{
    assert(key != SM_DEFAULT_KEY);  // cannot insert invalid key element
    
    uint32_t index = _hash(key) % SM_TABLE_SIZE;
    
    uint32_t tried = 0;
    while (tried < SM_TABLE_SIZE &&
           sm->_map[index].key != key &&
           sm->_map[index].key != SM_DEFAULT_KEY) {
        // Linear probing
        index = (index + 1) % SM_TABLE_SIZE;
        ++tried;
    }
    
    if (tried == SM_TABLE_SIZE) {
        return E_SM_ALREADY_FULL;
    }
    // Found either empty spot or existing entry of same key -> insert
    if (sm->_map[index].key == SM_DEFAULT_KEY) {  // empty
        sm->_map[index].key = key;
        sm->_indices[sm->size++] = index;
    }
    uint8_t ans = BS_insert(&sm->_map[index].bs, elem); 
    assert(ans != BS_INVALID_ELEM);
    
    return SM_OK;
}

SearchMapEntry *SM_get(SearchMap *sm, const uint32_t i)
{
    assert(i < sm->size);
    
    return &sm->_map[sm->_indices[i]];
}

const SearchMapEntry *SM_find(const SearchMap *sm, const uint32_t key)
{
    assert(key != SM_DEFAULT_KEY);  // cannot find invalid key element
    
    uint32_t index = _hash(key) % SM_TABLE_SIZE;
    
    uint32_t tried = 0;
    while (tried < SM_TABLE_SIZE &&
           sm->_map[index].key != key &&
           sm->_map[index].key != SM_DEFAULT_KEY) {
        // Linear probing
        index = (index + 1) % SM_TABLE_SIZE;
        ++tried;
    }
    
    if (sm->_map[index].key == key)  {  // Entry found
        return &sm->_map[index];
    }
    
    return NULL;  // not found
}
#endif /* SEARCH_MAP_H */
