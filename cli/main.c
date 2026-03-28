/*
 * Usage: caravel <command> [args...]
 *
 * Commands:
 *   init <name>   Create a new Caravel project
 *   build         Compile the program for SBF
 *   test          Build and run the test suite
 *   deploy        Deploy program to a Solana cluster
 *   clean         Remove build artifacts
 *   idl           Generate Anchor-compatible IDL JSON
 */

#include "cli.h"

static void print_usage(void) {
    printf(
        "\n"
        "  Caravel v%s\n"
        "\n"
        "  Usage: caravel <command> [options]\n"
        "\n"
        "  Commands:\n"
        "    init <name>   Create a new project\n"
        "    build         Compile program to SBF\n"
        "    test          Build and run tests\n"
        "    deploy        Deploy to Solana cluster\n"
        "    clean         Remove build artifacts\n"
        "    idl           Generate IDL (build/idl.json)\n"
        "\n"
        "  Build options:\n"
        "    --backend=<name>  platform-tools (default), sbpf-linker\n"
        "    --debug           Debug build with symbols\n"
        "    --fast            Fast build (-O1)\n"
        "\n"
        "  Options:\n"
        "    --help, -h        Show this message\n"
        "    --version         Show version\n"
        "\n",
        CVL_CLI_VERSION
    );
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }
    if (strcmp(cmd, "--version") == 0) {
        printf("caravel %s\n", CVL_CLI_VERSION);
        return 0;
    }

    int sub_argc = argc - 2;
    char **sub_argv = argv + 2;

    if (strcmp(cmd, "init") == 0)   return cmd_init(sub_argc, sub_argv);
    if (strcmp(cmd, "build") == 0)  return cmd_build(sub_argc, sub_argv);
    if (strcmp(cmd, "test") == 0)   return cmd_test(sub_argc, sub_argv);
    if (strcmp(cmd, "deploy") == 0) return cmd_deploy(sub_argc, sub_argv);
    if (strcmp(cmd, "clean") == 0)  return cmd_clean(sub_argc, sub_argv);
    if (strcmp(cmd, "idl") == 0)    return cmd_idl(sub_argc, sub_argv);

    fprintf(stderr, "err: unknown command '%s'\n", cmd);
    print_usage();
    return 1;
}
