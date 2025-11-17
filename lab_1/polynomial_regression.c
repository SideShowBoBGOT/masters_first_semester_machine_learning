#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "thirdparty/glad/glad.h"
#include "thirdparty/glad/glad_egl.h"

#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>

#define GLFW_EXPOSE_NATIVE_EGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_OPENGL3
#define CIMGUI_USE_GLFW
#include "thirdparty/cimgui/cimgui.h"
#include "thirdparty/cimplot/cimplot.h"
#include "thirdparty/cimgui/cimgui_impl.h"


#define ASSERT(expr, ...) \
    do {\
        if(!(expr)) {\
            fprintf(stderr, "%s:%d: %s: Assertion `%s' failed", __FILE__, __LINE__, __FUNCTION__, #expr);\
            __VA_OPT__(fprintf(stderr, " explanation: " __VA_ARGS__);)\
            fprintf(stderr, "\n");\
            abort();\
        }\
    } while(0)

#define ASSERT_NOT_MINUS_ONE(expr, ...) ASSERT((expr) != -1 __VA_OPT__(,) __VA_ARGS__)


#define LOG(...) \
    do {\
        fprintf(stderr, "[%s:%d] [%s] [%ld] ", __FILE__, __LINE__, __FUNCTION__, time(NULL));\
        fprintf(stderr, "[" __VA_ARGS__);\
        fprintf(stderr, "]\n");\
    } while(0)

#define _STRINGIFY(...) #__VA_ARGS__
#define S(...)          _STRINGIFY(__VA_ARGS__)

static void my_gl_clear_errors(void) {
    while(glGetError() != GL_NO_ERROR) {}
}

static void my_egl_clear_errors(void) {
    while(eglGetError() != EGL_SUCCESS) {}
}

static const char* mygl_str_error(const GLenum error) {
    #define DECLARE_ERROR_CASE(error) case error: return #error
        switch(error) {
            DECLARE_ERROR_CASE(GL_NO_ERROR);
            DECLARE_ERROR_CASE(GL_INVALID_ENUM);
            DECLARE_ERROR_CASE(GL_INVALID_VALUE);
            DECLARE_ERROR_CASE(GL_INVALID_OPERATION);
            DECLARE_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
            DECLARE_ERROR_CASE(GL_OUT_OF_MEMORY);
            default: break;
        }
        ASSERT(false, "Invalid OpenGL error: %u", error);
    #undef DECLARE_ERROR_CASE
}

static const char* myegl_str_error(const EGLint error) {
    #define DECLARE_ERROR_CASE(error) case error: return #error
        switch(error) {
            DECLARE_ERROR_CASE(EGL_SUCCESS);
            DECLARE_ERROR_CASE(EGL_NOT_INITIALIZED);
            DECLARE_ERROR_CASE(EGL_BAD_ACCESS);
            DECLARE_ERROR_CASE(EGL_BAD_ALLOC);
            DECLARE_ERROR_CASE(EGL_BAD_ATTRIBUTE);
            DECLARE_ERROR_CASE(EGL_BAD_CONFIG);
            DECLARE_ERROR_CASE(EGL_BAD_CONTEXT);
            DECLARE_ERROR_CASE(EGL_BAD_CURRENT_SURFACE);
            DECLARE_ERROR_CASE(EGL_BAD_DISPLAY);
            DECLARE_ERROR_CASE(EGL_BAD_MATCH);
            DECLARE_ERROR_CASE(EGL_BAD_NATIVE_PIXMAP);
            DECLARE_ERROR_CASE(EGL_BAD_NATIVE_WINDOW);
            DECLARE_ERROR_CASE(EGL_BAD_PARAMETER);
            DECLARE_ERROR_CASE(EGL_BAD_SURFACE);
            DECLARE_ERROR_CASE(EGL_CONTEXT_LOST);
            default: break;
        }
        ASSERT(false, "Invalid EGL error: %d", error);
    #undef DECLARE_ERROR_CASE
}

static void gl_assert(const char *const function, const char *const file, const int line, const char *const expr) {
    GLenum error;
    while(true) {
        error = glGetError();
        if(error == GL_NO_ERROR) {
            break;
        }
        fprintf(stderr, "%s:%d: %s: error:  %u: strerror: %s: OpenGL assertion `%s' failed\n", file, line, function, error, mygl_str_error(error), expr);
        abort();
    }
}

static void egl_assert(const char *const function, const char *const file, const int line, const char *const expr) {
    EGLint error;
    while(true) {
        error = eglGetError();
        if(error == EGL_SUCCESS) {
            break;
        }
        fprintf(stderr, "%s:%d: %s: error:  %d: strerror: %s: EGL assertion `%s' failed\n", file, line, function, error, myegl_str_error(error), expr);
        abort();
    }
}

#define ASSERT_GL(x) do { my_gl_clear_errors(); (x); gl_assert(__FUNCTION__, __FILE__, __LINE__, #x); } while(0)
#define ASSERT_EGL(x) do { my_egl_clear_errors(); (x); egl_assert(__FUNCTION__, __FILE__, __LINE__, #x); } while(0)


static GLuint mygl_create_compile_shader(const GLenum shader_type, const char *const shader_code) {
    GLuint shader_id;
    ASSERT_GL(shader_id = glCreateShader(shader_type));
    {
        const GLchar *cptr = shader_code;
        ASSERT_GL(glShaderSource(shader_id, 1, &cptr, NULL));
    }
    ASSERT_GL(glCompileShader(shader_id));
    GLint compile_status;
    ASSERT_GL(glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status));
    if(!compile_status) {
        GLint logLength;
        ASSERT_GL(glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &logLength));
        char *const log = (char*)malloc((GLuint)logLength);
        ASSERT_GL(glGetShaderInfoLog(shader_id, logLength, NULL, log));
        LOG("Shader compilation error, %s: %s\n", shader_code, log);
        free(log);
        ASSERT(false);
    }
    return shader_id;
}

static void mygl_create_shader_program(
    GLuint *const shader_program,
    GLuint *const shader_vertex,
    GLuint *const shader_fragment,
    const char *const vertex_shader_code,
    const char *const fragment_shader_code
) {
    *shader_vertex = mygl_create_compile_shader(GL_VERTEX_SHADER, vertex_shader_code);
    *shader_fragment = mygl_create_compile_shader(GL_FRAGMENT_SHADER, fragment_shader_code);

    *shader_program = glCreateProgram();
    ASSERT(*shader_program);

    ASSERT_GL(glAttachShader(*shader_program, *shader_vertex));
    ASSERT_GL(glAttachShader(*shader_program, *shader_fragment));
    {
        ASSERT_GL(glLinkProgram(*shader_program));
        GLint status;
        ASSERT_GL(glGetProgramiv(*shader_program, GL_LINK_STATUS, &status));
        ASSERT(status == GL_TRUE);
    }
    {
        ASSERT_GL(glValidateProgram(*shader_program));
        GLint status;
        ASSERT_GL(glGetProgramiv(*shader_program, GL_VALIDATE_STATUS, &status));
        ASSERT(status == GL_TRUE);
    }
}

static void mygl_create_compute_shader_program(
    GLuint *const shader_program,
    GLuint *const shader_compute,
    const char *const shader_code
) {
    *shader_compute = mygl_create_compile_shader(GL_COMPUTE_SHADER, shader_code);
    *shader_program = glCreateProgram();
    ASSERT(*shader_program);

    ASSERT_GL(glAttachShader(*shader_program, *shader_compute));
    {
        ASSERT_GL(glLinkProgram(*shader_program));
        GLint status;
        ASSERT_GL(glGetProgramiv(*shader_program, GL_LINK_STATUS, &status));
        ASSERT(status == GL_TRUE);
    }
    {
        ASSERT_GL(glValidateProgram(*shader_program));
        GLint status;
        ASSERT_GL(glGetProgramiv(*shader_program, GL_VALIDATE_STATUS, &status));
        ASSERT(status == GL_TRUE);
    }
}

__attribute__((noreturn)) static void my_glfw_error_callback(const int error, const char* const description) {
    LOG("GLFW Error %d: %s", error, description);
    abort();
}

static GLFWwindow* my_glfw_init(void) {
    glfwSetErrorCallback(my_glfw_error_callback);
    ASSERT(glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    // glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* const glfw_window = glfwCreateWindow(1280, 720, "demo", NULL, NULL);
    ASSERT(glfw_window);
    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(0);
    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress));
    ASSERT(gladLoadEGLLoader((GLADloadproc)glfwGetProcAddress));
    
    #define MY_PRINT_GL_GET_STRING(name) do {const GLubyte *value; ASSERT_GL(value = glGetString(name)); LOG(#name ": %s", value);} while(0)
        MY_PRINT_GL_GET_STRING(GL_RENDERER);
        MY_PRINT_GL_GET_STRING(GL_VENDOR);
        MY_PRINT_GL_GET_STRING(GL_VERSION);
        MY_PRINT_GL_GET_STRING(GL_SHADING_LANGUAGE_VERSION);
    #undef MY_PRINT_GL_GET_STRING

    EGLDisplay egl_display = glfwGetEGLDisplay();
    ASSERT(egl_display);

    #define MY_PRINT_EGL_QUERY_STRING(name) do {const char *value; ASSERT_EGL(value = eglQueryString(egl_display, name)); LOG(#name ": %s", value);} while(0)
        MY_PRINT_EGL_QUERY_STRING(EGL_CLIENT_APIS);
        MY_PRINT_EGL_QUERY_STRING(EGL_EXTENSIONS);
        MY_PRINT_EGL_QUERY_STRING(EGL_VENDOR);
        MY_PRINT_EGL_QUERY_STRING(EGL_VERSION);
    #undef MY_PRINT_EGL_QUERY_STRING
 
    #define MY_PRINT_INTEGERI_V(name, col) do {GLint value; ASSERT_GL(glGetIntegeri_v(name, col, &value)); LOG(#name " " #col ": %d", value);} while(0)
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0);
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1);
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2);
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0);
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1);
        MY_PRINT_INTEGERI_V(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2);
    #undef MY_PRINT_INTEGERI_V
  
    #define MY_PRINT_INTEGERV(name) do {GLint value; ASSERT_GL(glGetIntegerv(name, &value)); LOG(#name ": %d", value);} while(0)
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_ATOMIC_COUNTERS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_UNIFORM_BLOCKS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_UNIFORM_COMPONENTS);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);
        MY_PRINT_INTEGERV(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    #undef MY_PRINT_INTEGERV

    return glfw_window;
}

#define SHADER_VERSION_STRING "#version 460\n"

static const char matrix_mul_compute_shader[] = SHADER_VERSION_STRING S(
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

uniform uint m;
uniform uint n;
uniform uint l;
uniform uint x0;
uniform uint y0;

layout(std430, binding = 0) buffer ssbo_A { float A[]; };
layout(std430, binding = 1) buffer ssbo_B { float B[]; };
layout(std430, binding = 2) buffer ssbo_R { float R[]; };

const uint BLOCK_SIZE = gl_WorkGroupSize.x;

shared float As[BLOCK_SIZE][BLOCK_SIZE];
shared float Bs[BLOCK_SIZE][BLOCK_SIZE];

void __memoryBarrierShared() {
    memoryBarrierShared();
    barrier();
}

void main() {
    uint hA = m;
    uint wA = n;
    uint wB = l;

    uint bx = gl_WorkGroupID.x;
    uint by = gl_WorkGroupID.y;
    uint tx = gl_LocalInvocationID.x;
    uint ty = gl_LocalInvocationID.y;

    uint gx = x0 + gl_GlobalInvocationID.x;
    uint gy = y0 + gl_GlobalInvocationID.y;

    uint aBegin = wA * (y0 + BLOCK_SIZE * by);
    uint aEnd   = aBegin + wA - 1;
    uint aStep  = BLOCK_SIZE;

    uint bBegin = x0 + BLOCK_SIZE * bx;
    uint bStep  = BLOCK_SIZE * wB;

    float Rsub = 0.0f;

    for(uint a = aBegin, b = bBegin, blkStart = 0;
        a <= aEnd;
        a += aStep, b += bStep, blkStart += BLOCK_SIZE) {
        As[ty][tx] = (gy < hA && blkStart + tx < wA)
            ? A[a + wA * ty + tx]
            : 0.0f;

        Bs[ty][tx] = (gx < wB && blkStart + ty < wA)
            ? B[b + wB * ty + tx]
            : 0.0f;

        __memoryBarrierShared();

        // #pragma unroll
        for(uint k = 0; k < BLOCK_SIZE; k++) {
            Rsub += As[ty][k] * Bs[k][tx];
        }

        __memoryBarrierShared();
    }

    if(gy < hA && gx < wB) {
        uint c = wB * (y0 + BLOCK_SIZE * by) + (x0 + BLOCK_SIZE * bx);
        R[c + wB * ty + tx] = Rsub;
    }
}
);

typedef struct {
    uintptr_t *items;
    size_t count;
    size_t capacity;
} MyArena;

#define my_arena_bytes_count(a) (a)->count * sizeof(*(a)->items)
#define my_arena_reset(a) (a)->count = 0

static MyArena my_arena_init(const size_t bytes_count) {
    MyArena arena = {0};
    const size_t word_size = sizeof(*arena.items);
    const size_t words_count = (bytes_count + word_size - 1) / word_size;
    arena.items = (uintptr_t*)calloc(words_count, word_size);
    ASSERT(arena.items);
    arena.capacity = words_count;
    return arena;
}

static void* my_arena_alloc(MyArena *const arena, const size_t bytes_count) {
    const size_t word_size = sizeof(*arena->items);
    const size_t words_count = (bytes_count + word_size - 1) / word_size;
    ASSERT(arena->count + words_count <= arena->capacity);
    void *const result = &arena->items[arena->count];
    arena->count += words_count;
    return result;
} 

static void mult_matrixes(void) {
    MyArena arena = my_arena_init(1024 * 1024);

    size_t rows_count = 40;
    size_t cols_count = 30;

    const 


    GLuint buffers[3];
    ASSERT_GL(glGenBuffers(3, buffers));
    
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[0]));
    ASSERT_GL(glBufferData(GL_SHADER_STORAGE_BUFFER, ));

    free(arena.items);
}

int main(void) {
    GLFWwindow* const glfw_window = my_glfw_init();
    ImGuiContext *const ig_context = igCreateContext(NULL);
    ImPlotContext *const implot_context = ImPlot_CreateContext();
    ImGuiIO *const ig_io = igGetIO_Nil();
    ig_io->IniFilename = NULL;
    ig_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ig_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ig_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGuiStyle *const ig_style = igGetStyle();
    igStyleColorsDark(ig_style);
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init("#version 460");




    
    while(!glfwWindowShouldClose(glfw_window)) {
        glfwPollEvents();
        ASSERT_GL(glClearColor(0, 0, 0, 1));
        ASSERT_GL(glClear(GL_COLOR_BUFFER_BIT));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        igNewFrame();

        // {
        //     ImGuiViewport* const ig_viewport = igGetWindowViewport();
        //     igSetNextWindowPos((ImVec2){0}, ImGuiCond_FirstUseEver, (ImVec2){0});
        //     igSetNextWindowSize(ig_viewport->WorkSize, ImGuiCond_Always);
        // }
        // if(igBegin("demo", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        //
        // }
        // igEnd();


            ImS64   bar_data[11];
            for(int i = 0; i < 11; i++) {
                bar_data[i] = i;
            }

            ImVec2 plot_size;
            plot_size.x = 500;
            plot_size.y = 300;
            
            igBegin("My Window", NULL, 0);
            
            if(ImPlot_BeginPlot("My Plot",plot_size,0)) {
                ImPlot_PlotBars_S64PtrInt("My Bar Plot", bar_data, 11,0.67,0,0,0,sizeof(ImS64));
                ImPlot_EndPlot();
            }
            
            //igText("Test");
            //igButton("Test",(struct ImVec2){200,50});

            igEnd();


        static bool showdemowindow = true;
        igShowDemoWindow(&showdemowindow);



        igRender();
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
        glfwSwapBuffers(glfw_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot_DestroyContext(implot_context);
    igDestroyContext(ig_context);

    glfwTerminate();
    return 0;
}
