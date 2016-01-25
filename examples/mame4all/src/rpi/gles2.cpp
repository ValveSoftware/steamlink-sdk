/*
 * video.c - Raspberry Pi suport by djdron
 *
 * Copyright (c) 2013 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * This file is based on Portable ZX-Spectrum emulator.
 * Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
 *
 * C++ to C code conversion by Pate
 *
 * Atari800 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GLES2/gl2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "minimal.h"

//IMPORTANT, make sure this function is commented out at runtime
//as it has a big performance impact!
#define	SHOW_ERROR	//gles_show_error();

static const char* vertex_shader_prg =
    "uniform mat4 u_vp_matrix;                              \n"
    "attribute vec4 a_position;                             \n"
    "attribute vec2 a_texcoord;                             \n"
    "varying mediump vec2 v_texcoord;                       \n"
    "void main()                                            \n"
    "{                                                      \n"
    "   v_texcoord = a_texcoord;                            \n"
    "   gl_Position = u_vp_matrix * a_position;             \n"
    "}                                                      \n";

static const char* fragment_shader_none =
    "varying mediump vec2 v_texcoord;                                           \n"
    "uniform sampler2D u_texture;                                               \n"
    "uniform sampler2D u_palette;                                               \n"
    "void main()                                                                \n"
    "{                                                                          \n"
	"	// u_texture is the index bitmap, u_palette is the palette for it		\n"
    "	vec4 index = texture2D(u_texture, v_texcoord);                          \n"
    "	gl_FragColor = texture2D(u_palette, index.xy);  				\n"
    "}                                                                          \n";

static const char* fragment_shader_none_16bit =
    "varying mediump vec2 v_texcoord;                       \n"
    "uniform sampler2D u_texture;                           \n"
    "uniform sampler2D u_palette;                           \n"
    "void main()                                            \n"
    "{                                                      \n"
	"	gl_FragColor = texture2D(u_texture, v_texcoord);	\n"
    "}                                                      \n";

// scanline-3x.shader
static const char* fragment_shader_scanline =
	"varying mediump vec2 v_texcoord; 												\n"
	"uniform sampler2D u_texture; 													\n"
    "uniform sampler2D u_palette;                                                   \n"
	"void main()     																\n"
	"{																				\n"
	"	vec4 index = texture2D(u_texture, v_texcoord);  								\n"
    "   vec4 rgb = texture2D(u_palette, index.xy);  					    \n"
	"	vec4 intens ;  																\n"
	"	if (fract(gl_FragCoord.y * (0.5*4.0/3.0)) > 0.5)  							\n"
	"		intens = vec4(0);  														\n"
	"	else  																		\n"
	"		intens = smoothstep(0.2,0.8,rgb) + normalize(vec4(rgb.xyz, 1.0));  		\n"
	"	float level = (4.0-0.0) * 0.19;  											\n"
	"	gl_FragColor = intens * (0.5-level) + rgb * 1.1 ;  							\n"
	"} 																				\n";

// scanline-3x.shader
static const char* fragment_shader_scanline_16bit =
	"varying mediump vec2 v_texcoord; 												\n"
	"uniform sampler2D u_texture; 													\n"
    "uniform sampler2D u_palette;                                                   \n"
	"void main()     																\n"
	"{																				\n"
	"	vec4 rgb = texture2D(u_texture, v_texcoord);  								\n"
	"	vec4 intens ;  																\n"
	"	if (fract(gl_FragCoord.y * (0.5*4.0/3.0)) > 0.5)  							\n"
	"		intens = vec4(0);  														\n"
	"	else  																		\n"
	"		intens = smoothstep(0.2,0.8,rgb) + normalize(vec4(rgb.xyz, 1.0));  		\n"
	"	float level = (4.0-0.0) * 0.19;  											\n"
	"	gl_FragColor = intens * (0.5-level) + rgb * 1.1 ;  							\n"
	"} 																				\n";

static const GLfloat vertices[] =
{
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

//Values defined in gles2_create()
static GLfloat uvs[8];

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

void gles_show_error()
{
	GLenum error = GL_NO_ERROR;
    error = glGetError();
    if (GL_NO_ERROR != error)
        printf("GL Error %x encountered!\n", error);
}

static GLuint CreateShader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);
	if(!shader)
		return 0;

	// Load and compile the shader source
	glShaderSource(shader, 1, &shader_src, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error compiling shader:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint CreateProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
	GLuint vertex_shader = CreateShader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
		return 0;
	GLuint fragment_shader = CreateShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program_object = glCreateProgram();
	if(!program_object)
		return 0;
	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	// Link the program
	glLinkProgram(program_object);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
	if(!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(info_len);
			glGetProgramInfoLog(program_object, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error linking program:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteProgram(program_object);
		return 0;
	}
	// Delete these here because they are attached to the program object.
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program_object;
}

static void SetOrtho(float m[4][4], float left, float right, float bottom, float top, float near, float far, float scale_x, float scale_y)
{
	memset(m, 0, 4*4*sizeof(float));
	m[0][0] = 2.0f/(right - left)*scale_x;
	m[1][1] = 2.0f/(top - bottom)*scale_y;
	m[2][2] = -2.0f/(far - near);
	m[3][0] = -(right + left)/(right - left);
	m[3][1] = -(top + bottom)/(top - bottom);
	m[3][2] = -(far + near)/(far - near);
	m[3][3] = 1;
}

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
		GLint u_palette;
} ShaderInfo;

static ShaderInfo shader;
static GLuint buffers[3];
static GLuint textures[2];

static char palette_changed;

static int dis_width;
static int dis_height;
static float proj[4][4];
static float tex_width, tex_height;

extern int vector_game;

void gles2_create(int display_width, int display_height, int bitmap_width, int bitmap_height, int depth)
{
	float min_u, max_u, min_v, max_v;
	
	tex_width = (float)bitmap_width;
	tex_height = (float)bitmap_height;

	//Setup drawing area of texture, i.e. the whole thing
	min_u=0;
	max_u=1.0f;
	min_v=0;
	max_v=1.0f;

	float op_zoom = 1.0f;
	
	memset(&shader, 0, sizeof(ShaderInfo));

	if (options.display_effect == 1 && !vector_game)
		if(depth == 8)
			shader.program = CreateProgram(vertex_shader_prg, fragment_shader_scanline);
		else
			shader.program = CreateProgram(vertex_shader_prg, fragment_shader_scanline_16bit);
	else
		if(depth == 8)
			shader.program = CreateProgram(vertex_shader_prg, fragment_shader_none);
		else
			shader.program = CreateProgram(vertex_shader_prg, fragment_shader_none_16bit);
	
	if(shader.program)
	{
		shader.a_position	= glGetAttribLocation(shader.program, "a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program, "a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program, "u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program, "u_texture");
		shader.u_palette    = glGetUniformLocation(shader.program, "u_palette");
	}
	if(!shader.program) return;

	glGenTextures(2, textures);	SHOW_ERROR

	//create bitmap texture
	//8bit uses a texture palette, 16bit does not
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	if (depth == 8) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex_width, tex_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL); SHOW_ERROR
	}
	else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL); SHOW_ERROR
	}

	//create palette texture, only used by 8bit
	glBindTexture(GL_TEXTURE_2D, textures[1]); SHOW_ERROR	// color palette
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL); SHOW_ERROR

	uvs[0] = min_u;
	uvs[1] = min_v;
	uvs[2] = max_u;
	uvs[3] = min_v;
	uvs[4] = max_u;
	uvs[5] = max_v;
	uvs[6] = min_u;
	uvs[7] = max_v;

	glGenBuffers(3, buffers); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); SHOW_ERROR
	glDisable(GL_DEPTH_TEST); SHOW_ERROR
	glDisable(GL_DITHER); SHOW_ERROR

	float sx = 1.0f, sy = 1.0f;

	dis_width = display_width;
	dis_height = display_height;

	// Screen aspect ratio adjustment
	float a = (float)display_width/(float)display_height;
	float a0 = (float)bitmap_width/(float)bitmap_height;
	if(a > a0)
		sx = a0/a;
	else
		sy = a/a0;

	//Set the dimensions for displaying the texture(bitmap) on the screen
	SetOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f, sx*op_zoom, sy*op_zoom);

    palette_changed = 1;
}

void gles2_destroy()
{
	if(!shader.program) return;
	if(shader.program) glDeleteProgram(shader.program);
	glDeleteBuffers(3, buffers); SHOW_ERROR
	glDeleteTextures(2, textures); SHOW_ERROR
}

//Draw using a palette texture
static void gles2_DrawQuad_8(const ShaderInfo *sh, GLuint p_textures[2])
{
	glUniform1i(sh->u_texture, 0); SHOW_ERROR

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR

    glActiveTexture(GL_TEXTURE1); SHOW_ERROR
    glBindTexture(GL_TEXTURE_2D, p_textures[1]); SHOW_ERROR
    glUniform1i(sh->u_palette, 1); SHOW_ERROR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_position); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_texcoord); SHOW_ERROR

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0); SHOW_ERROR
}

//Draw directly, without a palette texture.
static void gles2_DrawQuad_16(const ShaderInfo *sh, GLuint p_textures[2])
{
	glUniform1i(sh->u_texture, 0); SHOW_ERROR

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_position); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_texcoord); SHOW_ERROR

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0); SHOW_ERROR
}

extern volatile unsigned short gp2x_palette[512];

void gles2_draw(void *screen, int width, int height, int depth)
{
	if(!shader.program) return;

	glClear(GL_COLOR_BUFFER_BIT); SHOW_ERROR
	glViewport(0, 0, dis_width, dis_height); SHOW_ERROR

	glDisable(GL_BLEND); SHOW_ERROR
	glUseProgram(shader.program); SHOW_ERROR
	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, &proj[0][0]); SHOW_ERROR

	if(depth == 8 && palette_changed)
    {
        glActiveTexture(GL_TEXTURE1); SHOW_ERROR
        glBindTexture(GL_TEXTURE_2D, textures[1]); SHOW_ERROR
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, (const GLvoid*)gp2x_palette); SHOW_ERROR
		palette_changed = 0;
    }

	glActiveTexture(GL_TEXTURE0); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	
	if(depth == 8)
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, (unsigned char*) screen); SHOW_ERROR
		gles2_DrawQuad_8(&shader, textures);
	}
	else
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, (unsigned short*) screen); SHOW_ERROR
		gles2_DrawQuad_16(&shader, textures);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR
}

void gles2_palette_changed()
{
    palette_changed = 1;
}

