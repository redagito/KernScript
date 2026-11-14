#pragma once

#include <array>
#include <cstdint>
#include <string>

enum class Opcode
{
    // System
    Nop = 0, /**< No operation. */
    Break,   /**< Breakpoint, for debugging. */
    Exit,    /**< Exit operation, immediately ends script. */

    // Data management
    Movv, /**< Move variable to variable. */
    Movi, /**< Move integer to variable. */
    Movf, /**< Move float to variable. */
    Movs, /**< Move string to variable. */

    // Runtime stack manipulation
    Pushv, /**< Push value to stack. */
    Pushi, /**< Push integer constant to stack. */
    Pushu, /**< Push unsigned integer constant to stack. */
    Pushf, /**< Push float constant to stack. */
    Pushs, /**< Push string constant to stack. */
    Pop,   /**< Pop value from stack. */
    Popv,  /**< Pop value from stack into variable. */

    // Function calling
    Call,  /**< Call script function. */
    Calle, /**< Call extern function. */
    Ret,   /**< Return from script function without return value. */
    Retv,  /**< Return from script function with variable as return value. */
    Reti,  /**< Return from script function with integer as return value. */
    Retf,  /**< Return from script function with float return value. */
    Rets,  /**< Return from script function with string return value. */

    // Arithmetic operations
    Add, /**< Add. */
    Sub, /**< Subtract. */
    Mul, /**< Multiply. */
    Div, /**< Divide. */
    Inc, /**< Increment. */
    Dec, /**< Decrement. */

    // Logical operations
    And, /**< Logical AND. */
    Or,  /**< Logical OR. */
    Not, /**< Logical NOT. */
    Xor, /**< Logical XOR. */

    // Flow control
    Jmp, /**< Unconditional jump. */
    Je,  /**< Jump if equal. */
    Jne, /**< Jump if not equal. */
    Jg,  /**< Jump if greater. */
    Jge, /**< Jump if greater or equal. */
    Jl,  /**< Jump if less. */
    Jle, /**< Jump if less or equal. */
    // Append to preserve existing wire opcode numbers.
    Neg /**< Numeric negation, preserving floating-point signed zero. */
};

// One instruction schema drives assembly operand parsing, linking, validation,
// and stack-effect verification. Calls obtain their effect from signatures.
enum class OperandKind
{
    None,
    Local,
    Integer,
    Unsigned,
    Float,
    String,
    Function,
    Extern,
    Label
};
enum class FlowKind
{
    Next,
    Jump,
    Conditional,
    Return,
    Exit,
    Call,
    ExternCall,
    Alias
};
struct InstructionLookup
{
    std::string mnemonic;
    unsigned int argCount;
    std::array<OperandKind, 4> operands;
    unsigned int pops;
    unsigned int pushes;
    FlowKind flow = FlowKind::Next;
};
inline const InstructionLookup instructions[] = {
    {"nop", 0, {}, 0, 0},
    {"break", 0, {}, 0, 0},
    {"exit", 0, {}, 0, 0, FlowKind::Exit},
    {"movv", 2, {OperandKind::Local, OperandKind::Local}, 0, 0},
    {"movi", 2, {OperandKind::Local, OperandKind::Integer}, 0, 0},
    {"movf", 2, {OperandKind::Local, OperandKind::Float}, 0, 0},
    {"movs", 2, {OperandKind::Local, OperandKind::String}, 0, 0},
    {"pushv", 1, {OperandKind::Local}, 0, 1},
    {"pushi", 1, {OperandKind::Integer}, 0, 1},
    {"pushu", 1, {OperandKind::Unsigned}, 0, 1},
    {"pushf", 1, {OperandKind::Float}, 0, 1},
    {"pushs", 1, {OperandKind::String}, 0, 1},
    {"pop", 0, {}, 1, 0},
    {"popv", 1, {OperandKind::Local}, 1, 0},
    {"call", 1, {OperandKind::Function}, 0, 0, FlowKind::Call},
    {"calle", 1, {OperandKind::Extern}, 0, 0, FlowKind::ExternCall},
    {"ret", 0, {}, 0, 0, FlowKind::Return},
    {"retv", 1, {OperandKind::Local}, 0, 0, FlowKind::Alias},
    {"reti", 1, {OperandKind::Integer}, 0, 0, FlowKind::Alias},
    {"retf", 1, {OperandKind::Float}, 0, 0, FlowKind::Alias},
    {"rets", 1, {OperandKind::String}, 0, 0, FlowKind::Alias},
    {"add", 0, {}, 2, 1},
    {"sub", 0, {}, 2, 1},
    {"mul", 0, {}, 2, 1},
    {"div", 0, {}, 2, 1},
    {"inc", 0, {}, 1, 1},
    {"dec", 0, {}, 1, 1},
    {"and", 0, {}, 2, 1},
    {"or", 0, {}, 2, 1},
    {"not", 0, {}, 1, 1},
    {"xor", 0, {}, 2, 1},
    {"jmp", 1, {OperandKind::Label}, 0, 0, FlowKind::Jump},
    {"je", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"jne", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"jg", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"jge", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"jl", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"jle", 1, {OperandKind::Label}, 2, 0, FlowKind::Conditional},
    {"neg", 0, {}, 1, 1},
};

static_assert(std::size(instructions) == static_cast<size_t>(Opcode::Neg) + 1);
