#ifndef GL_COMMON_H
#define GL_COMMON_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glut.h>

#include <stdio.h>
#include <stdlib.h>

#define MAX_INFO 512

GLuint createGLShader(GLenum shaderType, const GLchar *shaderSource) {
    // Create shader
    GLuint shader = glCreateShader(shaderType);
    // Specify source for shader
    glShaderSource(shader, 1, &shaderSource, NULL);
    // Compile
    glCompileShader(shader);
    // Check for successful compilation
    GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	
	if (!success) {
        GLchar infoLog[MAX_INFO];
		glGetShaderInfoLog(shader, MAX_INFO, NULL, infoLog);
		fprintf(stderr, "SHADER COMPILATION FAILED:\n%s\n", infoLog);
        // Cleanup
		glDeleteShader(shader);
		exit(EXIT_FAILURE);
	}
    return shader;    
}

GLuint createGLProgram(const GLchar *vShaderSource, const GLchar *fShaderSource) {
    // Initialize and compile shaders
    GLuint vertShader = createGLShader(GL_VERTEX_SHADER, vShaderSource);
    GLuint fragShader = createGLShader(GL_FRAGMENT_SHADER, fShaderSource);
    
    // Create and link shader program
    GLuint program = glCreateProgram();
    // Attach shaders
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    // Link program
    glLinkProgram(program);
    // Check success
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
	
	if (!success) {
        GLchar infoLog[MAX_INFO];
		glGetProgramInfoLog(program, MAX_INFO, NULL, infoLog);
		fprintf(stderr, "SHADER PROGRAM LINKING FAILED:\n%s\n", infoLog);
        // Cleanup
		glDetachShader(program, vertShader);
		glDetachShader(program, fragShader);
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
		glDeleteProgram(program);
		
		exit(EXIT_FAILURE);
	}
    // Detach shaders
    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);
    // Delete shaders
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    return program;
}

#endif /* GL_COMMON_H */
