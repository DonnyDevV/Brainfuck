// Libraries
#include <cstdio>
#include <cstring>
#include <getopt.h>

bool print_bytecode = false;
const char *input_file = nullptr;
FILE *input_stream = nullptr;
// type definitions

// function declaration

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "c")) != -1) {
        switch (opt) {
        case 'c':
            print_bytecode = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-c] program_file\n", argv[0]);
            return 1;
        }
    }

    if (optind < argc) {
        input_file = argv[optind];
        input_stream = fopen(input_file, "rb");
        if (input_stream == nullptr) {
            fprintf(stderr, "Error: Unable to open file %s\n", input_file);
            return 1;
        }
    } else {
        input_stream = stdin;
    }

    if (input_stream != stdin) {
        fclose(input_stream);
    }

    return 0;
}
