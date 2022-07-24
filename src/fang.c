/*
 * Main driver program for board game 'Fang de Boeg'.
 *
 */

#include <stdio.h>
#include <time.h>  // time()
#include <stdlib.h>

#include "game_state.h"
#include "splitmix64.h"

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: ./fang <num_players %d:%d> <list of player strategies (a/g/u)>\n",
                                MIN_PLAYERS, MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
    
    // Seed (pseudo-) random number generator
    set_seed(time(NULL));
    //set_seed(42);
    
    unsigned int nPlayers = atoi(argv[1]);
    if (!(MIN_PLAYERS <= nPlayers && nPlayers <= MAX_PLAYERS)) {
        fprintf(stderr, "Invalid number of players\n");
        fprintf(stderr, "Usage: ./fang <num_players %d:%d> <list of player strategies (a,g,u)>\n",
                                MIN_PLAYERS, MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
    printf("#Players: %u\n", nPlayers);
    
    if (argc - 2 != (int)nPlayers) {
        fprintf(stderr, "Need to specify list of player strategies for all %u players\n", nPlayers);
        fprintf(stderr, "Supported strategies: a(voidant), g(reedy), u(ser_command)\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize player strategies
    enum MOVE_STRATEGY *player_strategies =
            (enum MOVE_STRATEGY *) malloc(nPlayers * sizeof(enum MOVE_STRATEGY));
    assert(player_strategies != NULL);
    
    unsigned int i;
    for (i = 0; i < nPlayers; ++i) {
        // Read first character from individual arguments
        char strat = argv[i + 2][0];
        switch (strat) {
            case 'a':
                player_strategies[i] = AVOIDANT;
                break;
            case 'g':
                player_strategies[i] = GREEDY;
                break;
            case 'u':
                player_strategies[i] = USER_COMMAND;
                break;
            default:
                // Unrecognized strategy -> exit
                fprintf(stderr, "Did not recognize option: '%c'\n", strat);
                free(player_strategies);
                exit(EXIT_FAILURE);
        }
    }
    // Initialize board
    BoardInfo_t binfo;
    BoardInfo_init(&binfo);
    
    // Initialize game state
    GameState_t gstate;
    GameState_init(&gstate, nPlayers, binfo.nPositions);
    
    GameState_run(&binfo, &gstate, player_strategies, false, true);
    //GameState_statistics(&binfo, &gstate, player_strategies, 1024);
    // Clean up board info and game state
    BoardInfo_free(&binfo);
    GameState_free(&gstate);
    // Clean up
    free(player_strategies);
    
    return EXIT_SUCCESS;
}

int main0(int argc, char *argv[]) {
    
    if (argc != 3) {
        fprintf(stderr, "Usage: ./fang <start> <n-steps>\n");
        exit(EXIT_FAILURE);
    }
    
    const char *start = argv[1];
    int distance = atoi(argv[2]);
    
    // Initialize player graph
    Graph graph_player;
    FILE *fp = fopen("board/graph_player.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Could not open player board\n");
        exit(EXIT_FAILURE);
    }
    // Initialize graph from file
    Graph_init_file(&graph_player, fp);
    fclose(fp);
    // Number of vertices of graph
    const unsigned int nVert = graph_player.nVert;
    // Find out index of source
    // Read locations:
    Location_t *locations = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(locations != NULL);
    Location_t *locations_sorted = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(locations_sorted != NULL);
    
    fp = fopen("board/locations.txt", "r");
    if (fp == NULL) {
        free(locations);
        free(locations_sorted);
        fprintf(stderr, "Could not open locations file\n");
        exit(EXIT_FAILURE);
    }
    // Read locations from file into locations array
    read_locations(fp, locations, locations_sorted);
    // Close locations file
    fclose(fp);
    // Sort locations array in ascending order
    qsort((void *)&locations_sorted[0], nVert,
                            sizeof(Location_t), &location_cmp);
    
    unsigned int source = location_binsearch(locations_sorted, start, nVert);
    
    // Work space
    bool *visited_buf = (bool *) malloc(nVert * sizeof(bool));
    assert(visited_buf != NULL);
    int *distances_buf = (int *) malloc(nVert * sizeof(int));
    assert(distances_buf != NULL);
    
    HashMap reachablePos;
    reachablePos = Graph_reachable_pos(&graph_player, source, distance, 
                                         visited_buf, distances_buf);
    
    printf("Reachable locations (%lu) from '%s' using exactly %d steps:\n\n", 
                HashMap_size(&reachablePos), locations[source].name, distance);
    
    for (size_t i = 0; i < reachablePos.size; ++i) {
        printf("%s\n", locations[reachablePos.map[reachablePos.index[i]]].name);
    }
    
    // Free locations
    free(locations);
    free(locations_sorted);
    
    // Free work space
    free(visited_buf);
    free(distances_buf);
    
    // Cleanup
    Graph_free(&graph_player);
    
    return 0;    
}

int main1(int argc, char *argv[]) {
    
    if (argc != 4) {
        fprintf(stderr, "Usage: ./fang <start> <end> <n-steps>\n");
        exit(EXIT_FAILURE);
    }
    
    const char *start = argv[1];
    const char *end = argv[2];
    int distance = atoi(argv[3]);
    
    // Initialize player graph
    Graph graph_player;
    FILE *fp = fopen("board/graph_player.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Could not open player board\n");
        exit(EXIT_FAILURE);
    }
    // Initialize graph from file
    Graph_init_file(&graph_player, fp);
    fclose(fp);
    // Number of vertices of graph
    const unsigned int nVert = graph_player.nVert;
    // Find out index of source
    // Read locations:
    Location_t *locations = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(locations != NULL);
    Location_t *locations_sorted = (Location_t *) malloc(nVert * sizeof(Location_t));
    assert(locations_sorted != NULL);
    
    fp = fopen("board/locations.txt", "r");
    if (fp == NULL) {
        free(locations);
        free(locations_sorted);
        fprintf(stderr, "Could not open locations file\n");
        exit(EXIT_FAILURE);
    }
    // Read locations from file into locations array
    read_locations(fp, locations, locations_sorted);
    // Close locations file
    fclose(fp);
    // Sort locations array in ascending order
    qsort((void *)&locations_sorted[0], nVert,
                            sizeof(Location_t), &location_cmp);
    
    unsigned int source = location_binsearch(locations_sorted, start, nVert);
    unsigned int target = location_binsearch(locations_sorted, end, nVert);
    
    // Work space
    bool *visited_buf = (bool *) malloc(nVert * sizeof(bool));
    assert(visited_buf != NULL);
    int *distances_buf = (int *) malloc(nVert * sizeof(int));
    assert(distances_buf != NULL);
    
    bool reachable;
    reachable = Graph_is_reachable(&graph_player, source, target, distance, 
                                        visited_buf, distances_buf);
    // Report
    printf("'%s' (%u) reachable from '%s' (%u) with exactly %d steps: ", 
            locations[target].name, target, locations[source].name, source, distance);
    if (reachable)
        printf("yes\n");
    else
        printf("no\n");
    
    // Free locations
    free(locations);
    free(locations_sorted);
    
    // Free work space
    free(visited_buf);
    free(distances_buf);
    
    // Cleanup
    Graph_free(&graph_player);
    
    return 0;    
}
