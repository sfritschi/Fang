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

//#include <omp.h>

#include "linked_list.h"
#include "hashmap.h"

enum GRAPH_TYPE {
    GRAPH_DIRECTED,
    GRAPH_UNDIRECTED
};

struct Edge {
    unsigned int index;
    bool isBoegOnly;
    struct Edge *next;    
};

typedef struct Edge * EdgeList;

typedef struct {
    EdgeList *adjList;
    unsigned int nVert;
    unsigned int nEdge;
    enum GRAPH_TYPE type; 
} Graph;

void Graph_init_file(Graph *, FILE *);
void Graph_insert_edge(Graph *, unsigned int, unsigned int, bool);
void Graph_BFS_SP(const Graph *, bool, unsigned int, int *, int *);
void Graph_BFS_APSP(const Graph *, bool, int *, int *);
void Graph_DFS_reachable(const Graph *, bool, unsigned int , unsigned int, int,
                                    bool *, int *, bool *);
bool Graph_is_reachable(const Graph *, bool, unsigned int, unsigned int, int,
                                  bool *, int *);
void Graph_DFS_reachable_pos(const Graph *, bool, unsigned int, int, 
                                bool *, int *, HashMap *);
HashMap Graph_reachable_pos(const Graph *, bool, unsigned int, int, bool *, int *);
void Graph_free(Graph *);

void Graph_insert_edge(Graph *g, unsigned int node, 
                       unsigned int index, bool isBoegOnly)
{
    EdgeList el = NULL;
    // Initialize edge from data
    el = (EdgeList) malloc(sizeof(struct Edge)); assert(el);
    el->index = index;
    el->isBoegOnly = isBoegOnly;
    el->next = g->adjList[node];
    // Update head of linked list
    g->adjList[node] = el;
}

void Graph_init_file(Graph *graph, FILE *fp) 
{
    assert(graph && fp);
    
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
    
    graph->adjList = (EdgeList *) calloc(nVertices, sizeof(EdgeList));
    assert(graph->adjList);
    
    graph->nVert = nVertices;
    graph->nEdge = 0;
    graph->type = type;
    
    unsigned int from_node;
    unsigned int to_node;
    unsigned int boeg;
    // Initialize graph by parsing file
    while ((fscanf(fp, "%u %u %u", &from_node, &to_node, &boeg)) != EOF) {
        if (from_node >= nVertices || to_node >= nVertices) {
            fprintf(stderr, "Invalid edge at line: %u\n", graph->nEdge + 1);
            Graph_free(graph);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        bool isBoegOnly = (bool)boeg;
        // Insert directed edge
        Graph_insert_edge(graph, from_node, to_node, isBoegOnly);
        
        if (type == GRAPH_UNDIRECTED) {
            // Also insert edge in opposite direction
            Graph_insert_edge(graph, to_node, from_node, isBoegOnly);
        }
        ++graph->nEdge;
    }
}

void Graph_BFS_SP(const Graph *graph, bool isBoeg,
                  unsigned int source, int *distances, int *parents) 
{
    assert(graph && source < graph->nVert);
    assert(distances && parents);
    
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
        
        // Get iterator for adjacency list
        EdgeList iter = graph->adjList[current];
        
        while (iter) {
            // Skip edges only accessible to Boeg from player perspective
            if (!isBoeg && iter->isBoegOnly)
                goto next_iter;
            
            unsigned int neighbor = iter->index;
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
next_iter:
            // Move on to next neighbor
            iter = iter->next;
        }
    }
    
    // Cleanup
    free(visited);
    // Technically not necessary, since already empty
    LL_free(&searchList);    
}

void Graph_BFS_APSP(const Graph *graph, bool isBoeg, int *distances, int *parents)
{
    assert(graph && distances && parents);
    // Run BFS for every vertex as source in parallel
    //#pragma omp parallel for num_threads(4)
    for (unsigned int source = 0; source < graph->nVert; ++source) {
        unsigned int offset = source * graph->nVert;
        Graph_BFS_SP(graph, isBoeg, source, &distances[offset], &parents[offset]);
    }
}

void Graph_DFS_reachable_pos(const Graph *graph, bool isBoeg, unsigned int u,
                 int distance, bool *visited_buf, int *distances_buf, 
                     HashMap *reachableVert)
{    
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
    EdgeList iter = graph->adjList[u];
    
    while (iter) {
        // Skip edges only accessible to Boeg from player perspective
        if (!isBoeg && iter->isBoegOnly)
            goto next_iter;
            
        unsigned int neighbor = iter->index;
        // Check if cycle is reached; move on to next neighbor
        if (visited_buf[neighbor])
            goto next_iter;
            
        // Update distance
        distances_buf[neighbor] = distances_buf[u] + 1;
        // Check if already past distance limit
        if (distances_buf[neighbor] <= distance) {
            // Recursion
            Graph_DFS_reachable_pos(graph, isBoeg, neighbor, distance, visited_buf, 
                        distances_buf, reachableVert);
        }
next_iter:
        // Go to next neighbor in adjacency list
        iter = iter->next;
    }
    // Backtrack
    visited_buf[u] = false;
}

HashMap Graph_reachable_pos(const Graph *graph, bool isBoeg,
                            unsigned int source, int distance,
                            bool *visited_buf, int *distances_buf) 
{
    assert(source < graph->nVert);
    assert(visited_buf && distances_buf);
    assert(distance >= 0);
    
    // (Re-)initialize workspace
    unsigned int i;
    for (i = 0; i < graph->nVert; ++i) {
        visited_buf[i] = false;
        distances_buf[i] = -1;
    }
    // Initialize arrays for source
    distances_buf[source] = 0;
    
    // Initialize linked-list containing reachable vertices
    HashMap reachableVert;
    HashMap_init(&reachableVert);
    
    Graph_DFS_reachable_pos(graph, isBoeg, source, distance, visited_buf, 
                                distances_buf, &reachableVert);
    
    return reachableVert;
}

void Graph_DFS_reachable(const Graph *graph, bool isBoeg, unsigned int u,
                 unsigned int v, int distance,
                   bool *visited_buf, int *distances_buf, 
                     bool *is_reachable) 
{
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
    EdgeList iter = graph->adjList[u];
    
    while (iter != NULL) {
        // Skip edges only accessible to Boeg from player perspective
        if (!isBoeg && iter->isBoegOnly)
            goto next_iter;
            
        unsigned int neighbor = iter->index;
        // Check if cycle is reached; move on to next neighbor
        if (visited_buf[neighbor])
            goto next_iter;
            
        // Update distance
        distances_buf[neighbor] = distances_buf[u] + 1;
        // Check if already past distance limit
        if (neighbor == v || distances_buf[neighbor] != distance) {
            // Recursion
            Graph_DFS_reachable(graph, isBoeg, neighbor, v, distance, visited_buf, 
                        distances_buf, is_reachable);
        }
next_iter:
        // Go to next neighbor in adjacency list
        iter = iter->next;
    }
    // Backtrack
    visited_buf[u] = false;
}

// Determine if there exists a simple path from source to distance of
// length exactly distance
bool Graph_is_reachable(const Graph *graph, bool isBoeg, 
                        unsigned int source, unsigned int target, int distance,
                        bool *visited_buf, int *distances_buf)
{
    assert(source < graph->nVert && target < graph->nVert);
    assert(visited_buf && distances_buf);
    assert(distance >= 0);
    
    // (Re-)initialize workspace
    unsigned int i;
    for (i = 0; i < graph->nVert; ++i) {
        visited_buf[i] = false;
        distances_buf[i] = -1;
    }
    // Initialize arrays for source
    distances_buf[source] = 0;
    
    bool is_reachable = false;
    Graph_DFS_reachable(graph, isBoeg, source, target, distance, visited_buf,
                distances_buf, &is_reachable);
    
    return is_reachable;
}

void Graph_free(Graph *graph)
{
    // Free all linked lists
    for (unsigned int i = 0; i < graph->nVert; ++i) {
        EdgeList iter = graph->adjList[i];
        while (iter) {
            EdgeList temp = iter;
            iter = iter->next;
            free(temp);
        }
    }
    // Free adjacency list
    free(graph->adjList);
}

#endif /* GRAPH_H */
