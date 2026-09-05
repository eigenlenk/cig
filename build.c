#define NOB_IMPLEMENTATION
#include "deps/nob.h"
#include <string.h>

#define BIN_FOLDER   "bin/"
#define SRC_FOLDER   "src/"
#define DEPS_FOLDER  "deps/"
#define TESTS_FOLDER "tests/"
#define DEMO_FOLDER  "demo/"

int main(int argc, char **argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);
  
  enum targets {
    TARGET_TEST = 1,
    TARGET_RAYLIB_DEMO = 2
  };
  
  int targets_included = 0;
  
  if (argc == 1) {
    printf("Usage: build <target>\n\n");
    printf("Builds the specified target.\n\n");
    printf("Arguments:\n");
    printf("\ttest\tBuilds the test target\n");
    printf("\tdemo\tBuilds the demo target\n");
    printf("\tall\tBuilds both test and demo targets\n");
    return 0;
  } else {
    int i;
    for (i = 1; i < argc; ++i) {
      if (!strcmp(argv[i], "all")) {
        targets_included = TARGET_TEST | TARGET_RAYLIB_DEMO;
      } else if (!strcmp(argv[i], "test")) {
        targets_included |= TARGET_TEST;
      } else if (!strcmp(argv[i], "demo")) {
        targets_included |= TARGET_RAYLIB_DEMO;
      } else {
        printf("Unknown target '%s'!\n", argv[i]);
        return 1;
      }
    }
  }

  if (!nob_mkdir_if_not_exists(BIN_FOLDER)) {
    return 1;
  }

  Nob_Cmd cmd = { 0 };

  if (targets_included & TARGET_TEST) {
    nob_cmd_append(
      &cmd,
      "gcc",
      "-std=gnu99",
      "-Wall",
      // "-Wextra",
      "-Wno-missing-field-initializers",
      "-Wno-unused-parameter",
      "-Wfatal-errors",
      // "-g",
      
      "-I"SRC_FOLDER,
      "-I"TESTS_FOLDER,
      "-I"DEPS_FOLDER,
      "-I"DEPS_FOLDER"utf8/",
      "-I"DEPS_FOLDER"unity/src/",
      "-I"DEPS_FOLDER"unity/extras/fixture/src/",

      "-DDEBUG",
      "-DUNITY_INCLUDE_PRINT_FORMATTED",
      "-DUNITY_INCLUDE_DOUBLE",

      // "-Wno-initializer-overrides",

      "-o", BIN_FOLDER"tests",

      DEPS_FOLDER"unity/src/unity.c",
      DEPS_FOLDER"unity/extras/fixture/src/fixture.c",
      DEPS_FOLDER"utf8/utf8.c",
      SRC_FOLDER"cigcore.c",
      SRC_FOLDER"cigtext.c",
      SRC_FOLDER"cigimage.c",
      TESTS_FOLDER"main.c",
      TESTS_FOLDER"core/layout.c",
      TESTS_FOLDER"core/state.c",
      TESTS_FOLDER"core/input.c",
      TESTS_FOLDER"core/macros.c",
      TESTS_FOLDER"text/label.c",
      TESTS_FOLDER"text/style.c",
      TESTS_FOLDER"image/image.c",
      TESTS_FOLDER"allocator.c",
      TESTS_FOLDER"types.c"
    );

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
  }
  
  if (targets_included & TARGET_RAYLIB_DEMO) {
    nob_cmd_append(
      &cmd,
      "gcc",
      "-std=gnu99",
      "-Wall",
      // "-Wextra",
      "-Wno-missing-field-initializers",
      "-Wno-unused-parameter",
      "-Wfatal-errors",
      // "-g",
      
      "-I"SRC_FOLDER,
      "-I"DEPS_FOLDER,
      "-I"DEMO_FOLDER"win95/",
      "-I"DEPS_FOLDER"utf8/",
      "-I"DEPS_FOLDER"raylib-6.0_win64_mingw-w64/include",
      
      "-DDEBUG",
      // "-O2",

      "-o", BIN_FOLDER"win95",

      DEPS_FOLDER"utf8/utf8.c",
      SRC_FOLDER"cigcore.c",
      SRC_FOLDER"cigtext.c",
      SRC_FOLDER"cigimage.c",
      DEMO_FOLDER"win95/main.c",
      DEMO_FOLDER"win95/win95.c",
      DEMO_FOLDER"win95/cigext.c",
      DEMO_FOLDER"win95/system/window_manager.c",
      DEMO_FOLDER"win95/components/window.c",
      DEMO_FOLDER"win95/components/button.c",
      DEMO_FOLDER"win95/components/menu.c",
      DEMO_FOLDER"win95/components/file_browser.c",
      DEMO_FOLDER"win95/components/scroller.c",
      DEMO_FOLDER"win95/apps/explorer/explorer.c",
      DEMO_FOLDER"win95/apps/welcome/welcome.c",
      DEMO_FOLDER"win95/apps/games/wordwiz/wordwiz.c",
      DEMO_FOLDER"win95/apps/accessories/calculator/calculator.c",
      
      DEPS_FOLDER"raylib-6.0_win64_mingw-w64/lib/libraylib.a",
      "-lopengl32",
      "-lgdi32",
      "-lwinmm"
    );

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    nob_copy_directory_recursively(DEMO_FOLDER"win95/res", BIN_FOLDER"res");
  }

  return 0;
}
