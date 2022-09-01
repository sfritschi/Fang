/*
 * Main driver program for board game 'Fang de Boeg'.
 *
 */

#include <stdio.h>
#include <time.h>  // time()
#include <stdlib.h>

#include "game_state.h"
#include "graphics.h"
#include "splitmix64.h"

#define TEXT_BUF_SIZE 32

static unsigned int nNodes = 0;
static unsigned int nEdges = 0;
static BoardInfo_t binfo;
static GameState_t gstate;
static unsigned int _userId = MAX_PLAYERS;
static vec3 targetBgCol[N_TARGETS_PLAYER];
static const char *locationText = "";

static GLboolean _isAlternating = GL_FALSE;

void alternateColors(int value)
{
    // ISSUE: Possible data-race on global variables?
    if (_isAlternating) {
        vec3 col;
        
        for (GLuint i = 0; i < _sm.size; ++i) {
            SearchMapEntry *sme = SM_get(&_sm, i);

            // Alternate color of individual players
            uint8_t playerId = BS_nextPos(&sme->bs);
            assert(playerId != BS_INVALID_ELEM);
            glm_vec3_copy(COLORS[playerId], col);
            
            setColor(col, sme->key);
            
            const GLuint offsetTargets = _userId*N_TARGETS_PLAYER;
            for (GLuint j = 0; j < N_TARGETS_PLAYER; ++j) {
                if (gstate.player_targets[offsetTargets + j] == sme->key) {
                    glm_vec3_copy(col, targetBgCol[j]);
                    break;
                }
            }
        }
        glutPostRedisplay();
        // Register next callback
        glutTimerFunc(DELAY_MS, alternateColors, value);
    }
}

void draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    char buf[TEXT_BUF_SIZE];
    const GLfloat targetScale = 0.5f;
    const GLuint offsetTargets = _userId*N_TARGETS_PLAYER;
    for (GLuint i = 0; i < N_TARGETS_PLAYER; ++i) {
        const unsigned int target = 
            gstate.player_targets[offsetTargets + i];
        // Skip non-active targets
        if (target == N_TARGETS) {
            continue;
        }
        snprintf(buf, TEXT_BUF_SIZE, "%u", i + 1);
                
        GLfloat cx = binfo.locations[target].pos[0];
        GLfloat cy = binfo.locations[target].pos[1];
        // Map normalized coordinates to screen range
        cx = 0.5f*ORIGIN_WIDTH*(cx + 1.f);
        cy = 0.5f*ORIGIN_HEIGHT*(cy + 1.f);  // flip y
        // Render number at center of target node
        fontRenderTextCentered(buf, cx, cy, targetScale, 
                               COLORS[_userId], targetBgCol[i]);
    }
    
    // Draw board
    renderBoard(nNodes, nEdges);
    // Write name of location in bottom corner
    fontRenderText(locationText, 20.0f, 20.0f, 0.8f, COLORS[COL_TEXT], COLORS[COL_BG]);
    
    // Write 'Player X.' in top left corner
    snprintf(buf, TEXT_BUF_SIZE, "Player %u.", _userId+1);
    const GLfloat playerScale = 0.8f;
    TextDims td = fontGetTextDims(buf, playerScale);
    fontRenderText(buf, 20.0f, ORIGIN_HEIGHT - td.height, 
                   playerScale, COLORS[_userId], COLORS[COL_BG]);
    
    /*
    playerTxt = "Dice roll:";
    TextDims td2 = fontGetTextDims(playerTxt, playerScale);
    fontRenderText(playerTxt, 20.0f, ORIGIN_HEIGHT - td.height - 1.5f*td2.height,
                   playerScale, COLORS[COL_TEXT], COLORS[COL_BG]);
    
    fontRenderText(" 3", 20.0f + td2.width, ORIGIN_HEIGHT - td.height - 1.5f*td2.height,
                   playerScale, COLORS[COL_RED], COLORS[COL_BG]);
    glUseProgram(_boardShaderProgram);
    */
    // Swap buffers and show the buffer's content on the screen
    glutSwapBuffers();
}

void keyPressed(unsigned char key, int x, int y)
{
    // Unused parameters; don't warn
    (void)x;
    (void)y;
    
    if (key == 'q' || key == 'Q') {
        exit(EXIT_SUCCESS);
    }
}

void mouseClick(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Get current window sizes
        GLfloat width = glutGet(GLUT_WINDOW_WIDTH);
        GLfloat height = glutGet(GLUT_WINDOW_HEIGHT);
        // Map screen (pixel) coordinates to range [-1,1]
        GLfloat xf = 2.f*(GLfloat)x/width - 1.f;
        GLfloat yf = 1.f - 2.f*(GLfloat)y/height;  // flip y
                
        // Iterate through all circles to check if one was clicked (inefficient)
        // (Assume circles do NOT overlap)
        GLfloat radiusSq = RAD_CIRCLE*RAD_CIRCLE;
        
        for (GLuint i = 0; i < nNodes; ++i) {
            GLfloat dx = xf - binfo.locations[i].pos[0];
            GLfloat dy = yf - binfo.locations[i].pos[1];
            
            if ((dx*dx + dy*dy) < radiusSq) {  // Found circle
                // Set clicked location name
                locationText = binfo.locations[i].name;
                // Change color of circle
                setColor(COLORS[_userId], i);
                // DEBUG: Update globalValue (poison pill)
                _isAlternating = GL_FALSE;
                // Check if clicked location is a target location
                const GLuint offsetTargets = _userId*N_TARGETS_PLAYER;
                for (GLuint j = 0; j < N_TARGETS_PLAYER; ++j) {
                    if (gstate.player_targets[offsetTargets + j] == i) {
                        glm_vec3_copy(COLORS[_userId], targetBgCol[j]);
                        break;
                    }
                }
                // Redisplay scene
                glutPostRedisplay();
                return;  // done
            }
        }
    }
}

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: ./fang <num_players %d:%d> <list of player strategies (a/g/u)>\n",
                                MIN_PLAYERS, MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
    
    // Seed (pseudo-) random number generator
    //set_seed(time(NULL));
    set_seed(42);
    
    unsigned int nPlayers = atoi(argv[1]);
    if (!(MIN_PLAYERS <= nPlayers && nPlayers <= MAX_PLAYERS)) {
        fprintf(stderr, "Invalid number of players\n");
        fprintf(stderr, "Usage: ./fang <num_players %d:%d> <list of player strategies (a,g,u)>\n",
                                MIN_PLAYERS, MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
    printf("#Players: %u\n", nPlayers);
    
    if (argc - 2 != (int)nPlayers) {
        fprintf(stderr, "Need to specify list of player strategies for exactly %u players\n", nPlayers);
        fprintf(stderr, "Supported strategies: a(voidant), g(reedy), u(ser_command)\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize player strategies
    enum MOVE_STRATEGY *player_strategies =
            (enum MOVE_STRATEGY *) malloc(nPlayers*sizeof(enum MOVE_STRATEGY));
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
    
    unsigned int userCount = 0;
    for (i = 0; i < nPlayers; ++i) {
        if (player_strategies[i] == USER_COMMAND) {
            _userId = i;
            ++userCount;
        }
        // Make sure only 1 user
        if (userCount > 1) {
            fprintf(stderr, "Multiple users not yet supported.\n");
            free(player_strategies);
            exit(EXIT_FAILURE);
        }
    }
    // Verify that exactly 1 user was specified
    if (_userId == MAX_PLAYERS) {
        fprintf(stderr, "No user specified, exiting...\n");
        free(player_strategies);
        exit(EXIT_FAILURE);
    }
    
    // Initialize board
    BoardInfo_init(&binfo);
    nNodes = binfo.nPositions;
    nEdges = binfo.graph.nEdge;
    
    // Initialize game state
    GameState_init(&gstate, nPlayers, nNodes);
    
    // Initialize board
    initBoardGL(&argc, argv, "fonts/LiberationMono-Regular.ttf", &binfo);
    
    // Initialize target background color
    for (GLuint i = 0; i < N_TARGETS_PLAYER; ++i)
        glm_vec3_copy(COLORS[COL_TARGET], targetBgCol[i]);
    
    populateSearchMap(&gstate);
    
    // Setup function callbacks
    glutDisplayFunc(draw);
    glutKeyboardFunc(keyPressed);
    glutMouseFunc(mouseClick);
    
    _isAlternating = GL_TRUE;
    glutTimerFunc(1, alternateColors, 0);
    
    // Start main loop
    glutMainLoop();
    
    //GameState_run(&binfo, &gstate, player_strategies, false, true);
    //GameState_statistics(&binfo, &gstate, player_strategies, 1024);
    // Clean up board info and game state
    BoardInfo_free(&binfo);
    GameState_free(&gstate);
    // Clean up
    free(player_strategies);
    
    return EXIT_SUCCESS;
}

/*
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
*/
