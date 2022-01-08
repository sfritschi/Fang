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
#include <stdbool.h>
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

// Colors used for terminal output
const char *DEFAULT_COLOR = "\033[0m";
const char *PLAYER_COLORS[MAX_PLAYERS] = {
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
    GAMEOVER
};

// Encodes all information of current state of the game
typedef struct {
    unsigned int targets[N_TARGETS];
    unsigned int *player_pos;
    unsigned int *player_targets;
    unsigned int *player_targets_left;
    Location_t *locations;
    Location_t *locations_sorted;
    // Arrays containing shortest path data
    int *dist_player, *dist_boeg;
    int *par_player, *par_boeg;
    // Graphs (adjacency lists)
    Graph graph_player, graph_boeg;
    // Auxiliary buffers needed for graph algorithms
    bool *visited_buf;
    int *distances_buf;
    unsigned int boeg_pos;
    unsigned int boeg_id;  // Keeps track which player is currently the boeg
    unsigned int nPositions;
    unsigned int nPlayers;
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
        if (j != player_id && gstate->player_pos[j] == target) {
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

// Initialize game state based on number of players
void GameState_init(GameState_t *gstate, unsigned int nPlayers) {
    // -- Initialize Game State data --
    gstate->player_pos = (unsigned int *) malloc(nPlayers * sizeof(unsigned int));
    assert(gstate->player_pos != NULL);
    gstate->player_targets = (unsigned int *) 
            malloc(nPlayers * N_TARGETS_PLAYER * sizeof(unsigned int));
    assert(gstate->player_targets != NULL);
    gstate->player_targets_left = (unsigned int *)
            malloc(nPlayers * sizeof(unsigned int));
    assert(gstate->player_targets_left != NULL);
    
    // Initialize number of players
    gstate->nPlayers = nPlayers;
    // Initialize Boeg id
    gstate->boeg_id = BOEG_ID_DEFAULT; 
    unsigned int i, j;
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
    
    // Initialize graphs (adjacency lists)
    FILE *fp_player = fopen("board/graph_player.txt", "r");
    FILE *fp_boeg = fopen("board/graph_full.txt", "r");
    if (fp_player == NULL || fp_boeg == NULL) {
        free(gstate->player_pos);
        free(gstate->player_targets);
        free(gstate->player_targets_left);
        fprintf(stderr, "Could not open board file(s)\n");
        exit(EXIT_FAILURE);
    }
    // Init graphs
    Graph_init_file(&gstate->graph_player, fp_player);
    fclose(fp_player);
    Graph_init_file(&gstate->graph_boeg, fp_boeg);
    fclose(fp_boeg);
    
    // Make sure graphs share same number of vertices
    assert(gstate->graph_player.nVert == gstate->graph_boeg.nVert);
    // Number of total vertices
    const unsigned int nVert = gstate->graph_player.nVert;
    // Set number of positions
    gstate->nPositions = nVert;
    
    // Place players randomly & set initial number of targets
    for (i = 0; i < nPlayers; ++i) {
        // Place players ONLY on non-target positions to avoid
        // possible collisions with placement of Boeg
        gstate->player_pos[i] = (rand() % (nVert - N_TARGETS)) + N_TARGETS;
        gstate->player_targets_left[i] = N_TARGETS_PLAYER;
    }
    // Read locations:
    gstate->locations = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(gstate->locations != NULL);
    gstate->locations_sorted = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(gstate->locations_sorted != NULL);
    
    FILE *fp_loc = fopen("board/locations.txt", "r");
    if (fp_loc == NULL) {
        free(gstate->player_pos);
        free(gstate->player_targets);
        free(gstate->player_targets_left);
        free(gstate->locations);
        free(gstate->locations_sorted);
        Graph_free(&gstate->graph_player);
        Graph_free(&gstate->graph_boeg);
        fprintf(stderr, "Could not open locations file\n");
        exit(EXIT_FAILURE);
    }
    // Read locations from file into locations array
    read_locations(fp_loc, gstate->locations, gstate->locations_sorted);
    // Close locations file
    fclose(fp_loc);
    // Sort locations array in ascending order
    qsort((void *)&gstate->locations_sorted[0], nVert,
                                    sizeof(Location_t), &location_cmp);
                            
    // Initialize distances and parents using BFS APSP
    gstate->dist_player = (int *) malloc(nVert * nVert * sizeof(int));
    assert(gstate->dist_player != NULL);
    gstate->par_player = (int *) malloc(nVert * nVert * sizeof(int));
    assert(gstate->par_player != NULL);
    
    gstate->dist_boeg = (int *) malloc(nVert * nVert * sizeof(int));
    assert(gstate->dist_boeg != NULL);
    gstate->par_boeg = (int *) malloc(nVert * nVert * sizeof(int));
    assert(gstate->par_boeg != NULL);
    
    // Compute all pairs shortest paths (APSP) for both boards
    Graph_BFS_APSP(&gstate->graph_player, gstate->dist_player, gstate->par_player);
    Graph_BFS_APSP(&gstate->graph_boeg, gstate->dist_boeg, gstate->par_boeg);
    
    // Initialize auxiliary buffers
    gstate->visited_buf = NULL;
    gstate->distances_buf = NULL;
}

// Reset game state and re-randomize for next round
void GameState_reset(GameState_t *gstate) {
    unsigned int i, j;
    for (i = 0; i < gstate->nPlayers; ++i) {
        // Place players ONLY on non-target positions to avoid
        // possible collisions with placement of Boeg
        gstate->player_pos[i] = (rand() % (gstate->nPositions - N_TARGETS)) + N_TARGETS;
        gstate->player_targets_left[i] = N_TARGETS_PLAYER;
    }
    // Reset (static) targets
    for (i = 0; i < N_TARGETS; ++i) {
        gstate->targets[i] = i;
    }
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
void GameState_info(const GameState_t *gstate) {
    assert(gstate != NULL);
    
    unsigned int pos;
    unsigned int i, j;
    printf("\nPlayer pos:\n");
    for (i = 0; i < gstate->nPlayers; ++i) {
        pos = gstate->player_pos[i];
        print_colored(gstate->locations[pos].name, PLAYER_COLORS[i]);
        printf("\n");
    }
    printf("\nPlayer targets:\n");
    unsigned int offset = 0;
    for (i = 0; i < gstate->nPlayers; ++i) {
        for (j = 0; j < N_TARGETS_PLAYER; ++j) {
            pos = gstate->player_targets[offset + j];
            if (pos == N_TARGETS) {
                continue;
            }
            print_colored(gstate->locations[pos].name, PLAYER_COLORS[i]);
            printf("\n");
        }
        printf("\n");
        offset += N_TARGETS_PLAYER;
    }
    printf("Boeg pos:\n");
    printf("%s\n\n", gstate->locations[gstate->boeg_pos].name);
}

// Cleanup
void GameState_free(GameState_t *gstate) {
    assert(gstate != NULL);
    
    free(gstate->player_pos);
    free(gstate->player_targets);
    free(gstate->player_targets_left);
    free(gstate->locations);
    free(gstate->locations_sorted);
    free(gstate->dist_player);
    free(gstate->dist_boeg);
    free(gstate->par_player);
    free(gstate->par_boeg);
    // Clean up graphs
    Graph_free(&gstate->graph_player);
    Graph_free(&gstate->graph_boeg);
    // Clean up auxiliary buffers
    free(gstate->visited_buf);
    free(gstate->distances_buf);
}

// GREEDY STRATEGY:
// Always move to closest target using shortest path
enum STATUS GameState_move_greedy(GameState_t *gstate, unsigned int player_id,
                                    int verbose) {
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
        printf("%u> Dice Roll: %d\n", player_id, dice_roll);
    // Check if playing as Boeg
    if (player_id == gstate->boeg_id) {
        offset_targets = player_id * N_TARGETS_PLAYER;
        offset_board = gstate->boeg_pos * gstate->nPositions;
        // Check all remaining targets if reachable
        for (i = 0; i < N_TARGETS_PLAYER; ++i) {
            target = gstate->player_targets[offset_targets + i];
            // If target invalid -> skip
            if (target == N_TARGETS) {
                continue;  // move on to next target
            }
            
            // Distance from current pos to target
            dist = gstate->dist_boeg[offset_board + target];
            if (dice_roll >= dist) {  // Found reachable target
                // Check if target is already occupied -> skip
                if (opponent_at_target(gstate, target, player_id)) {
                    continue;
                }
                // DEBUG
                if (verbose) {
                    print_path(gstate->par_boeg, gstate->locations, 
                            gstate->boeg_pos, target, gstate->nPositions,
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
            closest_pos = follow_path(gstate->par_boeg, gstate->boeg_pos, min_target,
                                        gstate->nPositions, dice_roll);
        }
        // EDGE CASE: Check if already occupied by opponent(s)
        if (min_target == N_TARGETS || opponent_at_target(gstate, closest_pos, player_id)) {
            // Try to move to different location 'dice_roll' away
            // that is as close as possible
            if (verbose)
                printf("Occupied...\n");
                
            closest_pos = gstate->nPositions;
            // Consider all possible locations that are in reach
            unsigned int offset;
            int sum_dists;
            int min_sum = RAND_MAX;
            // Obtain HashMap of all reachable positions from current pos
            // in exactly 'dice_roll' steps
            HashMap reachablePos;
            reachablePos = Graph_reachable_pos(&gstate->graph_boeg,
                                gstate->boeg_pos, dice_roll,
                                 &gstate->visited_buf, 
                                 &gstate->distances_buf);
            // Iterate over all reachable positions
            size_t current;
            const size_t nReachable = HashMap_size(&reachablePos);
            for (current = 0; current < nReachable; ++current) {
                j = HashMap_get(&reachablePos, current);
                // Make sure no opponent is already at current pos
                if (!opponent_at_target(gstate, j, player_id)) {
                    // Compute offset
                    offset = j * gstate->nPositions;
                    // Iterate over all targets and sum min distances
                    sum_dists = 0;
                    for (i = 0; i < N_TARGETS_PLAYER; ++i) {
                        target = gstate->player_targets[offset_targets + i];
                        // Make sure target is valid
                        if (target == N_TARGETS) {
                            continue;
                        }
                        dist = gstate->dist_boeg[offset + target];
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
        if (closest_pos != gstate->nPositions) {
            // Update Boeg position and print path taken
            if (verbose) {
                print_path(gstate->par_boeg, gstate->locations, 
                            gstate->boeg_pos, closest_pos, gstate->nPositions,
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
        offset_board = gstate->player_pos[player_id] * gstate->nPositions;
        // Move to closest position of Boeg
        // Distance between player and Boeg
        dist = gstate->dist_player[offset_board + gstate->boeg_pos];
        if (dice_roll >= dist) {
            // DEBUG
            if (verbose) {
                print_path(gstate->par_player, gstate->locations, 
                            current_pos, gstate->boeg_pos, gstate->nPositions,
                            dist, PLAYER_COLORS[player_id]);
            }
            // Move player to Boeg
            gstate->player_pos[player_id] = gstate->boeg_pos;
            // Update Boeg id
            gstate->boeg_id = player_id;
            // Make next move as Boeg
            return GameState_move_greedy(gstate, player_id, verbose);
        }
        // Move as close as possible to boeg
        if (verbose) {
            gstate->player_pos[player_id] = print_path(gstate->par_player, gstate->locations, 
                            current_pos, gstate->boeg_pos, gstate->nPositions,
                            dice_roll, PLAYER_COLORS[player_id]);
        } else {
            gstate->player_pos[player_id] = follow_path(gstate->par_player, 
                    current_pos, gstate->boeg_pos, gstate->nPositions, dice_roll);
        }
        return CONTINUE;
    }
}

// Avoid opponents when playing as Boeg, while still minimizing
// distance to targets left
enum STATUS GameState_move_avoidant(GameState_t *gstate, unsigned int player_id,
                                    double avoidance, int verbose) {
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
        printf("%u> Dice Roll: %d\n", player_id, dice_roll);
    // Check if playing as Boeg
    if (player_id == gstate->boeg_id) {
        offset_targets = player_id * N_TARGETS_PLAYER;
        offset_board = gstate->boeg_pos * gstate->nPositions;
        // Check all remaining targets if reachable
        for (i = 0; i < N_TARGETS_PLAYER; ++i) {
            target = gstate->player_targets[offset_targets + i];
            // If target invalid -> skip
            if (target == N_TARGETS) {
                continue;  // move on to next target
            }
            
            // Distance from current pos to target
            dist = gstate->dist_boeg[offset_board + target];
            if (dice_roll >= dist) {  // Found reachable target
                // Check if target is already occupied -> skip
                if (opponent_at_target(gstate, target, player_id)) {
                    continue;
                }
                // DEBUG
                if (verbose) {
                    print_path(gstate->par_boeg, gstate->locations, 
                            gstate->boeg_pos, target, gstate->nPositions,
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
        optimal_pos = gstate->nPositions;
        // Consider all possible locations that are in reach
        unsigned int offset;
        double objective;
        double min_objective = INFINITY;
        // Obtain HashMap of all reachable positions from current pos
        // in exactly 'dice_roll' steps
        HashMap reachablePos;
        reachablePos = Graph_reachable_pos(&gstate->graph_boeg,
                            gstate->boeg_pos, dice_roll,
                                &gstate->visited_buf, 
                                &gstate->distances_buf);
        // Iterate over all reachable positions and evaluate objective
        size_t current;
        const size_t nReachable = HashMap_size(&reachablePos);
        for (current = 0; current < nReachable; ++current) {
            j = HashMap_get(&reachablePos, current);
            // Make sure no opponent is already at current pos
            if (!opponent_at_target(gstate, j, player_id)) {
                // Compute offset
                offset = j * gstate->nPositions;
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
                    dist = gstate->dist_boeg[offset + target];
                    // Update objective
                    objective += (double)dist;
                }
                // Iterate over all opponent positions and update objective
                for (i = 0; i < gstate->nPlayers; ++i) {
                    // Ignore self
                    if (i == player_id) {
                        continue;
                    }
                    unsigned int opp_pos = gstate->player_pos[i];
                    // Compute shortest distance from opponent to
                    // candidate position
                    int opp_dist = gstate->dist_player[opp_pos * gstate->nPositions + j];
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
        if (optimal_pos != gstate->nPositions) {
            // Update Boeg position and print path taken
            if (verbose) {
                print_path(gstate->par_boeg, gstate->locations, 
                            gstate->boeg_pos, optimal_pos, gstate->nPositions,
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
        offset_board = gstate->player_pos[player_id] * gstate->nPositions;
        // Move to closest position of Boeg
        // Distance between player and Boeg
        dist = gstate->dist_player[offset_board + gstate->boeg_pos];
        if (dice_roll >= dist) {
            // DEBUG
            if (verbose) {
                print_path(gstate->par_player, gstate->locations, 
                            current_pos, gstate->boeg_pos, gstate->nPositions,
                            dist, PLAYER_COLORS[player_id]);
            }
            // Move player to Boeg
            gstate->player_pos[player_id] = gstate->boeg_pos;
            // Update Boeg id
            gstate->boeg_id = player_id;
            // Make next move as Boeg
            return GameState_move_avoidant(gstate, player_id, avoidance, verbose);
        }
        // Move as close as possible to boeg
        if (verbose) {
            gstate->player_pos[player_id] = print_path(gstate->par_player, gstate->locations, 
                            current_pos, gstate->boeg_pos, gstate->nPositions,
                            dice_roll, PLAYER_COLORS[player_id]);
        } else {
            gstate->player_pos[player_id] = follow_path(gstate->par_player, 
                    current_pos, gstate->boeg_pos, gstate->nPositions, dice_roll);
        }
        return CONTINUE;
    }
}

// Run game for at most MAX_TURNS
GameResult_t GameState_run(GameState_t *gstate, bool verbose) {
    int winner = -1;
    unsigned int i;
    unsigned int nTurns = 0;
    enum STATUS status = CONTINUE;
    if (verbose) {
        printf("--Beginning Game--\n\n");
        GameState_info(gstate);
    }
    // Special game parameters
    const unsigned int avoidant_player = 0;
    const double avoidance = 40.0;
    
    while (status != GAMEOVER && nTurns < MAX_TURNS) {
        if (verbose)
            printf("Round: %u\n", nTurns + 1);
        
        for (i = 0; i < gstate->nPlayers; ++i) {
            // Player makes move
            if (i == avoidant_player) {
                status = GameState_move_avoidant(gstate, i, avoidance, verbose);
            } else {
                status = GameState_move_greedy(gstate, i, verbose);
            }
            
            if (verbose) {
                printf("\nBoard info:\n");
                // Print current state of game
                GameState_info(gstate);
            }
            if (status == GAMEOVER) {
                winner = i;
                break;
            }
        }
        if (verbose)
            printf("\n");
        
        ++nTurns;
    }
    // Check if maximum turns reached
    if (nTurns == MAX_TURNS) {
        fprintf(stderr, "\nReached maximum turns!\n");
    }
    return (GameResult_t) {.winner=winner, .nTurns=nTurns};
}

// Compute statistics (max., min. & avg. #turns as well as #wins of
// individual players)
void GameState_statistics(GameState_t *gstate, unsigned int nGames) {
    unsigned int *wins = (unsigned int *) calloc(gstate->nPlayers, sizeof(unsigned int));
    assert(wins != NULL);
    
    unsigned int max_turns = 0;
    unsigned int min_turns = MAX_TURNS + 1;
    double avg_turns = 0.;
    
    GameResult_t result;
    unsigned int i;
    for (i = 0; i < nGames; ++i) {
        result = GameState_run(gstate, false);
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
        GameState_reset(gstate);
    }
    avg_turns /= (double)nGames;
    
    printf("Total games played: %u\n", nGames);
    printf("\nStatistics:\n");
    for (i = 0; i < gstate->nPlayers; ++i) {
        printf("%sPlayer: %u\tWins: %u (%.2f%%)%s\n", PLAYER_COLORS[i], 
                    i, wins[i], (double)wins[i] / nGames * 100., DEFAULT_COLOR);
    }
    printf("Max. turns: %u\n", max_turns);
    printf("Min. turns: %u\n", min_turns);
    printf("Avg. turns: %.2f\n", avg_turns);
    // Cleanup
    free(wins);
}

#endif /* GAME_STATE_H */
