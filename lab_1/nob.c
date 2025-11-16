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

#define my_nob_comp_if_needs(object_files, output, input, ...)\
    do {\
        if(nob_needs_rebuild1((output), (input))) {\
            nob_cmd_append(&cmd, "clang", "-o", (output), (input) __VA_OPT__(,) __VA_ARGS__);\
            ASSERT(nob_cmd_run(&cmd));\
        }\
        nob_da_append((object_files), (output));\
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

    typedef struct {
        const char **items;
        size_t count;
        size_t capacity;
    } StringDA;

    StringDA string_da = {0};

    #define COMPILATION_ARGS "-c", "-std=c99", "-Weverything", "-O3"
        my_nob_comp_if_needs(&string_da, BUILD_FOLDER "glad.o", "thirdparty/glad/glad.c", COMPILATION_ARGS);
        my_nob_comp_if_needs(&string_da, BUILD_FOLDER "glad_egl.o", "thirdparty/glad/glad_egl.c", COMPILATION_ARGS);
    #undef COMPILATION_ARGS

    #define COMPILATION_ARGS "-c", "-O3", "-fno-exceptions", "-fno-rtti", "-std=c++11", "-nostdlib++", "-DIMGUI_IMPL_API=extern \"C\""
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "cimgui.o", "thirdparty/cimgui/cimgui.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "cimgui_impl.o", "thirdparty/cimgui/cimgui_impl.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_widgets.o", "thirdparty/cimgui/imgui/imgui_widgets.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_tables.o", "thirdparty/cimgui/imgui/imgui_tables.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_draw.o", "thirdparty/cimgui/imgui/imgui_draw.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_demo.o", "thirdparty/cimgui/imgui/imgui_demo.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui.o", "thirdparty/cimgui/imgui/imgui.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_impl_glfw.o", "thirdparty/cimgui/imgui/imgui_impl_glfw.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "imgui_impl_opengl3.o", "thirdparty/cimgui/imgui/imgui_impl_opengl3.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "cimplot.o", "thirdparty/cimplot/cimplot.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "implot.o", "thirdparty/cimplot/implot/implot.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "implot_demo.o", "thirdparty/cimplot/implot/implot_demo.cpp", COMPILATION_ARGS);
         my_nob_comp_if_needs(&string_da, BUILD_FOLDER "implot_items.o", "thirdparty/cimplot/implot/implot_items.cpp", COMPILATION_ARGS);
    #undef COMPILATION_ARGS

    #define STATIC_LIB "im_and_glad"
    #define STATIC_LIB_FULL_NAME "lib" STATIC_LIB ".a"

    if(nob_needs_rebuild(BUILD_FOLDER STATIC_LIB_FULL_NAME, string_da.items, string_da.count)) {
        nob_cmd_append(&cmd, "ar", "-rc", BUILD_FOLDER STATIC_LIB_FULL_NAME); 
        for(size_t i = 0; i < string_da.count; ++i) {
            nob_cmd_append(&cmd, string_da.items[i]); 
        }
        ASSERT(nob_cmd_run(&cmd));
    }

    string_da.count = 0;

    nob_da_append(&string_da, "polynomial_regression.c");
    nob_da_append(&string_da, BUILD_FOLDER STATIC_LIB_FULL_NAME);
    if(nob_needs_rebuild(BUILD_FOLDER "polynomial_regression", string_da.items, string_da.count)) {
        nob_cmd_append(&cmd, "clang", "-o", BUILD_FOLDER "polynomial_regression", "polynomial_regression.c", "-L", BUILD_FOLDER, "-l" STATIC_LIB, "-O3");
        nob_cmd_append(&cmd, "-std=c11");
        nob_cmd_append(&cmd, "-Werror");
        nob_cmd_append(&cmd, "-Weverything");
        nob_cmd_append(&cmd, "-Wno-variadic-macro-arguments-omitted");
        nob_cmd_append(&cmd, "-Wno-ms-bitfield-padding");
        nob_cmd_append(&cmd, "-Wno-declaration-after-statement");
        nob_cmd_append(&cmd, "-Wno-padded");
        nob_cmd_append(&cmd, "-Wno-unused-variable");
        nob_cmd_append(&cmd, "-Wno-unused-function");
        nob_cmd_append(&cmd, "-Wno-unreachable-code");
        nob_cmd_append(&cmd, "-Wno-unused-macros");
        nob_cmd_append(&cmd, "-Wno-reserved-macro-identifier");
        nob_cmd_append(&cmd, "-Wno-reserved-identifier");
        nob_cmd_append(&cmd, "-lGL");
        nob_cmd_append(&cmd, "-lglfw");
        nob_cmd_append(&cmd, "-lEGL");
        nob_cmd_append(&cmd, "-lm");
        nob_cmd_append(&cmd, "-Wno-address-of-packed-member");
        nob_cmd_append(&cmd, "-Wno-format-nonliteral");
        nob_cmd_append(&cmd, "-Wno-unused-parameter");
        nob_cmd_append(&cmd, "-Wno-unsafe-buffer-usage");
        nob_cmd_append(&cmd, "-Wno-cast-function-type-strict");
        nob_cmd_append(&cmd, "-Wno-pre-c11-compat");
        nob_cmd_append(&cmd, "-Wno-covered-switch-default");
        nob_cmd_append(&cmd, "-lX11");
        nob_cmd_append(&cmd, "-Wno-unused-but-set-variable");
        ASSERT(nob_cmd_run(&cmd));
    }

    #undef STATIC_LIB_FULL_NAME
    #undef STATIC_LIB

    nob_da_free(string_da);
    return 0;
}
