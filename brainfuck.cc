#include <getopt.h>
#include <stdexcept>
#include <vector>

const size_t MAX_PROGRAM_SIZE = 30000;
const size_t BUFFER_SIZE = 4096;

bool print_bytecode = false;
const char *input_file = nullptr;
FILE *input_stream = nullptr;

class TwoEndedTape {
  private:
    static const size_t TAPE_SIZE = 20000;
    unsigned char right[TAPE_SIZE] = {0};
    unsigned char left[TAPE_SIZE] = {0};
    size_t position = TAPE_SIZE;

  public:
    void moveRight() {
        if (position < 2 * TAPE_SIZE - 1) {
            ++position;
        } else {
            throw std::out_of_range("Tape overflow");
        }
    }

    void moveLeft() {
        if (position > 0) {
            --position;
        } else {
            throw std::out_of_range("Tape underflow");
        }
    }

    void increment() {
        if (position >= TAPE_SIZE) {
            ++right[position - TAPE_SIZE];
        } else {
            ++left[TAPE_SIZE - 1 - position];
        }
    }

    void decrement() {
        if (position >= TAPE_SIZE) {
            --right[position - TAPE_SIZE];
        } else {
            --left[TAPE_SIZE - 1 - position];
        }
    }

    void set_curr(unsigned char val) {
        if (position >= TAPE_SIZE) {
            right[position - TAPE_SIZE] = val;
        } else {
            left[TAPE_SIZE - 1 - position] = val;
        }
    }

    unsigned char get_curr() {
        if (position >= TAPE_SIZE) {
            return right[position - TAPE_SIZE];
        } else {
            return left[TAPE_SIZE - 1 - position];
        }
    }

    size_t get_Pointer() { return position - TAPE_SIZE; }
};

std::vector<unsigned char> read_program(FILE *stream) {
    std::vector<unsigned char> program;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_bytes_read = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, stream)) > 0) {
        program.insert(program.end(), buffer, buffer + bytes_read);
    }
    return program;
}

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
        if (!input_stream) {
            fprintf(stderr, "Error: Unable to open file %s\n", input_file);
            return 1;
        }
    } else {
        input_stream = stdin;
    }

    std::vector<unsigned char> ops = read_program(input_stream);
    TwoEndedTape tape;

    if (input_stream != stdin) {
        fclose(input_stream);
    }

    return 0;
}
