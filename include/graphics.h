#include "GL/glew.h"
#include "GL/gl.h"
#include "GL/freeglut.h"

#include <cglm/cglm.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "gl_common.h"
#define ORIGIN_WIDTH 1200
#define ORIGIN_HEIGHT 1000
#include "font.h"

#include "searchmap.h"
#include "bitset.h"

#include "game_state.h"
#include "location.h"
#include "graph.h"
#include "linked_list.h"

#define N_TRIANGLES_CIRCLE 32
#define BSIZE_VERT_POS (2*(N_TRIANGLES_CIRCLE + 2))
#define RAD_CIRCLE 0.03f
#define EDGE_WIDTH 3.0f
#define INSTANCE_LENGTH 64
#define DELAY_MS 1000

// Source of vertex shader in glsl
#define VERT_SHADER_LENGTH 1024
static GLchar _boardVertShaderSource[VERT_SHADER_LENGTH];

static const GLchar *_boardVertShaderTemplate = R"glsl(
#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aOffset;
uniform bool isInstanced;
uniform vec3 circInstanceColors[%d];
out vec3 vertexColor;
void main() {
    vec2 finalPos = aPos;
    if (isInstanced) {
        finalPos = finalPos + aOffset;
    }
    gl_Position = vec4(finalPos, 0.0, 1.0);
    
    if (isInstanced) {
        vertexColor = circInstanceColors[gl_InstanceID];
    } else {
        vertexColor = aColor;
    }
}
)glsl";

// Source of fragment shader in glsl
static const GLchar *_boardFragShaderSource = R"glsl(
#version 460 core
out vec4 fragColor;
in vec3 vertexColor;
void main() {
    fragColor = vec4(vertexColor, 1.0);
}
)glsl";

typedef struct {
    vec2 pos;
    vec3 col;
} Vertex;

// GLOBALS
static GLuint _boardShaderProgram;
static GLuint _vboNodeCirc, _vboNodeOffsets, _vaoNode;
static GLuint _vboEdge, _vaoEdge;

static SearchMap _sm;

// Colors (do not change order)
enum COLS {
    COL_RED,
    COL_GREEN,
    COL_BLUE,
    COL_YELLOW,
    COL_ORANGE,
    COL_PURPLE,
    COL_WHITE,
    COL_BG,
    COL_TEXT,
    COL_TARGET
};
static const vec3 COLORS[] = {
    {173.f/255.f, 6.f/255.f, 6.f/255.f},
    {27.f/255.f, 137.f/255.f, 25.f/255.f},
    {25.f/255.f, 26.f/255.f, 177.f/255.f},
    {223.f/255.f, 224.f/255.f, 38.f/255.f},
    {184.f/255.f, 95.f/255.f, 10.f/255.f},
    {97.f/255.f, 24.f/255.f, 184.f/255.f},
    {0.9f, 0.9f, 0.9f},
    {0.7f, 0.7f, 0.7f},
    {0.0f, 0.0f, 0.0f},
    {1.f, 92.f/255.f, 244.f/255.f}
};

// OpenGL helpers
void setIsInstanced(GLboolean flag)
{    
    // Get location for isInstanced from shader program
    GLint isInstancedLoc = glGetUniformLocation(_boardShaderProgram, "isInstanced");
    // Set isInstanced in vertex shader to 'flag'
    glUniform1i(isInstancedLoc, flag);
}

void setColor(const vec3 rgb, GLuint i)
{
    glUseProgram(_boardShaderProgram);
    
    char stringLoc[INSTANCE_LENGTH];
    snprintf(stringLoc, INSTANCE_LENGTH, "circInstanceColors[%u]", i);
    // Get location from shader program
    GLint circColorLoc = glGetUniformLocation(_boardShaderProgram, stringLoc);
    // Set location to color value
    glUniform3f(circColorLoc, rgb[0], rgb[1], rgb[2]);
    
    glUseProgram(0);
}

void initNodeCols(GLuint nNodes)
{
	// Set color of all circle instances to specified color
    for (GLuint i = 0; i < nNodes; ++i) {
        if (i < N_TARGETS) {
            setColor(COLORS[COL_TARGET], i);
        } else {
            setColor(COLORS[COL_TEXT], i);
        }
    }
}

// Initialize vertex positions (circle) common to all nodes
void initVertexPosNodes(GLfloat vertexPos[BSIZE_VERT_POS])
{
	const GLfloat angleInc = 2.0f*GLM_PI/(GLfloat)N_TRIANGLES_CIRCLE;
	GLfloat currentAngle = 0.0f;
    
    GLuint offset = 0;
    
    // Add center point to vertices
    vertexPos[offset    ] = 0.0f;
    vertexPos[offset + 1] = 0.0f;
    // Increment vertex offset
    offset += 2;
    
	for (size_t i = 0; i <= N_TRIANGLES_CIRCLE; ++i) {
		// Add current point on perimeter
		vertexPos[offset    ] = RAD_CIRCLE * cosf(currentAngle);
		vertexPos[offset + 1] = RAD_CIRCLE * sinf(currentAngle);
		// Increment vertex offset
        offset += 2;
        
		// Increment angle (move on to next point on circle perimeter)
		currentAngle += angleInc;
    }
}

void initEdges(Vertex *eVert, const BoardInfo_t *binfo)
{
    
    const Graph *g = &binfo->graph;
    
    unsigned int offset = 0;
    unsigned int from, to;
    for (from = 0; from < g->nVert; ++from) {
        EdgeList iter = g->adjList[from];
        
        while (iter) {
            to = iter->index;
            
            if (from < to) {  // include every edge only once
                // Determine edge color
                vec3 col;
                if (iter->isBoegOnly)
                    glm_vec3_copy(COLORS[COL_WHITE], col);
                else
                    glm_vec3_copy(COLORS[COL_TEXT], col);
                    
                eVert[offset    ].pos[0] = binfo->locations[from].pos[0];
                eVert[offset    ].pos[1] = binfo->locations[from].pos[1];
                glm_vec3_copy(col, eVert[offset].col);
                
                eVert[offset + 1].pos[0] = binfo->locations[to].pos[0];
                eVert[offset + 1].pos[1] = binfo->locations[to].pos[1];
                glm_vec3_copy(col, eVert[offset+1].col);
                // Increment
                offset += 2;
            }
            iter = iter->next;
        }
    }
    assert(offset == 2*g->nEdge);
}

void populateSearchMap(const GameState_t *gstate) 
{
    // Initialize
    SM_init(&_sm);
    int32_t err;
    // Insert positions of all players
    for (uint8_t i = 0; i < gstate->nPlayers; ++i) {
        // Ignore already finished players
        if (!is_active_player(gstate, i))
            continue;
        
        err = SM_insert(&_sm, gstate->player_pos[i], i); assert(err == SM_OK);
    }
    // Insert position of boeg (special case)
    err = SM_insert(&_sm, gstate->boeg_pos, MAX_PLAYERS); assert(err == SM_OK);
}

void initBoardGL(int *argc, char *argv[], const char *fontPath, 
                 const BoardInfo_t *binfo)
{
    // Initialize GL context
	glutInit(argc, argv);
    // Get screen resolution
    GLint screenWidth = glutGet(GLUT_SCREEN_WIDTH);
    GLint screenHeight = glutGet(GLUT_SCREEN_HEIGHT);
    // MULTISAMPLE for anti-aliasing
    glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutInitWindowSize(ORIGIN_WIDTH, ORIGIN_HEIGHT);
    // Place window in center of screen
    glutInitWindowPosition(screenWidth/2 - ORIGIN_WIDTH/2, 
                           screenHeight/2 - ORIGIN_HEIGHT/2);
    glutCreateWindow("Fang Game");
    
    // Initialize GLEW
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        exit(EXIT_FAILURE);	
	}
	glClearColor(COLORS[COL_BG][0], COLORS[COL_BG][1], COLORS[COL_BG][2], 1.0);
	glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize font
    fontInit(fontPath);
    
    // Set number of instances in vertex shader
    snprintf(_boardVertShaderSource, VERT_SHADER_LENGTH,
             _boardVertShaderTemplate, binfo->nPositions);
    // Create shader program
    _boardShaderProgram = createGLProgram(_boardVertShaderSource, 
                                          _boardFragShaderSource);
    // Finally, use shader program
    glUseProgram(_boardShaderProgram);
    
    // Prepare node properties for drawing
    {
        // Initialize vertex properties for circle
        GLfloat vertexPos[BSIZE_VERT_POS];
        initVertexPosNodes(vertexPos);
        // Initialize colors for nodes
        initNodeCols(binfo->nPositions);
        
        glGenVertexArrays(1, &_vaoNode);
        glBindVertexArray(_vaoNode);
        
        // Positions on circle perimeter (shared)
        glGenBuffers(1, &_vboNodeCirc);
        glBindBuffer(GL_ARRAY_BUFFER, _vboNodeCirc);
        glBufferData(GL_ARRAY_BUFFER, BSIZE_VERT_POS*sizeof(GLfloat), 
                     vertexPos, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);  // unbind
        
        // Node offsets
        glGenBuffers(1, &_vboNodeOffsets);
        glBindBuffer(GL_ARRAY_BUFFER, _vboNodeOffsets);
        glBufferData(GL_ARRAY_BUFFER, binfo->nPositions*sizeof(Location_t),
                    binfo->locations, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Location_t),
                              (void *)offsetof(Location_t, pos));
        glEnableVertexAttribArray(2);
        // Set vertex attribute divisor for instanced rendering
        glVertexAttribDivisor(2, 1);
        // unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // DONE nodes
    
    // Initialize edges
    {
        // Allocate temporary buffer
        GLuint nEdges = binfo->graph.nEdge;
        GLuint bufSize = 2*nEdges*sizeof(Vertex);
        Vertex *edgeBuf = (Vertex *) malloc(bufSize);
        assert(edgeBuf != NULL);
        
        initEdges(edgeBuf, binfo);
        
        glGenVertexArrays(1, &_vaoEdge);
        glBindVertexArray(_vaoEdge);
        
        // Edge positions
        glGenBuffers(1, &_vboEdge);
        glBindBuffer(GL_ARRAY_BUFFER, _vboEdge);
        glBufferData(GL_ARRAY_BUFFER, bufSize, edgeBuf, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        
        // Edge colors
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, col));
        glEnableVertexAttribArray(1);
        // unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);  // unbind
        glBindVertexArray(0);
        // Clean-up
        free(edgeBuf);
    }
    // DONE edges
    glUseProgram(0);
}

void renderBoard(GLuint nNodes, GLuint nEdges)
{
    glUseProgram(_boardShaderProgram);
    // Last argument specifies total number of vertices. Three consecutive
    // vertices are drawn as one triangle
    glBindVertexArray(_vaoNode);
    setIsInstanced(GL_TRUE);  // set to true
    // NOTE: GL_TRIANGLE_FAN draws N - 2 triangles -> first point
    //       (after center) must be included at the end as well
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, N_TRIANGLES_CIRCLE + 2, nNodes);
    glBindVertexArray(0);
    
    glBindVertexArray(_vaoEdge);
    glLineWidth(EDGE_WIDTH);
    setIsInstanced(GL_FALSE);  // set to false
    glDrawArrays(GL_LINES, 0, 6*nEdges);
    glBindVertexArray(0);
    
    glUseProgram(0);
}
