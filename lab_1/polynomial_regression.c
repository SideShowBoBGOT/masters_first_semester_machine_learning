#include <sys/stat.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

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

#define ASSERT(expr, ...) ((expr) ? (void)0 : (fprintf(stderr, "%s:%d: %s: Assertion `%s' failed", __FILE__, __LINE__, __FUNCTION__, #expr) __VA_OPT__(,) __VA_OPT__(fprintf(stderr, " explanation: " __VA_ARGS__)), fprintf(stderr, "\n"), abort()))
#define ASSERT_NOT_MINUS_ONE(expr, ...) ASSERT((expr) != -1 __VA_OPT__(,) __VA_ARGS__)

#define LOG(...) \
    do {\
        fprintf(stderr, "[%s:%d] [%s] [%ld] ", __FILE__, __LINE__, __FUNCTION__, time(NULL));\
        fprintf(stderr, "[" __VA_ARGS__);\
        fprintf(stderr, "]\n");\
    } while(0)

#define _STRINGIFY(...) #__VA_ARGS__
#define S(...)          _STRINGIFY(__VA_ARGS__)

#define my_array_count(a) (sizeof(a) / sizeof(a[0]))

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

static void my_gl_egl_print_stats(EGLDisplay egl_display) {
    #define MY_PRINT_GL_GET_STRING(name) do {const GLubyte *value; ASSERT_GL(value = glGetString(name)); LOG(#name ": %s", value);} while(0)
        MY_PRINT_GL_GET_STRING(GL_RENDERER);
        MY_PRINT_GL_GET_STRING(GL_VENDOR);
        MY_PRINT_GL_GET_STRING(GL_VERSION);
        MY_PRINT_GL_GET_STRING(GL_SHADING_LANGUAGE_VERSION);
    #undef MY_PRINT_GL_GET_STRING

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
}

static GLFWwindow* my_glfw_init(const bool visible) {
    glfwSetErrorCallback(my_glfw_error_callback);
    ASSERT(glfwInit());
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
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
    
    EGLDisplay egl_display = glfwGetEGLDisplay();
    ASSERT(egl_display != EGL_NO_DISPLAY);
    my_gl_egl_print_stats(egl_display);
    return glfw_window;
}

typedef struct {
    uint8_t *items;
    size_t count;
    size_t capacity;
} MyArena;

#define my_arena_reset(a) (a)->count = 0

static MyArena my_arena_init(const size_t bytes_count) {
    MyArena arena = {.capacity = bytes_count, .items = (uint8_t*)malloc(bytes_count)};
    ASSERT(arena.items);
    memset(arena.items, 0, bytes_count);
    return arena;
}

static void* my_arena_alloc(MyArena *const arena, const size_t bytes_count) {
    ASSERT(arena->count + bytes_count <= arena->capacity);
    void *const result = &arena->items[arena->count];
    arena->count += bytes_count;
    return result;
} 

typedef struct {
    size_t rows;
    size_t cols;
    GLfloat *items;
} MyMat;

#define MY_MAT_ASSERT_ENABLE
#ifdef MY_MAT_ASSERT_ENABLE
    #define MY_MAT_ASSERT(expr) ASSERT((expr))
#else
    #define MY_MAT_ASSERT(expr) (NULL)
#endif

#define my_mat_item_ptr(m, r, c) (MY_MAT_ASSERT((r) < (m)->rows && (c) < (m)->cols), &(m)->items[r * (m)->cols + c])
#define my_mat_item(m, r, c) (*my_mat_item_ptr((m), (r), (c)))
#define my_mat_row(m, r) ((m)->items[r * (m)->cols])
#define my_range_for(type, name, min, max) for(type name = (min); name < (max); ++name)
#define my_range_for_zero(type, name, max) for(type name = (type)0; name < (max); ++name)
#define my_mat_items_count(mat) ((mat)->rows * (mat)->cols)
#define my_mat_row_bytes_count(mat) ((mat)->cols * sizeof(*(mat)->items))
#define my_mat_bytes_count(mat) ((mat)->rows * my_mat_row_bytes_count((mat)))
#define my_mat_foreach(name, mat) my_range_for(GLfloat*, name, (mat)->items, ((mat)->items + my_mat_items_count((mat)))) 

static MyMat my_mat_alloc(MyArena arena[static 1], const size_t rows, const size_t cols) {
    MyMat m = {.rows = rows, .cols = cols};
    m.items = (GLfloat*)my_arena_alloc(arena, sizeof(*m.items) * rows * cols);
    return m;
}

static MyMat my_mat_hstack(MyArena arena[static 1], const MyMat first[static 1], const MyMat second[static 1]) {
    ASSERT(first->rows == second->rows);
    MyMat result = my_mat_alloc(arena, first->rows, first->cols + second->cols);
    my_range_for_zero(size_t, row, first->rows) {
        memcpy(&my_mat_row(&result, row), &my_mat_row(first, row), my_mat_row_bytes_count(first));
        memcpy(&my_mat_item(&result, row, first->cols), &my_mat_row(second, row), my_mat_row_bytes_count(second));
    }
    return result;
}

static MyMat my_mat_copy(MyArena arena[static 1], const MyMat src[static 1]) {
    MyMat m = my_mat_alloc(arena, src->rows, src->cols);
    memcpy(m.items, src->items, my_mat_bytes_count(src));
    return m; 
}

static void my_mat_mul(MyMat *const result, const MyMat *const first, const MyMat *const second) { ASSERT(first->cols == second->rows); ASSERT(first->rows == result->rows);
    ASSERT(first->cols == second->rows);
    ASSERT(first->rows == result->rows);
    ASSERT(second->cols == result->cols);
    my_range_for_zero(size_t, first_row_index, first->rows) {
        my_range_for_zero(size_t, second_col_index, second->cols) {
            GLfloat sum = 0.f;
            my_range_for_zero(size_t, first_col_index, first->cols) {
                sum += my_mat_item(first, first_row_index, first_col_index) * my_mat_item(second, first_col_index, second_col_index);
            }
            my_mat_item(result, first_row_index, second_col_index) = sum;
        }
    }
}

static void my_mat_transpose(void) {

}

#define SHADER_VERSION_STRING "#version 460\n"

static const char mygl_matrix_mul_compute_shader[] = SHADER_VERSION_STRING S(
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

uniform uint m;
uniform uint n;
uniform uint l;
uniform uint x0;
uniform uint y0;

layout(std430, binding = 0) readonly buffer ssbo_A { float A[]; };
layout(std430, binding = 1) readonly buffer ssbo_B { float B[]; };
layout(std430, binding = 2) writeonly buffer ssbo_R { float R[]; };

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
    GLuint rows;
    GLuint cols;
    GLuint ssb;
} MyGLMat;

static inline GLint my_gl_get_uniform_location(const GLuint shader_program, const char uniform_location_name[]) {
    GLint value;
    ASSERT_GL(value = glGetUniformLocation(shader_program, uniform_location_name));
    ASSERT(value != -1);
    return value;
}

static MyGLMat mygl_mat_buffer_data(const MyMat mat[static 1], const GLenum usage) {
    MyGLMat gl_mat = {.rows = (GLuint)mat->rows, .cols = (GLuint)mat->cols};
    ASSERT_GL(glGenBuffers(1, &gl_mat.ssb));
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_mat.ssb));
        ASSERT_GL(glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)my_mat_bytes_count(mat), mat->items, usage));
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));
    return gl_mat;
}

// static MyGLMat my_gl_mat_mul_result_alloc(const MyGLMat first[static 1], const MyGLMat second[static 1]) {
//     ASSERT(first->cols == second->rows);
// }

static void my_gl_dispatch_compute_mat_mul(const GLuint shader_program, const MyGLMat first[static 1], const MyGLMat second[static 1], const MyGLMat result[static 1]) {
    ASSERT(first->cols == second->rows);
    ASSERT(first->rows == result->rows);
    ASSERT(second->cols == result->cols);

    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, first->ssb));
    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, second->ssb));
    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, result->ssb));
    ASSERT_GL(glUseProgram(shader_program));

        const GLuint BLOCK_SIZE = 32;
        const GLuint wsize[3] = {BLOCK_SIZE, BLOCK_SIZE, 1};
        const GLuint gsize[3] = {64, 64, 1};
        
        const GLuint wcnt_x = (second->cols + wsize[0] - 1) / wsize[0];
        const GLuint wcnt_y = (first->rows + wsize[1] - 1) / wsize[1];
        
        const GLuint gcnt_x = (wcnt_x + gsize[0] - 1) / gsize[0];
        const GLuint gcnt_y = (wcnt_y + gsize[1] - 1) / gsize[1];
        
        const GLuint nelem_x = wsize[0] * gsize[0];
        const GLuint nelem_y = wsize[1] * gsize[1];

        ASSERT_GL(glUniform1ui(my_gl_get_uniform_location(shader_program, "m"), first->rows));
        ASSERT_GL(glUniform1ui(my_gl_get_uniform_location(shader_program, "n"), first->cols));
        ASSERT_GL(glUniform1ui(my_gl_get_uniform_location(shader_program, "l"), second->cols));

        for(GLuint yi = 0; yi < gcnt_y; yi++) {
            GLuint y0 = nelem_y * yi;
            for(GLuint xi = 0; xi < gcnt_x; xi++) {
                GLuint x0 = nelem_x * xi;
                ASSERT_GL(glUniform1ui(my_gl_get_uniform_location(shader_program, "x0"), x0));
                ASSERT_GL(glUniform1ui(my_gl_get_uniform_location(shader_program, "y0"), y0));
                ASSERT_GL(glDispatchCompute(gsize[0], gsize[1], 1));
            }
        }

    ASSERT_GL(glUseProgram(0));
    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0));
    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0));
    ASSERT_GL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0));
}

static void my_read_bin_data_to_mat(MyMat *const mat, MyArena *const arena, const char *const path) {
    mat->items = (GLfloat*)my_arena_alloc(arena, my_mat_bytes_count(mat));
    const int fd = open(path, O_RDONLY);
    ASSERT_NOT_MINUS_ONE(fd);
    struct stat stat;
    ASSERT_NOT_MINUS_ONE(fstat(fd, &stat));
    ASSERT((size_t)stat.st_size == my_mat_bytes_count(mat));
    GLfloat *const data = (GLfloat*)mmap(NULL, my_mat_bytes_count(mat), PROT_READ, MAP_SHARED, fd, 0);
    ASSERT(data != MAP_FAILED);
    memcpy(mat->items, data, my_mat_bytes_count(mat));
    ASSERT_NOT_MINUS_ONE(munmap(data, my_mat_bytes_count(mat)));
    ASSERT_NOT_MINUS_ONE(close(fd));
}

static inline float my_powf(const float value, const size_t degree) {
    double res = (double)value;
    for(size_t i = 1; i < degree; ++i) {
        res *= (double)value;
    }
    return (float)res;
}
static inline double my_pow(const double value, const size_t degree) {
    double res = value;
    for(size_t i = 1; i < degree; ++i) {
        res *= value;
    }
    return res;
}

static MyMat my_polynomial_features_create(MyArena arena[static 1], const MyMat features[static 1], const size_t degree) {
    ASSERT(degree > 0);
    MyMat polynomial_features = my_mat_alloc(arena, features->rows, features->cols * degree);
    my_range_for_zero(size_t, row, features->rows) {
        for(size_t deg = 1, base_col = 0; deg < degree; deg++, base_col += features->cols) {
            my_range_for_zero(size_t, col, features->cols) {
                my_mat_item(&polynomial_features, row, base_col + col) = my_powf(my_mat_item(features, row, col), deg);
            }
        }
    }
    return polynomial_features; 
}

static void my_mat_polynomial_features_standard_scale(MyMat mat[static 1]) {
    my_range_for_zero(size_t, col, mat->cols) {
        double mean = 0.0;
        my_range_for_zero(size_t, row, mat->rows) {
            mean += (double)my_mat_item(mat, row, col);
        }
        mean /= (double)mat->rows;

        double std = 0.0;
        my_range_for_zero(size_t, row, mat->rows) {
            std += my_pow((double)my_mat_item(mat, row, col) - mean, 2);
        }
        std /= (double)mat->rows;
        std = sqrt(std);

        my_range_for_zero(size_t, row, mat->rows) {
            my_mat_item(mat, row, col) = (GLfloat)(((double)my_mat_item(mat, row, col) - mean) / std);
        }
    }
}

typedef struct {
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;
} MyEGLData;

static void my_egl_deinit(MyEGLData data[static 1]) {
    ASSERT(eglMakeCurrent(data->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    ASSERT(eglDestroyContext(data->eglDisplay, data->eglContext));
    ASSERT(eglDestroySurface(data->eglDisplay, data->eglSurface));
    ASSERT(eglTerminate(data->eglDisplay));
}

static MyEGLData my_egl_init(void) {
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    EGLSurface egl_surface;

    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ASSERT(egl_display != EGL_NO_DISPLAY);
    EGLint major, minor;
    ASSERT(eglInitialize(egl_display, &major, &minor) == EGL_TRUE);
    
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLint numConfigs;
    ASSERT(eglChooseConfig(egl_display, configAttribs, &egl_config, 1, &numConfigs) == EGL_TRUE);
    ASSERT(numConfigs > 0);
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1920,
        EGL_HEIGHT, 1080,
        EGL_NONE
    };
    egl_surface = eglCreatePbufferSurface(egl_display, egl_config, pbufferAttribs);
    ASSERT(egl_surface != EGL_NO_SURFACE);
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    ASSERT(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);
    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, contextAttribs);
    ASSERT(egl_context != EGL_NO_CONTEXT);
    ASSERT(eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) == EGL_TRUE);

    ASSERT(gladLoadGLLoader((GLADloadproc)eglGetProcAddress));
    ASSERT(gladLoadEGLLoader((GLADloadproc)eglGetProcAddress));

    my_gl_egl_print_stats(egl_display);

    return (MyEGLData){.eglDisplay = egl_display, .eglContext = egl_context, .eglSurface = egl_surface};
}

static void my_polynomial_train(void) {
    MyEGLData egl_data = my_egl_init();
    // GLFWwindow* const glfw_window = my_glfw_init(false);
    
    MyMat x_train = {.rows = 20210, .cols = 167};
    MyMat y_train = {.rows = 20210, .cols = 1};
    MyMat x_test = {.rows = 1053, .cols = 167};
    MyMat y_test = {.rows = 1053, .cols = 1};

    MyArena arena = my_arena_init(
        my_mat_bytes_count(&x_train)
        + my_mat_bytes_count(&y_train)
        + my_mat_bytes_count(&x_test)
        + my_mat_bytes_count(&y_test)
    );
    #define LOCAL_MACRO(mat) my_read_bin_data_to_mat(&mat, &arena, "data/" #mat ".bin")
        LOCAL_MACRO(x_train);
        LOCAL_MACRO(y_train);
        LOCAL_MACRO(x_test);
        LOCAL_MACRO(y_test);
    #undef LOCAL_MACRO

    MyArena fit_arena = my_arena_init(1024 * 1024 * 300);
    MyMat polynomial_features = my_polynomial_features_create(&fit_arena, &x_train, 4);
    LOG("size: %lu", my_mat_bytes_count(&polynomial_features));
    my_mat_polynomial_features_standard_scale(&polynomial_features);
    MyMat ones = my_mat_alloc(&fit_arena, polynomial_features.rows, 1); 
    my_mat_foreach(el, &ones) {
        *el = 1.f;
    }
    MyMat xb = my_mat_hstack(&fit_arena, &ones, &polynomial_features);

    const MyGLMat gl_xb = mygl_mat_buffer_data(&xb, GL_DYNAMIC_DRAW);
    const MyGLMat gl_weights = mygl_mat_buffer_data(&(MyMat){.rows = xb.cols, .cols = 1}, GL_DYNAMIC_COPY);
    {
        ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_weights.ssb));
            const GLfloat value = 0.f;
            ASSERT_GL(glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32F, GL_RED, GL_FLOAT, &value));
        ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));
    }
    GLuint shader_program_mul_mat, compute_shader_mul_mat;
    mygl_create_compute_shader_program(&shader_program_mul_mat, &compute_shader_mul_mat, mygl_matrix_mul_compute_shader);

    MyGLMat predictions = mygl_mat_buffer_data(&(MyMat){.rows = gl_xb.rows, .cols = gl_weights.cols}, GL_DYNAMIC_COPY);

    my_range_for_zero(size_t, i, 100) {
        my_gl_dispatch_compute_mat_mul(shader_program_mul_mat, &gl_xb, &gl_weights, &predictions);
        ASSERT_GL(glFinish());
    }

    free(fit_arena.items);
    free(arena.items);
    // glfwTerminate();
    my_egl_deinit(&egl_data);
}

static void window_demo(void) {
    GLFWwindow* const glfw_window = my_glfw_init(true);
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
}

static void test_matrix_multiplication(void) {
    my_glfw_init(false);
    MyArena arena = my_arena_init(1024 * 1024 * 500);

    MyMat first_mat = my_mat_alloc(&arena, 10000, 10000);
    MyMat second_mat = my_mat_alloc(&arena, first_mat.cols, 1);
    MyMat third_mat = my_mat_alloc(&arena, first_mat.rows, second_mat.cols);
    my_mat_foreach(el, &first_mat) {
        *el = rand() % 100;
    }
    my_mat_foreach(el, &second_mat) {
        *el = rand() % 1000;
    }
    my_mat_foreach(el, &third_mat) {
        *el = NAN;
    }

    MyGLMat first_mat_gl = {.rows = (GLuint)first_mat.rows, .cols = (GLuint)first_mat.cols};
    MyGLMat second_mat_gl = {.rows = (GLuint)second_mat.rows, .cols = (GLuint)second_mat.cols};
    MyGLMat third_mat_gl = {.rows = (GLuint)third_mat.rows, .cols = (GLuint)third_mat.cols};

    GLuint buffers[3];
    ASSERT_GL(glGenBuffers(3, buffers));
    first_mat_gl.ssb = buffers[0];
    second_mat_gl.ssb = buffers[1];
    third_mat_gl.ssb = buffers[2];

    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[0]));
        ASSERT_GL(glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)my_mat_bytes_count(&first_mat), first_mat.items, GL_DYNAMIC_DRAW));
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));

    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[1]));
        ASSERT_GL(glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)my_mat_bytes_count(&second_mat), second_mat.items, GL_DYNAMIC_DRAW));
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));

    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[2]));
        ASSERT_GL(glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)my_mat_bytes_count(&third_mat), NULL, GL_DYNAMIC_DRAW));
    ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));

    GLuint shader_program, compute_shader;
    mygl_create_compute_shader_program(&shader_program, &compute_shader, mygl_matrix_mul_compute_shader);

    clock_t start = clock();
        my_gl_dispatch_compute_mat_mul(shader_program, &first_mat_gl, &second_mat_gl, &third_mat_gl);
    clock_t end = clock();
    const double opengl_elapsed_time = (((double)(end - start)) / CLOCKS_PER_SEC) * 1000;
    LOG("opengl_elapsed_time ms: %lf", opengl_elapsed_time);
    start = clock();
        my_mat_mul(&third_mat, &first_mat, &second_mat);
    end = clock();
    const double cpu_elapsed_time = (((double)(end - start)) / CLOCKS_PER_SEC) * 1000;
    LOG("cpu_elapsed_time ms: %lf", cpu_elapsed_time);
    LOG("cpu_elapsed_time / opengl_elapsed_time: %lf", cpu_elapsed_time / opengl_elapsed_time);
    {
        ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, third_mat_gl.ssb));
            GLfloat *data; 
            ASSERT_GL(data = (GLfloat*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)my_mat_bytes_count(&third_mat), GL_MAP_READ_BIT));
            my_range_for_zero(size_t, i, my_mat_items_count(&third_mat)) {
                // LOG("opengl: %lf, cpu: %lf", (double)data[i], (double)third_mat.items[i]);
                ASSERT(fabsf(data[i] - third_mat.items[i]) < 0.0001f);
            }
            ASSERT_GL(glUnmapBuffer(GL_SHADER_STORAGE_BUFFER));

        ASSERT_GL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));
    } 
    
    ASSERT_GL(glDeleteShader(compute_shader));
    ASSERT_GL(glDeleteProgram(shader_program));
    ASSERT_GL(glDeleteBuffers(my_array_count(buffers), buffers));
    free(arena.items);

    glfwTerminate();
}

static void test_hstack(void) {
    MyArena arena = my_arena_init(1024);
    MyMat first = my_mat_alloc(&arena, 4, 4);
    MyMat second = my_mat_alloc(&arena, 4, 3);

    my_mat_item(&first, 0, 0) = 1;
    my_mat_item(&first, 0, 1) = 1;
    my_mat_item(&first, 0, 2) = 1;
    my_mat_item(&first, 0, 3) = 1;

    my_mat_item(&first, 1, 0) = 2;
    my_mat_item(&first, 1, 1) = 2;
    my_mat_item(&first, 1, 2) = 2;
    my_mat_item(&first, 1, 3) = 2;

    my_mat_item(&first, 2, 0) = 3;
    my_mat_item(&first, 2, 1) = 3;
    my_mat_item(&first, 2, 2) = 3;
    my_mat_item(&first, 2, 3) = 3;

    my_mat_item(&first, 3, 0) = 4;
    my_mat_item(&first, 3, 1) = 4;
    my_mat_item(&first, 3, 2) = 4;
    my_mat_item(&first, 3, 3) = 4;

    my_mat_item(&second, 0, 0) = 5;
    my_mat_item(&second, 0, 1) = 5;
    my_mat_item(&second, 0, 2) = 5;

    my_mat_item(&second, 1, 0) = 6;
    my_mat_item(&second, 1, 1) = 6;
    my_mat_item(&second, 1, 2) = 6;

    my_mat_item(&second, 2, 0) = 7;
    my_mat_item(&second, 2, 1) = 7;
    my_mat_item(&second, 2, 2) = 7;

    my_mat_item(&second, 3, 0) = 8;
    my_mat_item(&second, 3, 1) = 8;
    my_mat_item(&second, 3, 2) = 8;

    const MyMat result = my_mat_hstack(&arena, &first, &second);
    #define ASSERT_MAT_VALUE(row, col, expected_value) ASSERT(fabsf(my_mat_item(&result, (row), (col)) - (expected_value)) < 0.0000000001f)
        ASSERT_MAT_VALUE(0, 0, 1);
        ASSERT_MAT_VALUE(0, 1, 1);
        ASSERT_MAT_VALUE(0, 2, 1);
        ASSERT_MAT_VALUE(0, 3, 1);
        ASSERT_MAT_VALUE(0, 4, 5);
        ASSERT_MAT_VALUE(0, 5, 5);
        ASSERT_MAT_VALUE(0, 6, 5);

        ASSERT_MAT_VALUE(1, 0, 2);
        ASSERT_MAT_VALUE(1, 1, 2);
        ASSERT_MAT_VALUE(1, 2, 2);
        ASSERT_MAT_VALUE(1, 3, 2);
        ASSERT_MAT_VALUE(1, 4, 6);
        ASSERT_MAT_VALUE(1, 5, 6);
        ASSERT_MAT_VALUE(1, 6, 6);

        ASSERT_MAT_VALUE(2, 0, 3);
        ASSERT_MAT_VALUE(2, 1, 3);
        ASSERT_MAT_VALUE(2, 2, 3);
        ASSERT_MAT_VALUE(2, 3, 3);
        ASSERT_MAT_VALUE(2, 4, 7);
        ASSERT_MAT_VALUE(2, 5, 7);
        ASSERT_MAT_VALUE(2, 6, 7);

        ASSERT_MAT_VALUE(3, 0, 4);
        ASSERT_MAT_VALUE(3, 1, 4);
        ASSERT_MAT_VALUE(3, 2, 4);
        ASSERT_MAT_VALUE(3, 3, 4);
        ASSERT_MAT_VALUE(3, 4, 8);
        ASSERT_MAT_VALUE(3, 5, 8);
        ASSERT_MAT_VALUE(3, 6, 8);

    #undef ASSERT_MAT_VALUE

    free(arena.items);
}
static void test_all(void) {
    test_matrix_multiplication();
    // test_hstack();
}

#define my_shift(xs, xs_sz) (ASSERT((xs_sz) > 0), (xs_sz)--, *(xs)++)

int main(int argc, const char* const* argv) {
    my_shift(argv, argc);
    if(argc == 0) {
        my_polynomial_train();
    } else {
        ASSERT(argc == 1);
        const char* const arg = my_shift(argv, argc);
        ASSERT(strcmp(arg, "test") == 0);
        test_all();
    }
    return 0;
}
