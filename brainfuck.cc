#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <vector>

const size_t BUFFER_SIZE = 4096;

const char *input_file = nullptr;

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

enum class OpCode { MV_RIGHT, MV_LEFT, INC_VAL, DEC_VAL, OUTPUT, INPUT, JUMP_FWD, JUMP_BACK };

struct Instruction {
    OpCode op;
    size_t jump_ref;
};

class Compiler {
  public:
    std::vector<Instruction> compile(const std::vector<unsigned char> &ops) {
        std::vector<Instruction> bytecode;
        std::stack<size_t> loop_stack;

        for (unsigned char op : ops) {
            switch (op) {
            case '.':
                bytecode.push_back({OpCode::OUTPUT, 0});
                break;
            case ',':
                bytecode.push_back({OpCode::INPUT, 0});
                break;
            case '-':
                bytecode.push_back({OpCode::DEC_VAL, 0});
                break;
            case '+':
                bytecode.push_back({OpCode::INC_VAL, 0});
                break;
            case '<':
                bytecode.push_back({OpCode::MV_LEFT, 0});
                break;
            case '>':
                bytecode.push_back({OpCode::MV_RIGHT, 0});
                break;
            case '[':
                loop_stack.push(bytecode.size());
                bytecode.push_back({OpCode::JUMP_FWD, 0});
                break;
            case ']':
                if (!loop_stack.empty()) { // Don't throw an error here
                    bytecode[loop_stack.top()].jump_ref = bytecode.size();
                    bytecode.push_back({OpCode::JUMP_BACK, loop_stack.top()});
                    loop_stack.pop();
                }
                break;
            }
        }
        return bytecode;
    }
};

class Interpreter {
  private:
    TwoEndedTape tape;

  public:
    void interprete(const std::vector<Instruction> &bytecode) {
        for (size_t pc; pc < bytecode.size(); ++pc) {
            const Instruction &instr = bytecode[pc];
            switch (instr.op) {
            case OpCode::OUTPUT:
                std::cout.put(tape.get_curr());
                break;
            case OpCode::INPUT:
                tape.set_curr(std::cin.get());
                break;
            case OpCode::DEC_VAL:
                tape.decrement();
                break;
            case OpCode::INC_VAL:
                tape.increment();
                break;
            case OpCode::MV_LEFT:
                tape.moveLeft();
                break;
            case OpCode::MV_RIGHT:
                tape.moveRight();
                break;
            case OpCode::JUMP_FWD:
                if (tape.get_curr() == 0) {
                    pc = instr.jump_ref;
                };
                break;
            case OpCode::JUMP_BACK:
                if (tape.get_curr() != 0) {
                    pc = instr.jump_ref;
                };
                break;
            }
        }
    }
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

void print_bytecode(const std::vector<Instruction> &bytecode) {
    for (const Instruction &instr : bytecode) {
        std::cout.put(static_cast<int>(instr.op));
    }
}

int main(int argc, char *argv[]) {
    int opt;
    FILE *input_stream = nullptr;
    bool should_print_bytecode = false;

    while ((opt = getopt(argc, argv, "c")) != -1) {
        switch (opt) {
        case 'c':
            should_print_bytecode = true;
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

    if (input_stream != stdin) {
        fclose(input_stream);
    }
    Compiler compiler;
    std::vector<Instruction> bytecode = compiler.compile(ops);

    if (should_print_bytecode) {
        print_bytecode(bytecode);
    } else {
        Interpreter interpreter;
        interpreter.interprete(bytecode);
    }

    return 0;
}
