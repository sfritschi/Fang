#ifndef FONT_H
#define FONT_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glut.h>

#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include "gl_common.h"

#if !defined(ORIGIN_WIDTH) || !defined(ORIGIN_HEIGHT)
#error "Need to set screen resolution ORIGIN_WIDTH and ORIGIN_HEIGHT."
#endif

#define N_CHARS 128

// Shaders
static const GLchar *_fontVertShaderSource = R"glsl(
#version 460 core
layout(location = 0) in vec4 aVert;  // <vec2 pos, vec2 tex>

out vec2 texCoords;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(aVert.xy, 0.0, 1.0);
    texCoords = aVert.zw;
}
)glsl";

static const GLchar *_fontFragShaderSource = R"glsl(
#version 460 core
in vec2 texCoords;

out vec4 fragColor;

uniform sampler2D text;
uniform vec3 textColor;
uniform vec3 backgroundColor;

void main()
{
    float a = clamp(texture(text, texCoords).r, 0.0, 1.0);
    fragColor = vec4(mix(backgroundColor, textColor, a), 1.0);
}
)glsl";

typedef struct {
    GLfloat width;
    GLfloat height;
} TextDims;

typedef struct {
    GLuint textureId;
    GLuint width;
    GLuint rows;
    GLuint bearingX;
    GLuint bearingY;
    GLuint advance;
} _FontChar;

// Globals
static _FontChar _fontChars[N_CHARS];
static GLuint _fontShaderProgram;
static GLuint _fontVao, _fontVbo;

void fontToTexture(const char *fontPath)
{
    FT_Library ft;
    FT_Error err = FT_Init_FreeType(&ft);
    
    if (err) {
        fprintf(stderr, "Failed to initialize FreeType\n");
        exit(EXIT_FAILURE);
    }
    // Load font face
    FT_Face face;
    err = FT_New_Face(ft, fontPath, 0, &face);
    
    if (err) {
        fprintf(stderr, "Failed to load face\n");
        exit(EXIT_FAILURE);
    }
    // Pixel width and height (48 px)
    err = FT_Set_Pixel_Sizes(face, 0, 48);
    if (err) {
        fprintf(stderr, "Failed to set pixel sizes\n");
        exit(EXIT_FAILURE);
    }
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Iterate over first N_CHARS ascii characters
    for (unsigned char c = 0; c < N_CHARS; ++c) {
        // Render glyph in glyph slot
        err = FT_Load_Char(face, c, FT_LOAD_RENDER);
        if (err) {
            fprintf(stderr, "Failed to load char: %u\n", c);
            exit(EXIT_FAILURE);
        }
        // Create texture from glyph slot
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RED,
                     face->glyph->bitmap.width,
                     face->glyph->bitmap.rows,
                     0,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     face->glyph->bitmap.buffer);
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Store texture information in global variable
        _fontChars[(GLuint)c] = (_FontChar) {
            texture,
            face->glyph->bitmap.width, face->glyph->bitmap.rows,
            face->glyph->bitmap_left, face->glyph->bitmap_top,
            face->glyph->advance.x
        };
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    // Clean-up
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void fontInit(const char *fontPath)
{
    // Initialize shader programs
    _fontShaderProgram = createGLProgram(_fontVertShaderSource, _fontFragShaderSource);
    glUseProgram(_fontShaderProgram);
    // Orthographic projection matrix for text
    mat4 proj;
    glm_ortho(0.0f, ORIGIN_WIDTH, 0.0f, ORIGIN_HEIGHT, -1.0f, 1.0f, proj);
    // Set projection matrix
    GLint projLoc  = glGetUniformLocation(_fontShaderProgram, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, (GLfloat *)proj);
    
    // Create textures for given font
    fontToTexture(fontPath);
    
    // Create vertex buffer arrays
    // - 2D quad with 6 vertices @ 4 components
    // - Dynamic drawing to allow updating content of vbo
    glGenVertexArrays(1, &_fontVao);
    glGenBuffers(1, &_fontVbo);
    glBindVertexArray(_fontVao);
    glBindBuffer(GL_ARRAY_BUFFER, _fontVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*6*4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          4*sizeof(GLfloat),
                          NULL);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);  // unbind
    glUseProgram(0);    
}

// Assumes text is null-terminated
TextDims fontGetTextDims(const char *text, GLfloat scale)
{
    assert(scale > 0.0f);
    assert(text);
    
    GLfloat xMin = +INFINITY;
    GLfloat xMax = -INFINITY;
    GLfloat yMin = +INFINITY;
    GLfloat yMax = -INFINITY;
    
    GLfloat cursor = 0.0f;
    const char *iter = text;
    while (*iter) {
        GLuint index = (GLuint)*iter;
        if (index >= N_CHARS) {
            fprintf(stderr, "Unrecognized char: %u.\n", index);
            continue;
        }
        // Get font character
        const _FontChar fc = _fontChars[index];
        GLfloat xPos = cursor + fc.bearingX*scale;
        // assert(fc.rows > fc.bearingY);
        GLfloat yPos = -(GLfloat)(fc.rows - fc.bearingY)*scale;
                
        GLfloat w    = fc.width*scale;
        GLfloat h    = fc.rows*scale;
        
        // Keep track of minimum and maximum position
        // (not necessary for x position)
        if (xPos < xMin)     xMin = xPos;
        if (xPos + w > xMax) xMax = xPos + w;
        if (yPos < yMin)     yMin = yPos;
        if (yPos + h > yMax) yMax = yPos + h;
        // Update cursor position
        cursor += (fc.advance >> 6)*scale;
        // Increment iterator
        ++iter;
    }
    GLfloat width = xMax - xMin; assert(width > 0.0f);
    GLfloat height = yMax - yMin; assert(height > 0.0f);
    
    return (TextDims) {.width = width, .height = height};
}

// Assumes text is null-terminated
void fontRenderText(const char *text, GLfloat x, GLfloat y, 
                GLfloat scale, const vec3 textCol, const vec3 bgCol)
{
    // Validate inputs
    assert(scale > 0.0f);
    assert((0.0f <= x && x <= ORIGIN_WIDTH) &&
           (0.0f <= y && y <= ORIGIN_HEIGHT));
    assert(text);
    
    // Use shader program
    glUseProgram(_fontShaderProgram);
    // Set text color
    GLint textColLoc = glGetUniformLocation(_fontShaderProgram, "textColor");
    glUniform3f(textColLoc, textCol[0], textCol[1], textCol[2]);
    // Set background color
    GLint bgColLoc = glGetUniformLocation(_fontShaderProgram, "backgroundColor");
    glUniform3f(bgColLoc, bgCol[0], bgCol[1], bgCol[2]);
    
    glActiveTexture(GL_TEXTURE0);  // activate texture unit
    glBindVertexArray(_fontVao);
    
    const char *iter = text;
    while (*iter) {
        GLuint index = (GLuint)*iter;
        if (index >= N_CHARS) {
            fprintf(stderr, "Unrecognized char: %u.\n", index);
            continue;
        }
        
        const _FontChar fc = _fontChars[index];
        // Calculate position of quad
        GLfloat xPos = x + fc.bearingX*scale;
        // assert(fc.rows > fc.bearingY);
        GLfloat yPos = y - (GLfloat)(fc.rows - fc.bearingY)*scale;
        
        GLfloat w    = fc.width*scale;
        GLfloat h    = fc.rows*scale;
        // Quad vertices, including texture coordinates
        const GLfloat vertices[6][4] = {
            {xPos    , yPos + h, 0.0f, 0.0f},
            {xPos    , yPos    , 0.0f, 1.0f},
            {xPos + w, yPos    , 1.0f, 1.0f},
            
            {xPos    , yPos + h, 0.0f, 0.0f},
            {xPos + w, yPos    , 1.0f, 1.0f},
            {xPos + w, yPos + h, 1.0f, 0.0f}
        };
        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, fc.textureId);
        glBindBuffer(GL_ARRAY_BUFFER, _fontVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);  // unbind
        // Render Quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Update cursor position by advance (divide by 64 since it is
        // the number of 1/64 pixels)
        x += (fc.advance >> 6)*scale;
        // Increment iterator
        ++iter;
    }
    // Unbind vao, texture and shader program again
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void fontRenderTextCentered(const char *text, GLfloat cx, GLfloat cy, 
                GLfloat scale, const vec3 textCol, const vec3 bgCol)
{
    // Validate inputs
    assert(scale > 0.0f);
    assert((0.0f <= cx && cx <= ORIGIN_WIDTH) &&
           (0.0f <= cy && cy <= ORIGIN_HEIGHT));
    assert(text && *text);  // make sure text contains at least 1 letter
    
    GLuint firstIndex = (GLuint)(*text);
    assert(firstIndex < N_CHARS);
    // Compute bearing (offset) needed for proper centering
    GLfloat offsetX = _fontChars[firstIndex].bearingX;
    // Determine bounding box of text
    TextDims td = fontGetTextDims(text, scale);
    // Render text at proper location
    fontRenderText(text,
                   cx - 0.5f*(td.width + offsetX),
                   cy - 0.5f*td.height,
                   scale,
                   textCol,
                   bgCol);
}

#endif /* FONT_H */
