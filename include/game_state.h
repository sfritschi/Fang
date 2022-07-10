/*
 * Encapsulates all the necessary information on the state of the game
 * and provides methods to simulate the board game for a variable number
 * of players, as well as statistical analysis of consecutive
 * runs of the game.
 * 
 * Depends on:
 * - Graph data structure (adjacency list) + algorithms
 * - Location data structure (name + vertex number)
 */

#pragma once
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>  // INFINITY

#include "graph.h"
#include "location.h"

#define DIE_SIZE (6)
#define MAX_TURNS (100)
#define MIN_PLAYERS (3)
#define MAX_PLAYERS (6)
#define BOEG_ID_DEFAULT (MAX_PLAYERS + 1)
#define N_TARGETS (40)
#define N_TARGETS_PLAYER (4)

// Modify active players (AP) mask
#define AP_INIT(ap, np) (ap = (1 << np) - 1)
#define AP_UNSET(ap, i) (ap = ap & ~(1 << i))
#define AP_ISSET(ap, i) ((ap >> i) & 0x1)

// Colors used for terminal output
static const char *DEFAULT_COLOR = "\033[0m";
static const char *PLAYER_COLORS[MAX_PLAYERS] = {
    "\033[38;5;160m",  // RED
    "\033[38;5;40m",   // GREEN
    "\033[38;5;68m",   // BLUE
    "\033[38;5;226m",  // YELLOW
    "\033[38;5;202m",  // ORANGE
    "\033[38;5;134m"   // PURPLE
};

// Game status
enum STATUS {
    CONTINUE,
    GAMEOVER,
    INVALID
};

// Move-making AI strategy
enum MOVE_STRATEGY {
    GREEDY,
    AVOIDANT,
    USER_COMMAND
};

static const char *STRATEGY_NAMES[] = {
    "GREEDY",
    "AVOIDANT",
    "USER COMMAND"
};

// Encodes all static information about game board
typedef struct {
    // Graphs (adjacency lists)
    Graph graph_player, graph_boeg;
    // Locations on board
    Location_t *locations;
    Location_t *locations_sorted;
    // Arrays containing shortest path data
    int *dist_player, *dist_boeg;
    int *par_player, *par_boeg;
    // Number of positions on board
    unsigned int nPositions;
} BoardInfo_t;

// Encodes all information of current state of the game
typedef struct {
    unsigned int targets[N_TARGETS];
    unsigned int *player_pos;
    unsigned int *player_targets;
    unsigned int *player_targets_left;
    unsigned int *player_order;
    unsigned int boeg_pos;
    unsigned int boeg_id;  // Keeps track which player is currently the boeg
    // Number of players
    unsigned int nPlayers;
    // Auxiliary buffers needed for graph algorithms
    bool *visited_buf;
    int *distances_buf;
    // Bitmask for active players
    int8_t active_players;
} GameState_t;

// Encodes information about results of game
typedef struct {
    int winner;
    unsigned int nTurns;
} GameResult_t;

// Print string in a given color to console
void print_colored(const char *text, const char *color) {
    printf("%s%s%s", color, text, DEFAULT_COLOR);
}

// Shuffle array randomly; from StackOverflow
void shuffle(unsigned int *array, size_t n) {
    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          unsigned int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

// Roll single dice and return result
int roll_dice() {
    return (rand() % DIE_SIZE) + 1;
}

// Determine if there is an opponent at the specified target location
bool opponent_at_target(const GameState_t *gstate, unsigned int target, 
                        unsigned int player_id) {
    unsigned int j;
    for (j = 0; j < gstate->nPlayers; ++j) {
        if (j != player_id && AP_ISSET(gstate->active_players, j) &&
                gstate->player_pos[j] == target) {
            return true;
        }
    }
    return false;
}

// Recursively explore shortest path using parents array
unsigned int __print_path(const int *parents, const Location_t *locations,
                        int target, int offset, unsigned int dist,
                        unsigned int *final_pos, const char *color) {
    if (parents[offset + target] == -1) {
        print_colored(locations[target].name, color);
        printf("\n");
        return 1;
    }
    unsigned int current_dist = __print_path(parents, locations, 
                        parents[offset + target], offset, dist,
                        final_pos, color) + 1;
    if (current_dist - 1 == dist) {
        *final_pos = locations[target].index;
    }
    if (current_dist - 1 <= dist) {
        print_colored(locations[target].name, color);
        printf("\n");
    }
    return current_dist;
}

// Print path (location names) from source to target using recursion
unsigned int print_path(const int *parents, const Location_t *locations, 
                    int source, int target, int n, unsigned int dist,
                    const char *color) {
    assert((0 <= source && source < n) && (0 <= target && target < n));
    unsigned int final_pos;
    __print_path(parents, locations, target, source * n, dist,
                    &final_pos, color);
    return final_pos;   
}

// Recursively follow along path to target using parents array
unsigned int __follow_path(const int *parents, int target, int offset,
                            unsigned int dist, unsigned int *final_pos) {
    unsigned int current_dist;
    if (parents[offset + target] == -1) {
        return 1;
    }
    current_dist = __follow_path(parents,  parents[offset + target], offset,
                                    dist, final_pos) + 1;
    if (current_dist - 1 == dist) {
        *final_pos = target;
    }
    
    return current_dist;
}

// Follow along path dist number of steps from source to target
unsigned int follow_path(const int *parents, int source, int target,
                            int n, unsigned int dist) {
    assert((0 <= source && source < n) && (0 <= target && target < n));
    unsigned int final_pos;
    __follow_path(parents, target, source * n, dist, &final_pos);
    return final_pos;   
}

void BoardInfo_init(BoardInfo_t *binfo) {
    // Initialize graphs (adjacency lists)
    FILE *fp_player = fopen("board/graph_player.txt", "r");
    FILE *fp_boeg = fopen("board/graph_full.txt", "r");
    if (fp_player == NULL || fp_boeg == NULL) {
        fprintf(stderr, "Could not open board file(s)\n");
        exit(EXIT_FAILURE);
    }
    // Init graphs
    Graph_init_file(&binfo->graph_player, fp_player);
    fclose(fp_player);
    Graph_init_file(&binfo->graph_boeg, fp_boeg);
    fclose(fp_boeg);
    
    // Make sure graphs share same number of vertices
    assert(binfo->graph_player.nVert == binfo->graph_boeg.nVert);
    // Number of total vertices
    const unsigned int nVert = binfo->graph_player.nVert;
    // Set number of positions
    binfo->nPositions = nVert;
    
    // Read locations:
    binfo->locations = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(binfo->locations != NULL);
    binfo->locations_sorted = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(binfo->locations_sorted != NULL);
    
    FILE *fp_loc = fopen("board/locations.txt", "r");
    if (fp_loc == NULL) {
        free(binfo->locations);
        free(binfo->locations_sorted);
        Graph_free(&binfo->graph_player);
        Graph_free(&binfo->graph_boeg);
        fprintf(stderr, "Could not open locations file\n");
        exit(EXIT_FAILURE);
    }
    // Read locations from file into locations array
    read_locations(fp_loc, binfo->locations, binfo->locations_sorted);
    // Close locations file
    fclose(fp_loc);
    // Sort locations array in ascending order
    qsort((void *)&binfo->locations_sorted[0], nVert,
           sizeof(Location_t), &location_cmp);
                            
    // Initialize distances and parents using BFS APSP
    binfo->dist_player = (int *) malloc(nVert * nVert * sizeof(int));
    assert(binfo->dist_player != NULL);
    binfo->par_player = (int *) malloc(nVert * nVert * sizeof(int));
    assert(binfo->par_player != NULL);
    
    binfo->dist_boeg = (int *) malloc(nVert * nVert * sizeof(int));
    assert(binfo->dist_boeg != NULL);
    binfo->par_boeg = (int *) malloc(nVert * nVert * sizeof(int));
    assert(binfo->par_boeg != NULL);
    
    // Compute all pairs shortest paths (APSP) for both boards
    Graph_BFS_APSP(&binfo->graph_player, binfo->dist_player, binfo->par_player);
    Graph_BFS_APSP(&binfo->graph_boeg, binfo->dist_boeg, binfo->par_boeg);
}

// Initialize game state based on number of players
void GameState_init(GameState_t *gstate, unsigned int nPlayers,
                    unsigned int nPositions) {
    // -- Initialize Game State data --
    gstate->player_pos = (unsigned int *) malloc(nPlayers * sizeof(unsigned int));
    assert(gstate->player_pos != NULL);
    gstate->player_targets = (unsigned int *) 
            malloc(nPlayers * N_TARGETS_PLAYER * sizeof(unsigned int));
    assert(gstate->player_targets != NULL);
    gstate->player_targets_left = (unsigned int *)
            malloc(nPlayers * sizeof(unsigned int));
    assert(gstate->player_targets_left != NULL);
    gstate->player_order = (unsigned int *) malloc(nPlayers * sizeof(unsigned int));
    assert(gstate->player_order != NULL);
    
    // Initialize number of players
    gstate->nPlayers = nPlayers;
    // Set active players
    AP_INIT(gstate->active_players, nPlayers);
    // Initialize Boeg id
    gstate->boeg_id = BOEG_ID_DEFAULT; 
    unsigned int i, j;
    // Initialize order in which players take turns
    for (i = 0; i < nPlayers; ++i) {
        gstate->player_order[i] = i;
    }
    // Randomly shuffle player order
    shuffle(gstate->player_order, nPlayers);
    
    // Initialize (static) targets
    for (i = 0; i < N_TARGETS; ++i) {
        gstate->targets[i] = i;
    }
    // Randomly shuffle targets
    shuffle(gstate->targets, N_TARGETS);
    // Initialize player targets
    unsigned int iter = 0;
    unsigned int offset = 0;
    for (i = 0; i < nPlayers; ++i) {
        for (j = 0; j < N_TARGETS_PLAYER; ++j) {
            gstate->player_targets[offset + j] = gstate->targets[iter++];
        }
        offset += N_TARGETS_PLAYER;
    }
    // Initialize boeg position
    gstate->boeg_pos = gstate->targets[iter];
    
    // Place players randomly & set initial number of targets
    for (i = 0; i < nPlayers; ++i) {
        // Place players ONLY on non-target positions to avoid
        // possible collisions with placement of Boeg
        gstate->player_pos[i] = (rand() % (nPositions - N_TARGETS)) + N_TARGETS;
        gstate->player_targets_left[i] = N_TARGETS_PLAYER;
    }
    // Initialize auxiliary buffers
    gstate->visited_buf = (bool *) malloc(nPositions * sizeof(bool));
    assert(gstate->visited_buf != NULL);
    gstate->distances_buf = (int *) malloc(nPositions * sizeof(int));
    assert(gstate->distances_buf != NULL);
}

// Reset game state and re-randomize for next round
void GameState_reset(GameState_t *gstate, unsigned int nPositions) {
    unsigned int i, j;
    for (i = 0; i < gstate->nPlayers; ++i) {
        // Place players ONLY on non-target positions to avoid
        // possible collisions with placement of Boeg
        gstate->player_pos[i] = (rand() % (nPositions - N_TARGETS)) + N_TARGETS;
        gstate->player_targets_left[i] = N_TARGETS_PLAYER;
    }
    // Reset active players
    AP_INIT(gstate->active_players, gstate->nPlayers);
    // Re-shuffle player order
    shuffle(gstate->player_order, gstate->nPlayers);
    // Reset (static) targets
    //for (i = 0; i < N_TARGETS; ++i) {
    //    gstate->targets[i] = i;
    //}
    // Randomly re-shuffle targets
    shuffle(gstate->targets, N_TARGETS);
    // Re-initialize player targets
    unsigned int iter = 0;
    unsigned int offset = 0;
    for (i = 0; i < gstate->nPlayers; ++i) {
        for (j = 0; j < N_TARGETS_PLAYER; ++j) {
            gstate->player_targets[offset + j] = gstate->targets[iter++];
        }
        offset += N_TARGETS_PLAYER;
    }
    // Re-initialize boeg position
    gstate->boeg_pos = gstate->targets[iter];
    // Reset id of Boeg
    gstate->boeg_id = BOEG_ID_DEFAULT;
}

// Print all relevant information about current state of game
void GameState_info(const BoardInfo_t *binfo, const GameState_t *gstate,
                    unsigned int command_id) {
    assert(binfo != NULL && gstate != NULL);
    
    unsigned int pos;
    unsigned int i, j;
    
    printf("\nPlayer pos:\n");
    for (i = 0; i < gstate->nPlayers; ++i) {
        if (AP_ISSET(gstate->active_players, i)) {
            pos = gstate->player_pos[i];
            print_colored(binfo->locations[pos].name, PLAYER_COLORS[i]);
            printf("\n");
        }
    }
    printf("\n");
    if (gstate->boeg_id != BOEG_ID_DEFAULT) {
        print_colored("Boeg pos:", PLAYER_COLORS[gstate->boeg_id]);
    } else {
        printf("Boeg pos:");
    }
    printf("\n%s\n\n", binfo->locations[gstate->boeg_pos].name);
    
    
    if (AP_ISSET(gstate->active_players, command_id)) {
        printf("Your targets:\n");
        unsigned int offset = command_id * N_TARGETS_PLAYER;
        for (j = 0; j < N_TARGETS_PLAYER; ++j) {
            pos = gstate->player_targets[offset + j];
            if (pos == N_TARGETS) {
                continue;
            }
            print_colored(binfo->locations[pos].name, PLAYER_COLORS[command_id]);
            printf("\n");
        }
    }
    printf("\n#Player targets left: ");
    for (i = 0; i < gstate->nPlayers; ++i) {
        if (AP_ISSET(gstate->active_players, i)) {
            printf("%s%u%s ", PLAYER_COLORS[i], gstate->player_targets_left[i],
                                DEFAULT_COLOR);
        }
    }
    printf("\n");
}

// Cleanup
void BoardInfo_free(BoardInfo_t *binfo) {
    assert(binfo != NULL);
    
    free(binfo->locations);
    free(binfo->locations_sorted);
    free(binfo->dist_player);
    free(binfo->dist_boeg);
    free(binfo->par_player);
    free(binfo->par_boeg);
    // Clean up graphs
    Graph_free(&binfo->graph_player);
    Graph_free(&binfo->graph_boeg);
}

void GameState_free(GameState_t *gstate) {
    assert(gstate != NULL);
    
    free(gstate->player_pos);
    free(gstate->player_targets);
    free(gstate->player_targets_left);
    free(gstate->player_order);
    // Clean up auxiliary buffers
    free(gstate->visited_buf);
    free(gstate->distances_buf);
}

// GREEDY STRATEGY:
// Always move to closest target using shortest path
enum STATUS GameState_move_greedy(const BoardInfo_t *binfo, 
            GameState_t *gstate, unsigned int player_id, bool verbose) {
    unsigned int i, j, offset_targets;
    unsigned int target, min_target = N_TARGETS;
    int dist, min_dist = RAND_MAX;
    unsigned int closest_pos;
    unsigned int current_pos = gstate->player_pos[player_id];
    // Offset in distances/parents array
    unsigned int offset_board;
    // Roll dice
    int dice_roll = roll_dice();
    if (verbose)
        printf("\n%sDice Roll: %d%s\n\n", PLAYER_COLORS[player_id], 
            dice_roll, DEFAULT_COLOR);
    // Check if playing as Boeg
    if (player_id == gstate->boeg_id) {
        
        offset_targets = player_id * N_TARGETS_PLAYER;
        offset_board = gstate->boeg_pos * binfo->nPositions;
        // Check all remaining targets if reachable
        for (i = 0; i < N_TARGETS_PLAYER; ++i) {
            target = gstate->player_targets[offset_targets + i];
            // If target invalid -> skip
            if (target == N_TARGETS) {
                continue;  // move on to next target
            }
            
            // Distance from current pos to target
            dist = binfo->dist_boeg[offset_board + target];
            if (dice_roll >= dist) {  // Found reachable target
                // Check if target is already occupied -> skip
                if (opponent_at_target(gstate, target, player_id)) {
                    continue;
                }
                // DEBUG
                if (verbose) {
                    print_path(binfo->par_boeg, binfo->locations, 
                            gstate->boeg_pos, target, binfo->nPositions,
                            dist, DEFAULT_COLOR);
                }
                // Move Boeg to this target
                gstate->boeg_pos = target;
                // Update targets (invalidate & decrement)
                gstate->player_targets[offset_targets + i] = N_TARGETS;
                // Check if GAMEOVER
                if (--gstate->player_targets_left[player_id] == 0)
                    return GAMEOVER;
                    
                return CONTINUE;
            }
            // Update sorted targets
            if (dist < min_dist) {
                min_dist = dist;
                min_target = target;
            }
        }
        if (min_target != N_TARGETS) {
            // Try to move as far as possible to closest (min) target
            closest_pos = follow_path(binfo->par_boeg, gstate->boeg_pos, min_target,
                                        binfo->nPositions, dice_roll);
        }
        // EDGE CASE: Check if already occupied by opponent(s)
        if (min_target == N_TARGETS || opponent_at_target(gstate, closest_pos, player_id)) {
            // Try to move to different location 'dice_roll' away
            // that is as close as possible
            if (verbose)
                printf("Occupied...\n");
                
            closest_pos = binfo->nPositions;
            // Consider all possible locations that are in reach
            unsigned int offset;
            int sum_dists;
            int min_sum = RAND_MAX;
            // Obtain HashMap of all reachable positions from current pos
            // in exactly 'dice_roll' steps
            HashMap reachablePos;
            reachablePos = Graph_reachable_pos(
                                &binfo->graph_boeg,
                                gstate->boeg_pos, dice_roll,
                                gstate->visited_buf, 
                                gstate->distances_buf);
            // Iterate over all reachable positions
            size_t current;
            const size_t nReachable = HashMap_size(&reachablePos);
            for (current = 0; current < nReachable; ++current) {
                j = HashMap_get(&reachablePos, current);
                // Make sure no opponent is already at current pos
                if (!opponent_at_target(gstate, j, player_id)) {
                    // Compute offset
                    offset = j * binfo->nPositions;
                    // Iterate over all targets and sum min distances
                    sum_dists = 0;
                    for (i = 0; i < N_TARGETS_PLAYER; ++i) {
                        target = gstate->player_targets[offset_targets + i];
                        // Make sure target is valid
                        if (target == N_TARGETS) {
                            continue;
                        }
                        dist = binfo->dist_boeg[offset + target];
                        sum_dists += dist;
                    }
                    // Update closest pos based on sum of min distances
                    if (sum_dists < min_sum) {
                        min_sum = sum_dists;
                        closest_pos = j;
                    }
                }
            }
        }
        // Check if succeeded in finding alternative position
        if (closest_pos != binfo->nPositions) {
            // Update Boeg position and print path taken
            if (verbose) {
                // ISSUE: Prints shortest path from boeg_pos to optimal
                //        pos instead of actual path taken
                print_path(binfo->par_boeg, binfo->locations, 
                            gstate->boeg_pos, closest_pos, binfo->nPositions,
                            dice_roll, DEFAULT_COLOR);
            }
            gstate->boeg_pos = closest_pos;
            
        } else {
            // Otherwise stay at current position
            if (verbose)
                printf("Skipping turn...\n");
        }
        return CONTINUE;
    } else {
        
        offset_board = gstate->player_pos[player_id] * binfo->nPositions;
        // Move to closest position of Boeg
        // Distance between player and Boeg
        dist = binfo->dist_player[offset_board + gstate->boeg_pos];
        if (dice_roll >= dist) {
            // DEBUG
            if (verbose) {
                print_path(binfo->par_player, binfo->locations, 
                            current_pos, gstate->boeg_pos, binfo->nPositions,
                            dist, PLAYER_COLORS[player_id]);
            }
            // Move player to Boeg
            gstate->player_pos[player_id] = gstate->boeg_pos;
            // Update Boeg id
            gstate->boeg_id = player_id;
            // Make next move as Boeg
            return GameState_move_greedy(binfo, gstate, player_id, verbose);
        }
        // Move as close as possible to boeg
        if (verbose) {
            gstate->player_pos[player_id] = print_path(binfo->par_player, binfo->locations, 
                            current_pos, gstate->boeg_pos, binfo->nPositions,
                            dice_roll, PLAYER_COLORS[player_id]);
        } else {
            gstate->player_pos[player_id] = follow_path(binfo->par_player, 
                    current_pos, gstate->boeg_pos, binfo->nPositions, dice_roll);
        }
        return CONTINUE;
    }
}

// Avoid opponents when playing as Boeg, while still minimizing
// distance to targets left
enum STATUS GameState_move_avoidant(const BoardInfo_t *binfo, 
        GameState_t *gstate, unsigned int player_id, 
        double avoidance, bool verbose) {
    unsigned int i, j, offset_targets;
    unsigned int target;
    int dist;
    unsigned int optimal_pos;
    unsigned int current_pos = gstate->player_pos[player_id];
    // Offset in distances/parents array
    unsigned int offset_board;
    // Roll dice
    int dice_roll = roll_dice();
    if (verbose)
        printf("\n%sDice Roll: %d%s\n\n", PLAYER_COLORS[player_id], 
            dice_roll, DEFAULT_COLOR);
    // Check if playing as Boeg
    if (player_id == gstate->boeg_id) {
        
        offset_targets = player_id * N_TARGETS_PLAYER;
        offset_board = gstate->boeg_pos * binfo->nPositions;
        // Check all remaining targets if reachable
        for (i = 0; i < N_TARGETS_PLAYER; ++i) {
            target = gstate->player_targets[offset_targets + i];
            // If target invalid -> skip
            if (target == N_TARGETS) {
                continue;  // move on to next target
            }
            
            // Distance from current pos to target
            dist = binfo->dist_boeg[offset_board + target];
            if (dice_roll >= dist) {  // Found reachable target
                // Check if target is already occupied -> skip
                if (opponent_at_target(gstate, target, player_id)) {
                    continue;
                }
                // DEBUG
                if (verbose) {
                    print_path(binfo->par_boeg, binfo->locations, 
                            gstate->boeg_pos, target, binfo->nPositions,
                            dist, DEFAULT_COLOR);
                }
                // Move Boeg to this target
                gstate->boeg_pos = target;
                // Update targets (invalidate & decrement)
                gstate->player_targets[offset_targets + i] = N_TARGETS;
                // Check if GAMEOVER
                if (--gstate->player_targets_left[player_id] == 0)
                    return GAMEOVER;
                    
                return CONTINUE;
            }
        }
        // Try to move to different location 'dice_roll' away
        // that is as close as possible to targets, while maintaining
        // distance to opponents -> minimize objective
        optimal_pos = binfo->nPositions;
        // Consider all possible locations that are in reach
        unsigned int offset;
        double objective;
        double min_objective = INFINITY;
        // Obtain HashMap of all reachable positions from current pos
        // in exactly 'dice_roll' steps
        HashMap reachablePos;
        reachablePos = Graph_reachable_pos(
                            &binfo->graph_boeg,
                            gstate->boeg_pos, dice_roll,
                            gstate->visited_buf, 
                            gstate->distances_buf);
        // Iterate over all reachable positions and evaluate objective
        size_t current;
        const size_t nReachable = HashMap_size(&reachablePos);
        for (current = 0; current < nReachable; ++current) {
            j = HashMap_get(&reachablePos, current);
            // Make sure no opponent is already at current pos
            if (!opponent_at_target(gstate, j, player_id)) {
                // Compute offset
                offset = j * binfo->nPositions;
                // Compute objective for candidate position
                objective = 0.0;
                // Iterate over all targets left and sum min distances
                for (i = 0; i < N_TARGETS_PLAYER; ++i) {
                    target = gstate->player_targets[offset_targets + i];
                    // Make sure target is valid
                    if (target == N_TARGETS) {
                        continue;
                    }
                    // Compute shortest distance to target
                    dist = binfo->dist_boeg[offset + target];
                    // Update objective
                    objective += (double)dist;
                }
                // Iterate over all opponent positions and update objective
                for (i = 0; i < gstate->nPlayers; ++i) {
                    // Ignore self
                    if (i == player_id || 
                            !AP_ISSET(gstate->active_players, i)) {
                        continue;
                    }
                    unsigned int opp_pos = gstate->player_pos[i];
                    // Compute shortest distance from opponent to
                    // candidate position
                    int opp_dist = binfo->dist_player[opp_pos * binfo->nPositions + j];
                    // DEBUG
                    assert(opp_dist != 0);
                    double denom = (double)opp_dist;
                    // Check if opponent cannot reach this position
                    // within one dice roll
                    if (opp_dist > DIE_SIZE) {
                        // Lessen penalty in objective in this case
                        // -> larger denominator
                        denom = 2. * denom;
                    }
                    // Update objective (parameterized)
                    objective += avoidance / denom;
                }
                // Update optimal pos based on objective value
                if (objective < min_objective) {
                    min_objective = objective;
                    optimal_pos = j;
                }
            }
        }
        // Check if succeeded in finding optimal position
        if (optimal_pos != binfo->nPositions) {
            // Update Boeg position and print path taken
            if (verbose) {
                // ISSUE: Prints shortest path from boeg_pos to optimal
                //        pos instead of actual path taken
                print_path(binfo->par_boeg, binfo->locations, 
                            gstate->boeg_pos, optimal_pos, binfo->nPositions,
                            dice_roll, DEFAULT_COLOR);
            }
            gstate->boeg_pos = optimal_pos;
            
        } else {
            // Otherwise stay at current position
            if (verbose)
                printf("Skipping turn...\n");
        }
        return CONTINUE;
        
    } else {
        
        offset_board = gstate->player_pos[player_id] * binfo->nPositions;
        // Move to closest position of Boeg
        // Distance between player and Boeg
        dist = binfo->dist_player[offset_board + gstate->boeg_pos];
        if (dice_roll >= dist) {
            // DEBUG
            if (verbose) {
                print_path(binfo->par_player, binfo->locations, 
                            current_pos, gstate->boeg_pos, binfo->nPositions,
                            dist, PLAYER_COLORS[player_id]);
            }
            // Move player to Boeg
            gstate->player_pos[player_id] = gstate->boeg_pos;
            // Update Boeg id
            gstate->boeg_id = player_id;
            // Make next move as Boeg
            return GameState_move_avoidant(binfo, gstate, player_id, avoidance, verbose);
        }
        // Move as close as possible to boeg
        if (verbose) {
            gstate->player_pos[player_id] = print_path(binfo->par_player, binfo->locations, 
                            current_pos, gstate->boeg_pos, binfo->nPositions,
                            dice_roll, PLAYER_COLORS[player_id]);
        } else {
            gstate->player_pos[player_id] = follow_path(binfo->par_player, 
                    current_pos, gstate->boeg_pos, binfo->nPositions, dice_roll);
        }
        return CONTINUE;
    }
}
                            
enum STATUS GameState_move_command(const BoardInfo_t *binfo, 
        GameState_t *gstate, unsigned int player_id) {
    
    unsigned int player_pos;
    unsigned int end_pos;
    int dist, min_dist;
    char end_loc[MAX_LOCATION_LEN];
    // Roll dice
    int dice_roll = roll_dice();
    printf("\n%sDice Roll: %d%s\n\n", PLAYER_COLORS[player_id], 
                dice_roll, DEFAULT_COLOR);
    
    // Need to consider all reachable positions
    HashMap reachablePos;
    size_t nReachable, current;
    unsigned int i, pos;
    unsigned int target, offset_board, offset_targets;
    
    if (player_id == gstate->boeg_id) {  // playing as boeg
        
        offset_board = gstate->boeg_pos * binfo->nPositions;
        offset_targets = player_id * N_TARGETS_PLAYER;
        // Verify that there are any valid moves
        bool no_valid_moves = true;
        reachablePos = Graph_reachable_pos(
                            &binfo->graph_boeg,
                            gstate->boeg_pos, dice_roll,
                            gstate->visited_buf, 
                            gstate->distances_buf);
        // Compute number of reachable Positions
        nReachable = HashMap_size(&reachablePos);
        for (current = 0; current < nReachable; ++current) {
            pos = HashMap_get(&reachablePos, current);
            // Check if opponent at pos
            if (!opponent_at_target(gstate, pos, player_id)) {
                // There is at least one valid move
                no_valid_moves = false;
                break;
            } 
        }
        // If there are no unoccupied locations reachable exactly within
        // dice_roll steps, also check for active target locations that
        // can be reached in strictly less steps and are vacant 
        if (no_valid_moves) {
            // Look for a valid, unoccupied target location that
            // is reachable within less steps than dice_roll
            for (i = 0; i < N_TARGETS_PLAYER; ++i) {
                target = gstate->player_targets[offset_targets + i];
                // Check if target is already visited
                if (target == N_TARGETS) {
                    continue;
                }
                // Distance from boeg position to target
                dist = binfo->dist_boeg[offset_board + target];
                if (!opponent_at_target(gstate, target, player_id) &&
                    dice_roll >= dist) {
                    // Found reachable, valid, unoccupied target location
                    no_valid_moves = false;
                    break;
                }
            }
        }
        // If there are no valid moves, skip turn
        if (no_valid_moves) {
            printf("No valid moves!\n");
            printf("Skipping turn...\n");
            return CONTINUE;
        }
        // Repeat until user enters valid location
        while (true) {
            printf("Enter valid target location: ");
            // Read target location from command line
            fgets(end_loc, MAX_LOCATION_LEN, stdin);
            // Remove newline
            end_loc[strcspn(end_loc, "\n")] = '\0';
            // Find associated index of target location using binary search
            end_pos = location_binsearch(binfo->locations_sorted, end_loc, 
                                                binfo->nPositions);
            // Make sure no opponent is at chosen end position
            if (opponent_at_target(gstate, end_pos, player_id)) {
                printf("\nAlready occupied by opponent!\n"); 
                continue;
            }
            printf("\nTarget location: '%s'\n", binfo->locations[end_pos].name);
            // See if end_pos corresponds to target location
            for (i = 0; i < N_TARGETS_PLAYER; ++i) {
                target = gstate->player_targets[offset_targets + i];
                // Check if target is already visited
                if (target == N_TARGETS) {
                    continue;
                }
                // Distance from boeg position to target
                dist = binfo->dist_boeg[offset_board + target];
                if (end_pos == target && dice_roll >= dist) {
                    // Valid position
                    // Print path taken
                    print_path(binfo->par_boeg, binfo->locations, 
                                gstate->boeg_pos, target, binfo->nPositions,
                                dist, DEFAULT_COLOR);
                    // Move Boeg to this target
                    gstate->boeg_pos = target;
                    // Update targets (invalidate & decrement)
                    gstate->player_targets[offset_targets + i] = N_TARGETS;
                    // Check if GAMEOVER
                    if (--gstate->player_targets_left[player_id] == 0)
                        return GAMEOVER;
                        
                    return CONTINUE;
                }
            }
            // Check if end_pos is reachable in exactly dice_roll steps
            if (HashMap_find(&reachablePos, end_pos)) {
                // Valid position
                // Print path taken
                print_path(binfo->par_boeg, binfo->locations, 
                                gstate->boeg_pos, end_pos, binfo->nPositions,
                                dice_roll, DEFAULT_COLOR);
                // Otherwise, move boeg to requested position
                gstate->boeg_pos = end_pos;
                
                return CONTINUE;
            } else {
                min_dist = binfo->dist_boeg[offset_board + end_pos];
                printf("\nCannot reach '%s' from '%s' using %d step(s) (min. is %d)\n",
                    binfo->locations[end_pos].name, 
                    binfo->locations[gstate->boeg_pos].name,
                    dice_roll, min_dist);
            }
        }
        
    } else {
        
        reachablePos = Graph_reachable_pos(
                            &binfo->graph_player,
                            gstate->player_pos[player_id], dice_roll,
                            gstate->visited_buf, 
                            gstate->distances_buf);
        // Player position
        player_pos = gstate->player_pos[player_id];
        // Repeat until user enters valid location
        offset_board = player_pos * binfo->nPositions;
        // Distance from current position of player to boeg
        dist = binfo->dist_player[offset_board + gstate->boeg_pos];
        // Repeat until valid position is entered
        while (true) {
            printf("Enter target location: ");
            // Read chosen end location from command line
            fgets(end_loc, MAX_LOCATION_LEN, stdin);
            // Remove newline
            end_loc[strcspn(end_loc, "\n")] = 0;
            // Find associated index of end location using binary search
            end_pos = location_binsearch(binfo->locations_sorted, end_loc, 
                                                binfo->nPositions);
            printf("\nTarget location: '%s'\n", binfo->locations[end_pos].name);
            // Check if player can reach boeg
            if (end_pos == gstate->boeg_pos && dice_roll >= dist) {
                // Print path taken
                print_path(binfo->par_player, binfo->locations, 
                                player_pos, 
                                gstate->boeg_pos, binfo->nPositions,
                                dist, PLAYER_COLORS[player_id]);
                // Move player to Boeg
                gstate->player_pos[player_id] = gstate->boeg_pos;
                // Update Boeg id
                gstate->boeg_id = player_id;
                // Make next move as Boeg
                return GameState_move_command(binfo, gstate, player_id);
            }
            // Check if end_pos is reachable in exactly dice_roll steps
            if (HashMap_find(&reachablePos, end_pos)) {
                // Valid position
                // Print path taken
                print_path(binfo->par_player, binfo->locations, 
                                player_pos, 
                                end_pos, binfo->nPositions,
                                dice_roll, PLAYER_COLORS[player_id]);
                // Otherwise, move player to requested position
                gstate->player_pos[player_id] = end_pos;
                
                return CONTINUE;
            } else {
                min_dist = binfo->dist_player[player_pos * binfo->nPositions + end_pos];
                printf("\nCannot reach '%s' from '%s' using %d step(s) (min. is %d)\n",
                    binfo->locations[end_pos].name, 
                    binfo->locations[player_pos].name,
                    dice_roll, min_dist);
            }
        }
    }
}

// Make move based on provided strategy
enum STATUS GameState_move(const BoardInfo_t *binfo, GameState_t *gstate, 
                            unsigned int player_id, double avoidance,
                                enum MOVE_STRATEGY move_strat, bool verbose) {
    switch (move_strat) {
        case GREEDY:
            return GameState_move_greedy(binfo, gstate, player_id, verbose);
        case AVOIDANT:
            return GameState_move_avoidant(binfo, gstate, player_id, avoidance, verbose);
        case USER_COMMAND:
            return GameState_move_command(binfo, gstate, player_id);
        default:
            return INVALID;
    }
}

// Run game for at most MAX_TURNS
GameResult_t GameState_run(const BoardInfo_t *binfo, GameState_t *gstate, 
        const enum MOVE_STRATEGY *player_strategies, bool verbose) {
    int winner = -1;
    unsigned int i, j;
    unsigned int player_id;
    unsigned int nTurns = 1;
    enum STATUS status;
    enum MOVE_STRATEGY move_strat;
    // Special game parameter
    const double avoidance = 40.0;
    unsigned int ranking[MAX_PLAYERS] = {0};
    unsigned int nFinished = 0;  // how many players have finished
    
    if (verbose) {
        printf("--Beginning Game--\n\n");
        printf("Player order: ");
        for (i = 0; i < gstate->nPlayers; ++i) {
            player_id = gstate->player_order[i];
            printf("%s%u%s ", PLAYER_COLORS[player_id], player_id+1, DEFAULT_COLOR);
        }
        printf("\n");
        // Print strategies of individual players
        for (i = 0; i < gstate->nPlayers; ++i) {
            player_id = gstate->player_order[i];
            // Retrieve strategy of current player
            move_strat = player_strategies[player_id];
            printf("%sPlayer %u: Strategy: %s%s\n", PLAYER_COLORS[player_id], 
                player_id+1, STRATEGY_NAMES[move_strat], DEFAULT_COLOR);
        }
    }
    // Print initial board for command line players
    for (i = 0; i < gstate->nPlayers; ++i) {
        player_id = gstate->player_order[i];
        // Retrieve strategy of current player
        move_strat = player_strategies[player_id];
        if (move_strat == USER_COMMAND) {
            GameState_info(binfo, gstate, player_id);
        }
    }
    
    while (nTurns < MAX_TURNS) {
        if (verbose)
            printf("\nRound: %u\n", nTurns);
        
        // All players that are actively playing make their move
        for (i = 0; i < gstate->nPlayers; ++i) {
            player_id = gstate->player_order[i];
            // If player has already finished, move on to next player
            if (!AP_ISSET(gstate->active_players, player_id)) {
                continue;
            }
            // Retrieve strategy of current player
            move_strat = player_strategies[player_id];
            // Print board info
            if (move_strat == USER_COMMAND) {
                printf("\nBoard info:\n");
                // Print current state of game
                GameState_info(binfo, gstate, player_id);
            }
            // Player makes move
            status = GameState_move(binfo, gstate, player_id, avoidance, move_strat, verbose);
            // DEBUG
            assert(status != INVALID);
            // Check if game is over
            if (status == GAMEOVER) {
                if (nFinished == 0) {
                    // First player to reach game over is winner
                    winner = player_id;
                }
                // Reset boeg ID to default
                assert(player_id == gstate->boeg_id);
                gstate->boeg_id = BOEG_ID_DEFAULT;
                // Remove this player from set of active players
                AP_UNSET(gstate->active_players, player_id);
                // Update ranking of players
                ranking[nFinished++] = player_id;
                
                if (nFinished == gstate->nPlayers - 1) {
                    // Determine last place (last set bit in active_players)
                    for (j = 0; j < gstate->nPlayers; ++j) {
                        if (AP_ISSET(gstate->active_players, j)) {
                            ranking[nFinished++] = j;
                            break;  // found
                        }
                    }
                    
                    // Game is decided -> QUIT
                    goto end;
                }
            }
        }
        
        ++nTurns;
    }
    
end:
    // Check if maximum turns reached AND no player finished
    if (winner == -1 && nTurns == MAX_TURNS) {
        fprintf(stderr, "\nReached maximum turns!\n");
    } else if (verbose) {
        assert(winner != -1);
        // Print result of game
        printf("\nWINNER: %sPlayer %u%s\n", PLAYER_COLORS[winner],
                        winner+1, DEFAULT_COLOR);
        for (i = 1; i < nFinished; ++i) {
            player_id = ranking[i];
            printf("%u. Place: %sPlayer %u%s\n", i+1, 
                PLAYER_COLORS[player_id], player_id+1, DEFAULT_COLOR);
        }
    }
    return (GameResult_t) {.winner=winner, .nTurns=nTurns};
}

// Compute statistics (max., min. & avg. #turns as well as #wins of
// individual players)
int GameState_statistics(const BoardInfo_t *binfo, GameState_t *gstate, 
        const enum MOVE_STRATEGY *player_strategies, unsigned int nGames) {
    
    unsigned int i;
    for (i = 0; i < gstate->nPlayers; ++i) {
        if (player_strategies[i] == USER_COMMAND) {
            return -1;  // invalid player strategy
        }
    }
    
    unsigned int *wins = (unsigned int *) calloc(gstate->nPlayers, sizeof(unsigned int));
    assert(wins != NULL);
    
    unsigned int max_turns = 0;
    unsigned int min_turns = MAX_TURNS + 1;
    double avg_turns = 0.;
    
    for (i = 0; i < nGames; ++i) {
        GameResult_t result = GameState_run(binfo, gstate, player_strategies, false);
        // Update wins
        if (result.winner != -1) {
            ++wins[result.winner];
        }
        
        // Update turn number statistics
        if (result.nTurns < min_turns) {
            min_turns = result.nTurns;
        }
        if (result.nTurns > max_turns) {
            max_turns = result.nTurns;
        }
        avg_turns += result.nTurns;
        // Reset game state and randomize for next game
        GameState_reset(gstate, binfo->nPositions);
    }
    avg_turns /= (double)nGames;
    
    printf("Total games played: %u\n", nGames);
    printf("\nStatistics:\n");
    for (i = 0; i < gstate->nPlayers; ++i) {
        printf("%sPlayer: %u\tWins: %u (%.2f%%)%s\n", PLAYER_COLORS[i], 
                    i+1, wins[i], (double)wins[i] / nGames * 100., DEFAULT_COLOR);
    }
    printf("Max. turns: %u\n", max_turns);
    printf("Min. turns: %u\n", min_turns);
    printf("Avg. turns: %.2f\n", avg_turns);
    // Cleanup
    free(wins);
    
    return 0;  // ok
}

#endif /* GAME_STATE_H */
