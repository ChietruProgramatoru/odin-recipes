#include "gl.h"

#include <GLFW/glfw3.h>

PFN_glClear pc_glClear = nullptr;
PFN_glClearColor pc_glClearColor = nullptr;
PFN_glViewport pc_glViewport = nullptr;
PFN_glEnable pc_glEnable = nullptr;
PFN_glDisable pc_glDisable = nullptr;
PFN_glBlendFunc pc_glBlendFunc = nullptr;
PFN_glDepthMask pc_glDepthMask = nullptr;
PFN_glFrontFace pc_glFrontFace = nullptr;
PFN_glGetString pc_glGetString = nullptr;
PFN_glGetError pc_glGetError = nullptr;
PFN_glGenTextures pc_glGenTextures = nullptr;
PFN_glDeleteTextures pc_glDeleteTextures = nullptr;
PFN_glBindTexture pc_glBindTexture = nullptr;
PFN_glTexImage2D pc_glTexImage2D = nullptr;
PFN_glTexParameteri pc_glTexParameteri = nullptr;
PFN_glActiveTexture pc_glActiveTexture = nullptr;
PFN_glPixelStorei pc_glPixelStorei = nullptr;
PFN_glReadBuffer pc_glReadBuffer = nullptr;
PFN_glReadPixels pc_glReadPixels = nullptr;
PFN_glDrawElements pc_glDrawElements = nullptr;
PFN_glDrawArrays pc_glDrawArrays = nullptr;
PFN_glCreateShader pc_glCreateShader = nullptr;
PFN_glShaderSource pc_glShaderSource = nullptr;
PFN_glCompileShader pc_glCompileShader = nullptr;
PFN_glGetShaderiv pc_glGetShaderiv = nullptr;
PFN_glGetShaderInfoLog pc_glGetShaderInfoLog = nullptr;
PFN_glDeleteShader pc_glDeleteShader = nullptr;
PFN_glCreateProgram pc_glCreateProgram = nullptr;
PFN_glAttachShader pc_glAttachShader = nullptr;
PFN_glLinkProgram pc_glLinkProgram = nullptr;
PFN_glGetProgramiv pc_glGetProgramiv = nullptr;
PFN_glGetProgramInfoLog pc_glGetProgramInfoLog = nullptr;
PFN_glDeleteProgram pc_glDeleteProgram = nullptr;
PFN_glUseProgram pc_glUseProgram = nullptr;
PFN_glGetUniformLocation pc_glGetUniformLocation = nullptr;
PFN_glUniform1i pc_glUniform1i = nullptr;
PFN_glUniform1f pc_glUniform1f = nullptr;
PFN_glUniform2f pc_glUniform2f = nullptr;
PFN_glUniform3f pc_glUniform3f = nullptr;
PFN_glUniform4f pc_glUniform4f = nullptr;
PFN_glUniformMatrix4fv pc_glUniformMatrix4fv = nullptr;
PFN_glGenVertexArrays pc_glGenVertexArrays = nullptr;
PFN_glDeleteVertexArrays pc_glDeleteVertexArrays = nullptr;
PFN_glBindVertexArray pc_glBindVertexArray = nullptr;
PFN_glGenBuffers pc_glGenBuffers = nullptr;
PFN_glDeleteBuffers pc_glDeleteBuffers = nullptr;
PFN_glBindBuffer pc_glBindBuffer = nullptr;
PFN_glBufferData pc_glBufferData = nullptr;
PFN_glVertexAttribPointer pc_glVertexAttribPointer = nullptr;
PFN_glEnableVertexAttribArray pc_glEnableVertexAttribArray = nullptr;

namespace {
template <typename F>
bool load(F& fn, const char* name) {
    fn = reinterpret_cast<F>(glfwGetProcAddress(name));
    return fn != nullptr;
}
}  // namespace

bool load_gl() {
    bool ok = true;
    ok &= load(pc_glClear, "glClear");
    ok &= load(pc_glClearColor, "glClearColor");
    ok &= load(pc_glViewport, "glViewport");
    ok &= load(pc_glEnable, "glEnable");
    ok &= load(pc_glDisable, "glDisable");
    ok &= load(pc_glBlendFunc, "glBlendFunc");
    ok &= load(pc_glDepthMask, "glDepthMask");
    ok &= load(pc_glFrontFace, "glFrontFace");
    ok &= load(pc_glGetString, "glGetString");
    ok &= load(pc_glGetError, "glGetError");
    ok &= load(pc_glGenTextures, "glGenTextures");
    ok &= load(pc_glDeleteTextures, "glDeleteTextures");
    ok &= load(pc_glBindTexture, "glBindTexture");
    ok &= load(pc_glTexImage2D, "glTexImage2D");
    ok &= load(pc_glTexParameteri, "glTexParameteri");
    ok &= load(pc_glActiveTexture, "glActiveTexture");
    ok &= load(pc_glPixelStorei, "glPixelStorei");
    ok &= load(pc_glReadBuffer, "glReadBuffer");
    ok &= load(pc_glReadPixels, "glReadPixels");
    ok &= load(pc_glDrawElements, "glDrawElements");
    ok &= load(pc_glDrawArrays, "glDrawArrays");
    ok &= load(pc_glCreateShader, "glCreateShader");
    ok &= load(pc_glShaderSource, "glShaderSource");
    ok &= load(pc_glCompileShader, "glCompileShader");
    ok &= load(pc_glGetShaderiv, "glGetShaderiv");
    ok &= load(pc_glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load(pc_glDeleteShader, "glDeleteShader");
    ok &= load(pc_glCreateProgram, "glCreateProgram");
    ok &= load(pc_glAttachShader, "glAttachShader");
    ok &= load(pc_glLinkProgram, "glLinkProgram");
    ok &= load(pc_glGetProgramiv, "glGetProgramiv");
    ok &= load(pc_glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load(pc_glDeleteProgram, "glDeleteProgram");
    ok &= load(pc_glUseProgram, "glUseProgram");
    ok &= load(pc_glGetUniformLocation, "glGetUniformLocation");
    ok &= load(pc_glUniform1i, "glUniform1i");
    ok &= load(pc_glUniform1f, "glUniform1f");
    ok &= load(pc_glUniform2f, "glUniform2f");
    ok &= load(pc_glUniform3f, "glUniform3f");
    ok &= load(pc_glUniform4f, "glUniform4f");
    ok &= load(pc_glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load(pc_glGenVertexArrays, "glGenVertexArrays");
    ok &= load(pc_glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load(pc_glBindVertexArray, "glBindVertexArray");
    ok &= load(pc_glGenBuffers, "glGenBuffers");
    ok &= load(pc_glDeleteBuffers, "glDeleteBuffers");
    ok &= load(pc_glBindBuffer, "glBindBuffer");
    ok &= load(pc_glBufferData, "glBufferData");
    ok &= load(pc_glVertexAttribPointer, "glVertexAttribPointer");
    ok &= load(pc_glEnableVertexAttribArray, "glEnableVertexAttribArray");
    return ok;
}
