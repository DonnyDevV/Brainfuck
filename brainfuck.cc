/* Analysis
 I benchmarked my Interpreter for Brainfuck programs Mandelbrot.bf from the test cases and two versions from Towers of
Hanoi

1. Compile-time vs run-time optimization tradeoff
No-Opt: Just coding a working solution that passes the tests, no threaded code (switch based instead) or pattern
detection, but without trees or so TTI: TokenThreadingInterpreter (with function pointers) instead of switch interpreter
-> performed generally worse than switch, maybe because of function overhead, maybe skill issue DTI:
DirectThreadingInterpreter (I realised this is rather called also a Token Threading Interpreter just with label
addresses and using opcode index addresses but we still have an extra lookup instead of single memory lookup) instead of
Direct Threading  instead switch interpreter -> performed better for mandelbrot than switch, slightly worse on
non-optimized Hanoi Threaded Coding on the Compiler/Parser instead of switch based approach, didn't improve the
execution time for me, probably because the input program is too small to be worth the effort DTI + Patterns: I added
e.g. Set Zero Pattern which gave roughly 0.5 s improvement for mandelbrot but a huge improvement for the hanoi program
(over 5s) and 0.03s for hanoi_opt; ADD_VAL instead of incrementing/decrementing and MOV_POS instead of moving only by
one to the left or right. These two gave huge improvements (3s for mandelbrot, 3s for Hanoi, 0.05s for Hanoi_opt). Then
I added the add to next pattern, multiply pattern, and set value pattern These three didnt really bring any noticeable
performance boots (if not worse) for me. It is a trade of for more intensive compiling and searching for patterns during
parsings and exploiting created superinstructions in the interpretation phase. I also tried inlining the tape methods
and data to the interpreter but it didn't make a significant difference and therefore I reverted for better readability

 Execution Time (seconds):
Program      | No-Opt  |   TTI    |    DTI    |   DTI + Patterns  |     Speedup
-------------|---------|----------|-----------|-------------------|---------------
mandelbrot.bf|   9.317 |   11.435 |    7.701  |        3.889      |     2.39x
hanoi.bf     |   8.889 |   10.211 |    9.222  |        0.378      |     23.52x
hanoi_opt.bf |   0.378 |    0.375 |    0.276  |        0.201      |     1.88x

2. Potential improvements:
- Nested loop analysis for complex arithmetic operations
- Addition/subtraction chains across (e.g., >++>++++>+++)
- Clear range optimizations ([->][-] as one operation)
- Arithmetic with offset patterns ([->>+<<])
Challenges:
   - Pattern detection becomes more complex with nesting depth and can slow down compilation
   - Memory analysis for range-based optimizations
Example:
   Input: >++>++++>+++
   Current: Compiles to separate MV_POS and ADD_VAL operations
   Potential: Could be one combined operation that adds values to multiple cells
*/

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
    void moveRightBy(uint16_t n) {
        if (position < 2 * TAPE_SIZE - n) {
            position += n;
        } else {
            throw std::out_of_range("Tape overflow");
        }
    }

    void moveLeftBy(uint16_t n) {
        if (position >= n) {
            position -= n;
        } else {
            throw std::out_of_range("Tape underflow");
        }
    }

    void add(int x) {
        if (position >= TAPE_SIZE) {
            right[position - TAPE_SIZE] += x;
        } else {
            left[TAPE_SIZE - 1 - position] += x;
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
};

enum class OpCode : uint16_t {
    OUTPUT,
    INPUT,
    JUMP_FWD,
    JUMP_BACK,
    SET_ZERO,
    ADD_VAL,
    MV_POS,
    ADD_TO_NEXT,
    MULTIPLY_MV,
    SET_VAL,
    SCAN_RIGHT,
    SCAN_LEFT
};

struct Instruction {
    OpCode op;
    size_t jump_ref;
    int32_t value;
};

class DirectThreadingCompiler {
  private:
    struct PatternCheckResults {
        bool found;
        int32_t val;
        size_t len_of_pattern;
    };

    bool is_set_zero_pattern(const std::vector<unsigned char> &ops, size_t start) {
        // [+] or [-]
        if (start + 2 >= ops.size())
            return false;
        if (ops[start] == '[' && (ops[start + 1] == '-' || ops[start + 1] == '+') && ops[start + 2] == ']') {
            return true;
        }
        return false;
    }

    PatternCheckResults is_scan_pattern(const std::vector<unsigned char> &ops, size_t pos) {
        // [>] or [<] - scan until zero cell is found
        if (pos + 2 >= ops.size())
            return {false, 0, 0};

        if (ops[pos] != '[')
            return {false, 0, 0};

        char direction = ops[pos + 1];
        if (direction != '>' && direction != '<')
            return {false, 0, 0};

        if (ops[pos + 2] != ']')
            return {false, 0, 0};

        return {true, direction == '>' ? 1 : 0, 3};
    }

    bool is_add_to_next_pattern(const std::vector<unsigned char> &ops, size_t pos) {
        // [->+<]
        if (pos + 4 >= ops.size())
            return false;
        return ops[pos] == '[' && ops[pos + 1] == '-' && ops[pos + 2] == '>' && ops[pos + 3] == '+' &&
               ops[pos + 4] == '<' && ops[pos + 5] == ']';
    }

    PatternCheckResults is_multiply_move_pattern(const std::vector<unsigned char> &ops, size_t pos) {
        // Pattern [->+(+)*<]
        if (pos + 4 >= ops.size() || ops[pos] != '[' || ops[pos + 1] != '-' || ops[pos + 2] != '>' ||
            (ops[pos + 3] != '+' && ops[pos + 3] != '-'))
            return {false, 0, 0};

        int32_t multiplier = 0;
        size_t i = pos + 3;

        while (i < ops.size() && (ops[i] == '+' || ops[i] == '-')) {
            multiplier += (ops[i] == '+') ? 1 : -1;
            i++;
        }

        if (i >= ops.size() || ops[i] != '<' || i + 1 >= ops.size() || ops[i + 1] != ']')
            return {false, 0, 0};

        return {true, multiplier, i + 2 - pos};
    }

    PatternCheckResults is_set_value_pattern(const std::vector<unsigned char> &ops, size_t pos) {
        // [-] followed by any number of + or - (combines set zero and count repeated chars to one op)
        if (pos + 2 >= ops.size())
            return {false, 0, 0};
        if (ops[pos] != '[' || ops[pos + 1] != '-' || ops[pos + 2] != ']')
            return {false, 0, 0};

        int32_t value = 0;
        size_t i = pos + 3;
        while (i < ops.size() && (ops[i] == '+' || ops[i] == '-')) {
            value += (ops[i] == '+') ? 1 : -1;
            i++;
        }

        if (value != 0) // Only optimize if we're actually setting a value
            return {true, value, i - pos};
        return {false, 0, 0};
    }

    size_t count_repeated_chars(const std::vector<unsigned char> &ops, size_t start, char target) {
        size_t count = 1;
        while (start + count < ops.size() && ops[start + count] == target) {
            count++;
        }
        return count;
    }

  public:
    std::vector<Instruction> compile(const std::vector<unsigned char> &ops) {
        static const void *dispatch_table[] = {
            [0 ... 255] = &&parse_unknown,          dispatch_table['>'] = &&parse_mv_right,
            dispatch_table['<'] = &&parse_mv_left,  dispatch_table['+'] = &&parse_inc_val,
            dispatch_table['-'] = &&parse_dec_val,  dispatch_table['.'] = &&parse_output,
            dispatch_table[','] = &&parse_input,    dispatch_table['['] = &&parse_jmp_fwd,
            dispatch_table[']'] = &&parse_jmp_back,
        };
        std::vector<Instruction> bytecode;
        std::stack<size_t> loop_stack;
        size_t i = 0;
#define NEXT_CHAR_N(n)                                                                                                 \
    i += n;                                                                                                            \
    if (i < ops.size())                                                                                                \
        goto *dispatch_table[ops[i]];                                                                                  \
    return bytecode;
        if (ops.empty())
            return bytecode;

#define NEXT_CHAR NEXT_CHAR_N(1)

        uint16_t repeated_char_count = 0;
        goto *dispatch_table[ops[i]];

    parse_mv_right:
        repeated_char_count = count_repeated_chars(ops, i, '>');
        bytecode.push_back({OpCode::MV_POS, 0, repeated_char_count});
        NEXT_CHAR_N(repeated_char_count);
    parse_mv_left:
        repeated_char_count = count_repeated_chars(ops, i, '<');
        bytecode.push_back({OpCode::MV_POS, 0, -repeated_char_count});
        NEXT_CHAR_N(repeated_char_count);
    parse_inc_val:
        repeated_char_count = count_repeated_chars(ops, i, '+');
        bytecode.push_back({OpCode::ADD_VAL, 0, repeated_char_count});
        NEXT_CHAR_N(repeated_char_count);
    parse_dec_val:
        repeated_char_count = count_repeated_chars(ops, i, '-');
        bytecode.push_back({OpCode::ADD_VAL, 0, -repeated_char_count});
        NEXT_CHAR_N(repeated_char_count);
    parse_output:
        bytecode.push_back({OpCode::OUTPUT, 0, 0});
        NEXT_CHAR;
    parse_input:
        bytecode.push_back({OpCode::INPUT, 0, 0});
        NEXT_CHAR;
    parse_jmp_fwd: {
        PatternCheckResults pattern;
        pattern = is_set_value_pattern(ops, i);
        if (pattern.found) {
            bytecode.push_back({OpCode::SET_VAL, 0, pattern.val});
            NEXT_CHAR_N(pattern.len_of_pattern);
        }
        if (is_set_zero_pattern(ops, i)) {
            bytecode.push_back({OpCode::SET_ZERO, 0, 0});
            NEXT_CHAR_N(3);
        }
        pattern = is_scan_pattern(ops, i);
        if (pattern.found) {
            if (pattern.val) {
                bytecode.push_back({OpCode::SCAN_RIGHT, 0, 0});
            } else {
                bytecode.push_back({OpCode::SCAN_LEFT, 0, 0});
            }
        }
        if (is_add_to_next_pattern(ops, i)) {
            bytecode.push_back({OpCode::ADD_TO_NEXT, 0, 0});
            NEXT_CHAR_N(6)
        }
        pattern = is_multiply_move_pattern(ops, i);
        if (pattern.found) {
            bytecode.push_back({OpCode::MULTIPLY_MV, 0, pattern.val});
            NEXT_CHAR_N(pattern.len_of_pattern);
        } else {
            loop_stack.push(bytecode.size());
            bytecode.push_back({OpCode::JUMP_FWD, 0, 0});
            NEXT_CHAR;
        }
    }
    parse_jmp_back:
        if (!loop_stack.empty()) { // dont throw error here
            bytecode[loop_stack.top()].jump_ref = bytecode.size();
            bytecode.push_back({OpCode::JUMP_BACK, loop_stack.top(), 0});
            loop_stack.pop();
        }
        NEXT_CHAR;
    parse_unknown:
        NEXT_CHAR;
    }
};

class DirectThreadingInterpreter {
  private:
    TwoEndedTape tape;

  public:
    void interprete(const std::vector<Instruction> &bytecode) {
        if (bytecode.empty())
            return;

        static void *dispatch_table[] = {&&do_output,      &&do_input,   &&do_jmp_fwd,    &&do_jmp_back,
                                         &&do_set_zero,    &&do_add_val, &&do_mv_pos,     &&do_add_to_next,
                                         &&do_multiply_mv, &&do_set_val, &&do_scan_right, &&do_scan_left};
        size_t pc = 0;
#define DISPATCH goto *dispatch_table[static_cast<int>(bytecode[pc].op)]
#define NEXT                                                                                                           \
    if (++pc < bytecode.size())                                                                                        \
        DISPATCH;                                                                                                      \
    return;

        DISPATCH;

    do_output:
        putchar(tape.get_curr());
        NEXT;
    do_input:
        tape.set_curr(std::cin.get());
        NEXT;
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
    do_set_zero:
        tape.set_curr(0);
        NEXT;
    do_add_val:
        tape.add(bytecode[pc].value);
        NEXT;
    do_mv_pos:
        if (bytecode[pc].value >= 0) {
            tape.moveRightBy(bytecode[pc].value);
        } else {
            tape.moveLeftBy(-bytecode[pc].value);
        }
        NEXT;
    do_add_to_next: {
        uint16_t tmp = tape.get_curr();
        tape.set_curr(0);
        tape.moveRightBy(1);
        tape.add(tmp);
        tape.moveLeftBy(1);
    }
        NEXT;
    do_multiply_mv: {
        uint32_t tmp = tape.get_curr();
        tape.set_curr(0);
        tape.moveRightBy(1);
        tape.add(tmp * bytecode[pc].value);
        tape.moveLeftBy(1);
    }
        NEXT;
    do_set_val:
        tape.set_curr(bytecode[pc].value);
        NEXT;
    do_scan_right: {
        while (tape.get_curr() != 0) {
            tape.moveRightBy(1);
        }
        NEXT;
    }
    do_scan_left: {
        while (tape.get_curr() != 0) {
            tape.moveLeftBy(1);
        }
        NEXT;
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
        putchar(static_cast<uint16_t>(instr.op));
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

// SWITCH BASED VERSION of the PARSER / COMPILER
//  class Compiler {
//    public:
//      std::vector<Instruction> compile(const std::vector<unsigned char> &ops) {
//          std::vector<Instruction> bytecode;
//          std::stack<size_t> loop_stack;
//
//          for (unsigned char op : ops) {
//              switch (op) {
//              case '.':
//                  bytecode.push_back({OpCode::OUTPUT, 0});
//                  break;
//              case ',':
//                  bytecode.push_back({OpCode::INPUT, 0});
//                  break;
//              case '-':
//                  bytecode.push_back({OpCode::DEC_VAL, 0});
//                  break;
//              case '+':
//                  bytecode.push_back({OpCode::INC_VAL, 0});
//                  break;
//              case '<':
//                  bytecode.push_back({OpCode::MV_LEFT, 0});
//                  break;
//              case '>':
//                  bytecode.push_back({OpCode::MV_RIGHT, 0});
//                  break;
//              case '[':
//                  loop_stack.push(bytecode.size());
//                  bytecode.push_back({OpCode::JUMP_FWD, 0});
//                  break;
//              case ']':
//                  if (!loop_stack.empty()) { // Don't throw an error here
//                      bytecode[loop_stack.top()].jump_ref = bytecode.size();
//                      bytecode.push_back({OpCode::JUMP_BACK, loop_stack.top()});
//                      loop_stack.pop();
//                  }
//                  break;
//              }
//          }
//          return bytecode;
//      }
//  };

// TOKEN THREADING WITH FUNCTION POINTERS INTERPRETER VERSION
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
