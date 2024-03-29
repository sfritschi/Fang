/*
 * Simple Linked Lists Datastructure (FIFO)
 * 
 */

#pragma once
#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef unsigned int LL_key_t;
typedef struct LL_node_t * LL_iterator_t;

// Node within linked list
struct LL_node_t {
    struct LL_node_t *next;
    LL_key_t key;
};

// Linked list data type
typedef struct {
    struct LL_node_t *head;  // add elements at head
    struct LL_node_t *tail;  // remove elements from tail
    size_t size;
} LinkedList;

// Initialize linked list datastructure
void LL_init(LinkedList *ll) {
    ll->head = NULL;
    ll->tail = NULL;
    ll->size = 0;
}

// Fetch current size of linked list
size_t LL_size(const LinkedList *ll) {
    return ll->size;
}

LL_iterator_t LL_iterator(const LinkedList *ll) {
    return ll->tail;
}

// Insert key into linked list (into head)
void LL_insert(LinkedList *ll, LL_key_t key) {
    struct LL_node_t *new_node = 
            (struct LL_node_t *) malloc(sizeof(struct LL_node_t));
    assert(new_node != NULL);
    new_node->key = key;
    new_node->next = NULL;
    // Increment size
    ++ll->size;
    if (ll->head == NULL)  { // first insert, set tail
        ll->head = new_node;
        ll->tail = new_node;
        return;
    }
    
    ll->head->next = new_node;
    ll->head = new_node;
}

// Remove first key in linked list (from tail)
LL_key_t LL_pop(LinkedList *ll) {
    assert(ll->tail);
    
    LL_key_t key = ll->tail->key;
    struct LL_node_t *temp = ll->tail;
    ll->tail = ll->tail->next;
    free(temp);
    if (ll->tail == NULL) {
        // Head points to invalid memory address at this point
        // Set NULL
        ll->head = NULL;
    }
    // Decrement size
    --ll->size;
    return key;
}

// Print contents of linked list (assumes unsigned int as format)
void LL_print(const LinkedList *ll) {
    LL_iterator_t iter = ll->tail;
    while (iter) {
        printf("%u ", iter->key);
        iter = iter->next;
    }
    printf("\n");
}

// Free all data associated with linked list (start at tail)
void LL_free(LinkedList *ll) {
    LL_iterator_t temp;
    while (ll->tail) {
        temp = ll->tail;
        ll->tail = ll->tail->next;
        free(temp);
    }
}

#endif /* LINKED_LIST_H */
