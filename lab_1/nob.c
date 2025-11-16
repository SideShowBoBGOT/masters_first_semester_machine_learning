#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    Nob_Cmd cmd = {0};

    // Let's append the command line arguments
    // On POSIX
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"hello", SRC_FOLDER"hello.c");

    // Let's execute the command.
    if (!nob_cmd_run(&cmd)) return 1;
    // nob_cmd_run() automatically resets the cmd array, so you can nob_cmd_append() more strings
    // into it.

    // nob.h ships with a bunch of nob_cc_* macros that try abstract away the specific compiler.
    // They are verify basic and not particularly flexible, but you can redefine them if you need to
    // or not use them at all and create your own abstraction on top of Nob_Cmd.
    
    // CIMGUI_FLAGS:=-O3 -fno-exceptions -fno-rtti -std=c++11 -I$(CIMGUI_IMGUI_DIR) -I$(CIMGUI_IMGUI_DIR)/backends -DIMGUI_IMPL_API="extern \"C\""

    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, BUILD_FOLDER "foo");
    nob_cc_inputs(&cmd, SRC_FOLDER "foo.c");
    if (!nob_cmd_run(&cmd)) return 1;

    return 0;
}
