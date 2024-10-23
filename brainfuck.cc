#include <cstdint>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <vector>

#if !defined(__GNUC__) && !defined(__clang__)
#error "Computed gotos are only supported with GCC/Clang!"
#endif

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

enum class OpCode : uint8_t { MV_RIGHT, MV_LEFT, INC_VAL, DEC_VAL, OUTPUT, INPUT, JUMP_FWD, JUMP_BACK };

struct Instruction {
    OpCode op;
    size_t jump_ref;
};

// Input simply to small to make a significant diference
class DirectThreadingCompiler {
  public:
    std::vector<Instruction> compile(const std::vector<unsigned char> &ops) {
        static void *dispatch_table[256];
        for (int i = 0; i < 256; i++) {
            dispatch_table[i] = &&parse_unknown;
        }

        dispatch_table['>'] = &&parse_mv_right;
        dispatch_table['<'] = &&parse_mv_left;
        dispatch_table['+'] = &&parse_inc_val;
        dispatch_table['-'] = &&parse_dec_val;
        dispatch_table['.'] = &&parse_output;
        dispatch_table[','] = &&parse_input;
        dispatch_table['['] = &&parse_jmp_fwd;
        dispatch_table[']'] = &&parse_jmp_back;

        std::vector<Instruction> bytecode;
        std::stack<size_t> loop_stack;
        size_t i = 0;
#define NEXT_CHAR                                                                                                      \
    ++i;                                                                                                               \
    if (i < ops.size())                                                                                                \
        goto *dispatch_table[ops[i]];                                                                                  \
    return bytecode;
        if (ops.empty())
            return bytecode;
        goto *dispatch_table[ops[i]];

    parse_mv_right:
        bytecode.push_back({OpCode::MV_RIGHT, 0});
        NEXT_CHAR;
    parse_mv_left:
        bytecode.push_back({OpCode::MV_LEFT, 0});
        NEXT_CHAR;
    parse_inc_val:
        bytecode.push_back({OpCode::INC_VAL, 0});
        NEXT_CHAR;
    parse_dec_val:
        bytecode.push_back({OpCode::DEC_VAL, 0});
        NEXT_CHAR;
    parse_output:
        bytecode.push_back({OpCode::OUTPUT, 0});
        NEXT_CHAR;
    parse_input:
        bytecode.push_back({OpCode::INPUT, 0});
        NEXT_CHAR;
    parse_jmp_fwd:
        loop_stack.push(bytecode.size());
        bytecode.push_back({OpCode::JUMP_FWD, 0});
        NEXT_CHAR;
    parse_jmp_back:
        if (!loop_stack.empty()) { // dont throw error here
            bytecode[loop_stack.top()].jump_ref = bytecode.size();
            bytecode.push_back({OpCode::JUMP_BACK, loop_stack.top()});
            loop_stack.pop();
        }
        NEXT_CHAR;
    parse_unknown:
        NEXT_CHAR;
    }
};

// class Compiler {
//   public:
//     std::vector<Instruction> compile(const std::vector<unsigned char> &ops) {
//         std::vector<Instruction> bytecode;
//         std::stack<size_t> loop_stack;
//
//         for (unsigned char op : ops) {
//             switch (op) {
//             case '.':
//                 bytecode.push_back({OpCode::OUTPUT, 0});
//                 break;
//             case ',':
//                 bytecode.push_back({OpCode::INPUT, 0});
//                 break;
//             case '-':
//                 bytecode.push_back({OpCode::DEC_VAL, 0});
//                 break;
//             case '+':
//                 bytecode.push_back({OpCode::INC_VAL, 0});
//                 break;
//             case '<':
//                 bytecode.push_back({OpCode::MV_LEFT, 0});
//                 break;
//             case '>':
//                 bytecode.push_back({OpCode::MV_RIGHT, 0});
//                 break;
//             case '[':
//                 loop_stack.push(bytecode.size());
//                 bytecode.push_back({OpCode::JUMP_FWD, 0});
//                 break;
//             case ']':
//                 if (!loop_stack.empty()) { // Don't throw an error here
//                     bytecode[loop_stack.top()].jump_ref = bytecode.size();
//                     bytecode.push_back({OpCode::JUMP_BACK, loop_stack.top()});
//                     loop_stack.pop();
//                 }
//                 break;
//             }
//         }
//         return bytecode;
//     }
// };

class DirectThreadingInterpreter {
  private:
    TwoEndedTape tape;

  public:
    void interprete(const std::vector<Instruction> &bytecode) {
        static void *dispatch_table[] = {&&do_mv_right, &&do_mv_left, &&do_inc_val, &&do_dec_val,
                                         &&do_output,   &&do_input,   &&do_jmp_fwd, &&do_jmp_back};
        size_t pc = 0;
#define DISPATCH goto *dispatch_table[static_cast<int>(bytecode[pc].op)]
#define NEXT                                                                                                           \
    ++pc;                                                                                                              \
    if (pc < bytecode.size())                                                                                          \
        DISPATCH;                                                                                                      \
    return;

        DISPATCH;

    do_mv_right:
        tape.moveRight();
        NEXT;

    do_mv_left:
        tape.moveLeft();
        NEXT;
    do_inc_val:
        tape.increment();
        NEXT;
    do_dec_val:
        tape.decrement();
        NEXT;
    do_output:
        putchar(tape.get_curr());
        NEXT;
    do_input:
        tape.set_curr(std::cin.get());
    do_jmp_fwd:
        if (tape.get_curr() == 0) {
            pc = bytecode[pc].jump_ref;
        }
        NEXT;
    do_jmp_back:
        if (tape.get_curr() != 0) {
            pc = bytecode[pc].jump_ref;
        }
        NEXT;
    }
};

// class TokenThreadingInterpreter {
//   private:
//     TwoEndedTape tape;
//     // one indirection more than using GNU extension with gotos
//     using Handler = void (*)(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc);
//
//   public:
//     static const Handler dispatch_table[];
//
//     void interprete(const std::vector<Instruction> &bytecode) {
//         size_t pc = 0;
//         dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// };
//
// static void handle_mv_right(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     tape.moveRight();
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_mv_left(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     tape.moveLeft();
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_inc_val(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     tape.increment();
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_dec_val(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     tape.decrement();
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_output(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     putchar(tape.get_curr());
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_input(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     tape.set_curr(std::cin.get());
//     ++pc;
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_jmp_fwd(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     if (tape.get_curr() == 0) {
//         pc = bytecode[pc].jump_ref;
//     } else {
//         ++pc;
//     }
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// static void handle_jmp_back(TwoEndedTape &tape, const std::vector<Instruction> &bytecode, size_t &pc) {
//     if (tape.get_curr() != 0) {
//         pc = bytecode[pc].jump_ref;
//     } else {
//         ++pc;
//     }
//     if (pc < bytecode.size()) {
//         TokenThreadingInterpreter::dispatch_table[static_cast<int>(bytecode[pc].op)](tape, bytecode, pc);
//     }
// }
//
// const TokenThreadingInterpreter::Handler TokenThreadingInterpreter::dispatch_table[] = {
//     handle_mv_right, handle_mv_left, handle_inc_val, handle_dec_val,
//     handle_output,   handle_input,   handle_jmp_fwd, handle_jmp_back};

// class Interpreter {
//   private:
//     TwoEndedTape tape;
//
//   public:
//     void interprete(const std::vector<Instruction> &bytecode) {
//         for (size_t pc = 0; pc < bytecode.size(); ++pc) {
//             const Instruction &instr = bytecode[pc];
//             switch (instr.op) {
//             case OpCode::OUTPUT:
//                 putchar(tape.get_curr());
//                 break;
//             case OpCode::INPUT:
//                 tape.set_curr(std::cin.get());
//                 break;
//             case OpCode::DEC_VAL:
//                 tape.decrement();
//                 break;
//             case OpCode::INC_VAL:
//                 tape.increment();
//                 break;
//             case OpCode::MV_LEFT:
//                 tape.moveLeft();
//                 break;
//             case OpCode::MV_RIGHT:
//                 tape.moveRight();
//                 break;
//             case OpCode::JUMP_FWD:
//                 if (tape.get_curr() == 0) {
//                     pc = instr.jump_ref;
//                 };
//                 break;
//             case OpCode::JUMP_BACK:
//                 if (tape.get_curr() != 0) {
//                     pc = instr.jump_ref;
//                 };
//                 break;
//             }
//         }
//     }
// };

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
        putchar(static_cast<uint8_t>(instr.op));
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
    DirectThreadingCompiler compiler;
    std::vector<Instruction> bytecode = compiler.compile(ops);

    if (should_print_bytecode) {
        print_bytecode(bytecode);
    } else {
        DirectThreadingInterpreter interpreter;
        interpreter.interprete(bytecode);
    }

    return 0;
}
