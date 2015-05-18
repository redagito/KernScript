#pagma once

#include "SInstruction.h"

// System
SInstruction instrNop = { "nop", 0 }; /**< No operation. */
SInstruction instrPause = { "pause", 1 }; /**< Pause operation (breakpoint, for debugging). */
SInstruction instrExit = { "exit", 2 }; /**< Exit operation, immediately ends script. */

// Stack management
SInstruction instrPush = { "push", 3 }; /**< Push value to stack. */
SInstruction instrPop = { "pop", 4 }; /**< Pop value from stack. */

// Function calling
SInstruction instrCall = { "call", 5 }; /**< Call script function. */
SInstruction instrCalle = { "calle", 6 }; /**< Call extern function. */

// Arithmetic operations
SInstruction instrAdd = { "add", 7 }; /**< Add. */
SInstruction instrSub = { "sub", 8 }; /**< Subtract. */
SInstruction instrMul = { "mul", 9 }; /**< Multiply. */
SInstruction instrDiv = { "div", 10 }; /**< Divide. */
SInstruction instrInc = { "inc", 11 }; /**< Increment. */
SInstruction instrDec = { "dec", 12 }; /**< Decrement. */

// Logical operations
SInstruction instrAnd = { "and", 14 }; /**< Logical AND. */
SInstruction instrOr = { "or", 15 }; /**< Logical OR. */
SInstruction instrNot = { "not", 16 }; /**< Logical NOT. */
SInstruction instrXor = { "xor", 17 }; /**< Logical XOR. */

// Flow control
SInstruction instrJmp = { "jmp", 18 }; /**< Unconditional jump. */
SInstruction instrJe = { "je", 19 }; /**< Jump if equal. */
SInstruction instrJne = { "jne", 20 }; /**< Jump if not equal. */