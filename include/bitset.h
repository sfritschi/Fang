#ifndef BITSET_H
#define BITSET_H

#include <stdint.h>
#include <assert.h>

#define BS_MAX_ELEMS 8 
#define BS_INVALID_ELEM (BS_MAX_ELEMS)

typedef struct {
    uint8_t _set;
    uint8_t size;      // current size of set
    uint8_t _counter;  // number of already visited set bits 
    uint8_t _cursor;   // current searching position in set
} BitSet;

void BS_init(BitSet *bs)
{
    bs->_set     = 0;
    bs->size     = 0;
    bs->_counter = 0;
    bs->_cursor  = 0;
}

uint8_t BS_insert(BitSet *bs, const uint8_t i)
{
    assert(i < BS_MAX_ELEMS);
    
    const uint8_t j = 1 << i;
    // Check if already contained
    if (bs->_set & j)
        return BS_INVALID_ELEM;
    
    bs->_set |= j;
    bs->size += 1;
    return i;
}

uint8_t BS_nextPos(BitSet *bs)
{
    if (bs->size == 0)
        return BS_INVALID_ELEM;
        
    for (uint8_t i = bs->_cursor; i < BS_MAX_ELEMS; ++i) {
        if (bs->_set & (1 << i)) {
            // Update cursor position
            bs->_cursor = i + 1;
            // Check if all elements have been found
            if (++bs->_counter == bs->size) {
                // Reset
                bs->_counter = 0;
                bs->_cursor  = 0;
            }
            return i;
        }
    }
    return BS_INVALID_ELEM;
}

#endif /* BITSET_H */
