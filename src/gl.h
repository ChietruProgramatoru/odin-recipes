// The OpenGL 3.3 functions and constants the game uses, loaded at startup through GLFW.
// (Windows only exports OpenGL 1.1 directly, so every program needs a loader like this.)
// Generated once by hand from the OpenGL headers; add a line when the game needs a new call.
#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define GL_APIENTRY __stdcall
#else
#define GL_APIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef float GLfloat;
typedef char GLchar;
typedef std::ptrdiff_t GLsizeiptr;

constexpr GLenum GL_FALSE = 0;
constexpr GLenum GL_TRUE = 1;
constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_LINES = 0x0001;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_CW = 0x0900;
constexpr GLenum GL_CCW = 0x0901;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_RGB = 0x1907;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_VENDOR = 0x1F00;
constexpr GLenum GL_RENDERER = 0x1F01;
constexpr GLenum GL_VERSION = 0x1F02;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_BACK = 0x0405;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;

typedef void(GL_APIENTRY* PFN_glClear)(GLbitfield mask);
extern PFN_glClear pc_glClear;
#define glClear pc_glClear
typedef void(GL_APIENTRY* PFN_glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern PFN_glClearColor pc_glClearColor;
#define glClearColor pc_glClearColor
typedef void(GL_APIENTRY* PFN_glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
extern PFN_glViewport pc_glViewport;
#define glViewport pc_glViewport
typedef void(GL_APIENTRY* PFN_glScissor)(GLint x, GLint y, GLsizei w, GLsizei h);
extern PFN_glScissor pc_glScissor;
#define glScissor pc_glScissor
typedef void(GL_APIENTRY* PFN_glEnable)(GLenum cap);
extern PFN_glEnable pc_glEnable;
#define glEnable pc_glEnable
typedef void(GL_APIENTRY* PFN_glDisable)(GLenum cap);
extern PFN_glDisable pc_glDisable;
#define glDisable pc_glDisable
typedef void(GL_APIENTRY* PFN_glBlendFunc)(GLenum s, GLenum d);
extern PFN_glBlendFunc pc_glBlendFunc;
#define glBlendFunc pc_glBlendFunc
typedef void(GL_APIENTRY* PFN_glDepthMask)(GLboolean flag);
extern PFN_glDepthMask pc_glDepthMask;
#define glDepthMask pc_glDepthMask
typedef void(GL_APIENTRY* PFN_glFrontFace)(GLenum mode);
extern PFN_glFrontFace pc_glFrontFace;
#define glFrontFace pc_glFrontFace
typedef const GLubyte*(GL_APIENTRY* PFN_glGetString)(GLenum name);
extern PFN_glGetString pc_glGetString;
#define glGetString pc_glGetString
typedef GLenum(GL_APIENTRY* PFN_glGetError)(void);
extern PFN_glGetError pc_glGetError;
#define glGetError pc_glGetError
typedef void(GL_APIENTRY* PFN_glGenTextures)(GLsizei n, GLuint* textures);
extern PFN_glGenTextures pc_glGenTextures;
#define glGenTextures pc_glGenTextures
typedef void(GL_APIENTRY* PFN_glDeleteTextures)(GLsizei n, const GLuint* textures);
extern PFN_glDeleteTextures pc_glDeleteTextures;
#define glDeleteTextures pc_glDeleteTextures
typedef void(GL_APIENTRY* PFN_glBindTexture)(GLenum target, GLuint texture);
extern PFN_glBindTexture pc_glBindTexture;
#define glBindTexture pc_glBindTexture
typedef void(GL_APIENTRY* PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
extern PFN_glTexImage2D pc_glTexImage2D;
#define glTexImage2D pc_glTexImage2D
typedef void(GL_APIENTRY* PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
extern PFN_glTexParameteri pc_glTexParameteri;
#define glTexParameteri pc_glTexParameteri
typedef void(GL_APIENTRY* PFN_glActiveTexture)(GLenum texture);
extern PFN_glActiveTexture pc_glActiveTexture;
#define glActiveTexture pc_glActiveTexture
typedef void(GL_APIENTRY* PFN_glPixelStorei)(GLenum pname, GLint param);
extern PFN_glPixelStorei pc_glPixelStorei;
#define glPixelStorei pc_glPixelStorei
typedef void(GL_APIENTRY* PFN_glReadBuffer)(GLenum mode);
extern PFN_glReadBuffer pc_glReadBuffer;
#define glReadBuffer pc_glReadBuffer
typedef void(GL_APIENTRY* PFN_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
extern PFN_glReadPixels pc_glReadPixels;
#define glReadPixels pc_glReadPixels
typedef void(GL_APIENTRY* PFN_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
extern PFN_glDrawElements pc_glDrawElements;
#define glDrawElements pc_glDrawElements
typedef void(GL_APIENTRY* PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern PFN_glDrawArrays pc_glDrawArrays;
#define glDrawArrays pc_glDrawArrays
typedef GLuint(GL_APIENTRY* PFN_glCreateShader)(GLenum type);
extern PFN_glCreateShader pc_glCreateShader;
#define glCreateShader pc_glCreateShader
typedef void(GL_APIENTRY* PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
extern PFN_glShaderSource pc_glShaderSource;
#define glShaderSource pc_glShaderSource
typedef void(GL_APIENTRY* PFN_glCompileShader)(GLuint shader);
extern PFN_glCompileShader pc_glCompileShader;
#define glCompileShader pc_glCompileShader
typedef void(GL_APIENTRY* PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern PFN_glGetShaderiv pc_glGetShaderiv;
#define glGetShaderiv pc_glGetShaderiv
typedef void(GL_APIENTRY* PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern PFN_glGetShaderInfoLog pc_glGetShaderInfoLog;
#define glGetShaderInfoLog pc_glGetShaderInfoLog
typedef void(GL_APIENTRY* PFN_glDeleteShader)(GLuint shader);
extern PFN_glDeleteShader pc_glDeleteShader;
#define glDeleteShader pc_glDeleteShader
typedef GLuint(GL_APIENTRY* PFN_glCreateProgram)(void);
extern PFN_glCreateProgram pc_glCreateProgram;
#define glCreateProgram pc_glCreateProgram
typedef void(GL_APIENTRY* PFN_glAttachShader)(GLuint program, GLuint shader);
extern PFN_glAttachShader pc_glAttachShader;
#define glAttachShader pc_glAttachShader
typedef void(GL_APIENTRY* PFN_glLinkProgram)(GLuint program);
extern PFN_glLinkProgram pc_glLinkProgram;
#define glLinkProgram pc_glLinkProgram
typedef void(GL_APIENTRY* PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern PFN_glGetProgramiv pc_glGetProgramiv;
#define glGetProgramiv pc_glGetProgramiv
typedef void(GL_APIENTRY* PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern PFN_glGetProgramInfoLog pc_glGetProgramInfoLog;
#define glGetProgramInfoLog pc_glGetProgramInfoLog
typedef void(GL_APIENTRY* PFN_glDeleteProgram)(GLuint program);
extern PFN_glDeleteProgram pc_glDeleteProgram;
#define glDeleteProgram pc_glDeleteProgram
typedef void(GL_APIENTRY* PFN_glUseProgram)(GLuint program);
extern PFN_glUseProgram pc_glUseProgram;
#define glUseProgram pc_glUseProgram
typedef GLint(GL_APIENTRY* PFN_glGetUniformLocation)(GLuint program, const GLchar* name);
extern PFN_glGetUniformLocation pc_glGetUniformLocation;
#define glGetUniformLocation pc_glGetUniformLocation
typedef void(GL_APIENTRY* PFN_glUniform1i)(GLint location, GLint v0);
extern PFN_glUniform1i pc_glUniform1i;
#define glUniform1i pc_glUniform1i
typedef void(GL_APIENTRY* PFN_glUniform1f)(GLint location, GLfloat v0);
extern PFN_glUniform1f pc_glUniform1f;
#define glUniform1f pc_glUniform1f
typedef void(GL_APIENTRY* PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern PFN_glUniform2f pc_glUniform2f;
#define glUniform2f pc_glUniform2f
typedef void(GL_APIENTRY* PFN_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern PFN_glUniform3f pc_glUniform3f;
#define glUniform3f pc_glUniform3f
typedef void(GL_APIENTRY* PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern PFN_glUniform4f pc_glUniform4f;
#define glUniform4f pc_glUniform4f
typedef void(GL_APIENTRY* PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
extern PFN_glUniformMatrix4fv pc_glUniformMatrix4fv;
#define glUniformMatrix4fv pc_glUniformMatrix4fv
typedef void(GL_APIENTRY* PFN_glGenVertexArrays)(GLsizei n, GLuint* arrays);
extern PFN_glGenVertexArrays pc_glGenVertexArrays;
#define glGenVertexArrays pc_glGenVertexArrays
typedef void(GL_APIENTRY* PFN_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern PFN_glDeleteVertexArrays pc_glDeleteVertexArrays;
#define glDeleteVertexArrays pc_glDeleteVertexArrays
typedef void(GL_APIENTRY* PFN_glBindVertexArray)(GLuint array);
extern PFN_glBindVertexArray pc_glBindVertexArray;
#define glBindVertexArray pc_glBindVertexArray
typedef void(GL_APIENTRY* PFN_glGenBuffers)(GLsizei n, GLuint* buffers);
extern PFN_glGenBuffers pc_glGenBuffers;
#define glGenBuffers pc_glGenBuffers
typedef void(GL_APIENTRY* PFN_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
extern PFN_glDeleteBuffers pc_glDeleteBuffers;
#define glDeleteBuffers pc_glDeleteBuffers
typedef void(GL_APIENTRY* PFN_glBindBuffer)(GLenum target, GLuint buffer);
extern PFN_glBindBuffer pc_glBindBuffer;
#define glBindBuffer pc_glBindBuffer
typedef void(GL_APIENTRY* PFN_glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern PFN_glBufferData pc_glBufferData;
#define glBufferData pc_glBufferData
typedef void(GL_APIENTRY* PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern PFN_glVertexAttribPointer pc_glVertexAttribPointer;
#define glVertexAttribPointer pc_glVertexAttribPointer
typedef void(GL_APIENTRY* PFN_glEnableVertexAttribArray)(GLuint index);
extern PFN_glEnableVertexAttribArray pc_glEnableVertexAttribArray;
#define glEnableVertexAttribArray pc_glEnableVertexAttribArray

// Loads every function above. Call after making a GL context current. Returns false if any
// function is missing (the graphics driver is too old for OpenGL 3.3).
bool load_gl();
