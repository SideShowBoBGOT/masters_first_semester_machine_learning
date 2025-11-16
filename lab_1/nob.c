#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#define ASSERT(expr, ...) \
    do {\
        if(!(expr)) {\
            fprintf(stderr, "%s:%d: %s: Assertion `%s\' failed", __FILE__, __LINE__, __FUNCTION__, #expr);\
            __VA_OPT__(fprintf(stderr, " explanation: " __VA_ARGS__);)\
            fprintf(stderr, "\n");\
            abort();\
        }\
    } while(0)

#define my_nob_comp_if_needs(output, input, ...)\
    do {\
        if(nob_needs_rebuild1((output), (input))) {\
            nob_cmd_append(&cmd, "clang", "-c", "-o", (output), (input) __VA_OPT__(,) __VA_ARGS__);\
            ASSERT(nob_cmd_run(&cmd));\
        }\
    } while(0)

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = {0};
    {
        nob_shift(argv, argc);
        while(argc > 0) {
            const char* const arg = nob_shift(argv, argc);
            if(strcmp(arg, "clean") == 0) {
                nob_cmd_append(&cmd, "rm", "-rf");
                nob_cmd_append(&cmd, BUILD_FOLDER);
                ASSERT(nob_cmd_run(&cmd));
                return 0;
            }
        }
    }
    if(!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    my_nob_comp_if_needs(BUILD_FOLDER "glad.o", "thirdparty/glad/glad.c", "-Weverything", "-O3");
    my_nob_comp_if_needs(BUILD_FOLDER "glad.o", "thirdparty/glad/glad_egl.c", "-Weverything", "-O3");
    
    // my_nob_comp_if_needs(BUILD_FOLDER "cimgui.o", "thirdparty/cimgui/cimgui.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimgui/imgui/backends/", "-DIMGUI_IMPL_API=extern \"C\"");
    // my_nob_comp_if_needs(BUILD_FOLDER "cimgui_impl.o", "thirdparty/cimgui/cimgui_impl.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimgui/imgui/backends/", "-DIMGUI_IMPL_API=extern \"C\"");
    // my_nob_comp_if_needs(BUILD_FOLDER "imgui_impl_glfw.o", "thirdparty/cimgui/imgui/backends/imgui_impl_glfw.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimgui/imgui/backends/", "-DIMGUI_IMPL_API=extern \"C\"");
    // my_nob_comp_if_needs(BUILD_FOLDER "imgui_impl_opengl3.o", "thirdparty/cimgui/imgui/backends/imgui_impl_opengl3.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimgui/imgui/backends/", "-DIMGUI_IMPL_API=extern \"C\"");
    //
    // my_nob_comp_if_needs(BUILD_FOLDER "cimplot.o", "thirdparty/cimplot/cimplot.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimplot/implot/");
    // my_nob_comp_if_needs(BUILD_FOLDER "implot.o", "thirdparty/cimplot/implot/implot.cpp", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-I", "thirdparty/cimgui/", "-I", "thirdparty/cimgui/imgui/", "-I", "thirdparty/cimplot/implot/");
    return 0;
}
