#include <glad/gl.h>
#include <stddef.h>

PFNGLGETSTRINGPROC glad_glGetString = NULL;
PFNGLGETERRORPROC glad_glGetError = NULL;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;

static int load_required(GLADloadfunc load)
{
    glad_glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    glad_glGetError = (PFNGLGETERRORPROC)load("glGetError");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)load("glGetIntegerv");
    glad_glViewport = (PFNGLVIEWPORTPROC)load("glViewport");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)load("glClearColor");
    glad_glClear = (PFNGLCLEARPROC)load("glClear");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load("glGenVertexArrays");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load("glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)load("glDeleteVertexArrays");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load("glGenBuffers");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load("glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)load("glBufferData");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load("glDeleteBuffers");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)load("glEnableVertexAttribArray");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)load("glVertexAttribPointer");
    glad_glCreateShader = (PFNGLCREATESHADERPROC)load("glCreateShader");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load("glShaderSource");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load("glCompileShader");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)load("glGetShaderInfoLog");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)load("glDeleteShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load("glCreateProgram");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)load("glAttachShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load("glLinkProgram");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)load("glGetProgramInfoLog");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load("glUseProgram");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load("glDeleteProgram");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load("glDrawArrays");

    return glad_glGetString
        && glad_glViewport
        && glad_glClear
        && glad_glClearColor
        && glad_glGenVertexArrays
        && glad_glBindVertexArray
        && glad_glDeleteVertexArrays
        && glad_glGenBuffers
        && glad_glBindBuffer
        && glad_glBufferData
        && glad_glDeleteBuffers
        && glad_glEnableVertexAttribArray
        && glad_glVertexAttribPointer
        && glad_glCreateShader
        && glad_glShaderSource
        && glad_glCompileShader
        && glad_glGetShaderiv
        && glad_glGetShaderInfoLog
        && glad_glDeleteShader
        && glad_glCreateProgram
        && glad_glAttachShader
        && glad_glLinkProgram
        && glad_glGetProgramiv
        && glad_glGetProgramInfoLog
        && glad_glUseProgram
        && glad_glDeleteProgram
        && glad_glDrawArrays;
}

int gladLoadGL(GLADloadfunc load)
{
    if (load == NULL) {
        return 0;
    }
    return load_required(load);
}
