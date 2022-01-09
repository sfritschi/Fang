/*
 * Simple Graph Datastructure using Adjacency List Representation 
 * 
 * Supports: (for both directed and undirected UNWEIGHTED graphs)
 * - BFS shortest paths for single vertex
 * - BFS shortest paths for all pairs of vertices using OMP
 * - DFS reachability for source target pair and fixed distance
 * - DFS Hash Map containing all (unique) vertices reachable from source
 *   using fixed number of steps
 * 
 * Depends on:
 * - Linked List datastructure (FIFO)
 * - Hash Map datastructure
 */

#pragma once
#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <omp.h>

#include "linked_list.h"
#include "hashmap.h"

enum GRAPH_TYPE {
    GRAPH_DIRECTED,
    GRAPH_UNDIRECTED
};

typedef struct {
    LinkedList *adjList;
    unsigned int nVert;
    unsigned int nEdge;
    enum GRAPH_TYPE type; 
} Graph;

void Graph_init_file(Graph *, FILE *);
void Graph_BFS_SP(const Graph *, unsigned int, int *, int *);
void Graph_BFS_APSP(const Graph *, int *, int *);
void Graph_DFS_reachable(const Graph *, unsigned int , unsigned int, int,
                                    bool *, int *, bool *);
bool Graph_is_reachable(const Graph *, unsigned int, unsigned int, int,
                                  bool **, int **);
void Graph_DFS_reachable_pos(const Graph *, unsigned int, int, 
                                bool *, int *, HashMap *);
HashMap Graph_reachable_pos(const Graph *, unsigned int, int, bool **, int **);
void Graph_free(Graph *);

void Graph_init_file(Graph *graph, FILE *fp) {
    assert(fp != NULL);
    
    enum GRAPH_TYPE type;
    char graph_type;
    assert(fscanf(fp, "%c", &graph_type) != EOF);
    
    if (graph_type == 'd') {
        type = GRAPH_DIRECTED;
    } else if (graph_type == 'u') {
        type = GRAPH_UNDIRECTED;
    } else {
        fprintf(stderr, "Unrecognized graph type: %c\n", graph_type);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
        
    unsigned int nVertices;
    assert(fscanf(fp, "%u", &nVertices) != EOF);
    
    graph->adjList = (LinkedList *) malloc(nVertices * sizeof(LinkedList));
    assert(graph->adjList != NULL);
    
    // Initialize individual linked lists
    for (unsigned int i = 0; i < nVertices; ++i) {
        LL_init(&graph->adjList[i]);
    }
    graph->nVert = nVertices;
    graph->nEdge = 0;
    graph->type = type;
    
    unsigned int from_node;
    unsigned int to_node;
    // Initialize graph by parsing file
    while ((fscanf(fp, "%u %u", &from_node, &to_node)) != EOF) {
        if (from_node >= nVertices || to_node >= nVertices) {
            fprintf(stderr, "Invalid edge at line: %u\n", graph->nEdge + 1);
            Graph_free(graph);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        
        if (type == GRAPH_DIRECTED) {
            // Insert 'to node' in adjacency list of 'from node'
            LL_insert(&graph->adjList[from_node], to_node);
        } else { // Symmetric (undirected)
            LL_insert(&graph->adjList[from_node], to_node);
            LL_insert(&graph->adjList[to_node], from_node);
        }
        ++graph->nEdge;
    }
}

void Graph_BFS_SP(const Graph *graph, unsigned int source, int *distances, int *parents) {
    assert(source < graph->nVert);

    bool *visited = (bool *) calloc(graph->nVert, sizeof(bool));
    assert(visited != NULL);
    
    // Initialize distances & parents array
    for (unsigned int i = 0; i < graph->nVert; ++i) {
        distances[i] = -1;
        parents[i] = -1;
    }
    // Initialize arrays for source
    visited[source] = true;
    distances[source] = 0;
    
    LinkedList searchList;
    LL_init(&searchList);
    LL_insert(&searchList, source);
    
    while (LL_size(&searchList) != 0) {
        unsigned int current = LL_pop(&searchList);
        assert(current != LL_INVALID_KEY);
        
        // Get iterator for adjacency list
        LL_iterator_t iter = LL_iterator(&graph->adjList[current]);
        
        while (iter != NULL) {
            unsigned int neighbor = iter->key;
            if (!visited[neighbor]) {
                // Visited current neighbor vertex
                visited[neighbor] = true;
                // Update distance from previous 'parent' vertex
                distances[neighbor] = distances[current] + 1;
                // Update parent
                parents[neighbor] = current;
                // Add to search list
                LL_insert(&searchList, neighbor);
            }
            // Move on to next neighbor
            iter = iter->next;
        }
    }
    
    // Cleanup
    free(visited);
    // Technically not necessary, since already empty
    LL_free(&searchList);    
}

void Graph_BFS_APSP(const Graph *graph, int *distances, int *parents) {
    // Run BFS for every vertex as source in parallel
    #pragma omp parallel for num_threads(4)
    for (unsigned int source = 0; source < graph->nVert; ++source) {
        unsigned int offset = source * graph->nVert;
        Graph_BFS_SP(graph, source, &distances[offset], &parents[offset]);
    }
}

void Graph_DFS_reachable_pos(const Graph *graph, unsigned int u,
                 int distance, bool *visited_buf, int *distances_buf, 
                     HashMap *reachableVert) {
    // Visit current vertex
    visited_buf[u] = true;
    
    if (distances_buf[u] == distance) {
        // Add to list
        HashMap_insert(reachableVert, u);
        // Backtrack
        visited_buf[u] = false;
        return;
    }
    
    // Get iterator for adjacency list
    LL_iterator_t iter = LL_iterator(&graph->adjList[u]);
    
    while (iter != NULL) {
        unsigned int neighbor = iter->key;
        // Check if cycle is reached; move on to next neighbor
        if (visited_buf[neighbor]) {
            iter = iter->next;
            continue;
        }
        // Update distance
        distances_buf[neighbor] = distances_buf[u] + 1;
        // Check if already past distance limit
        if (distances_buf[neighbor] <= distance) {
            // Recursion
            Graph_DFS_reachable_pos(graph, neighbor, distance, visited_buf, 
                        distances_buf, reachableVert);
        }
        // Go to next neighbor in adjacency list
        iter = iter->next;
    }
    // Backtrack
    visited_buf[u] = false;
}

HashMap Graph_reachable_pos(const Graph *graph, unsigned int source,
                                     int distance, bool **visited_buf,
                                       int **distances_buf) {
    assert(source < graph->nVert);
    assert(visited_buf != NULL && distances_buf != NULL);
    assert(distance >= 0);
    
    // Allocate buffer (workspace) if not allocated already
    if (*visited_buf == NULL) {
        *visited_buf = (bool *) malloc(graph->nVert * sizeof(bool));
        assert(*visited_buf != NULL);
    }
    if (*distances_buf == NULL) {
        *distances_buf = (int *) malloc(graph->nVert * sizeof(int));
        assert(*distances_buf != NULL);
    }
    
    // (Re-)initialize workspace
    unsigned int i;
    for (i = 0; i < graph->nVert; ++i) {
        (*visited_buf)[i] = false;
        (*distances_buf)[i] = -1;
    }
    // Initialize arrays for source
    (*distances_buf)[source] = 0;
    
    // Initialize linked-list containing reachable vertices
    HashMap reachableVert;
    HashMap_init(&reachableVert);
    
    Graph_DFS_reachable_pos(graph, source, distance, *visited_buf, 
                                *distances_buf, &reachableVert);
    
    return reachableVert;
}

void Graph_DFS_reachable(const Graph *graph, unsigned int u,
                 unsigned int v, int distance,
                   bool *visited_buf, int *distances_buf, 
                     bool *is_reachable) {
    if (*is_reachable) {
        return;  // Simple path already found; terminate
    }
    // Visit current vertex
    visited_buf[u] = true;
    
    if (u == v) {
        // Completed a valid simple path; check for prescribed length
        *is_reachable = distances_buf[v] == distance;
        // Backtrack
        visited_buf[u] = false;
        return;
    }
    
    // Get iterator for adjacency list
    LL_iterator_t iter = LL_iterator(&graph->adjList[u]);
    
    while (iter != NULL) {
        unsigned int neighbor = iter->key;
        // Check if cycle is reached; move on to next neighbor
        if (visited_buf[neighbor]) {
            iter = iter->next;
            continue;
        }
        // Update distance
        distances_buf[neighbor] = distances_buf[u] + 1;
        // Check if already past distance limit
        if (neighbor == v || distances_buf[neighbor] != distance) {
            // Recursion
            Graph_DFS_reachable(graph, neighbor, v, distance, visited_buf, 
                        distances_buf, is_reachable);
        }
        // Go to next neighbor in adjacency list
        iter = iter->next;
    }
    // Backtrack
    visited_buf[u] = false;
}

// Determine if there exists a simple path from source to distance of
// length exactly distance
bool Graph_is_reachable(const Graph *graph, unsigned int source,
                                unsigned int target, int distance,
                                  bool **visited_buf, int **distances_buf) {
    assert(source < graph->nVert && target < graph->nVert);
    assert(visited_buf != NULL && distances_buf != NULL);
    assert(distance >= 0);
    
    // Allocate buffer (workspace) if not allocated already
    if (*visited_buf == NULL) {
        *visited_buf = (bool *) malloc(graph->nVert * sizeof(bool));
        assert(*visited_buf != NULL);
    }
    if (*distances_buf == NULL) {
        *distances_buf = (int *) malloc(graph->nVert * sizeof(int));
        assert(*distances_buf != NULL);
    }
    
    // (Re-)initialize workspace
    unsigned int i;
    for (i = 0; i < graph->nVert; ++i) {
        (*visited_buf)[i] = false;
        (*distances_buf)[i] = -1;
    }
    // Initialize arrays for source
    (*distances_buf)[source] = 0;
    
    bool is_reachable = false;
    Graph_DFS_reachable(graph, source, target, distance, *visited_buf,
                *distances_buf, &is_reachable);
    
    return is_reachable;
}

void Graph_free(Graph *graph) {
    // Free all linked lists
    for (unsigned int i = 0; i < graph->nVert; ++i) {
        LL_free(&graph->adjList[i]);
    }
    // Free adjacency list
    free(graph->adjList);
}

#endif /* GRAPH_H */
