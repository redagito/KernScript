#pragma once

#include <cstdint>
#include <string>

typedef uint16_t InstructionId;

// System
const InstructionId instrNop = 0; /**< No operation. */
const InstructionId instrPause = 1; /**< Pause operation (breakpoint, for debugging). */
const InstructionId instrExit = 2; /**< Exit operation, immediately ends script. */

// Stack management
const InstructionId instrPush = 3; /**< Push value to stack. */
const InstructionId instrPop = 4; /**< Pop value from stack. */

// Function calling
const InstructionId instrCall = 5; /**< Call script function. */
const InstructionId instrCalle = 6; /**< Call extern function. */

// Arithmetic operations
const InstructionId instrAdd = 7; /**< Add. */
const InstructionId instrSub = 8; /**< Subtract. */
const InstructionId instrMul = 9; /**< Multiply. */
const InstructionId instrDiv = 10; /**< Divide. */
const InstructionId instrInc = 11; /**< Increment. */
const InstructionId instrDec = 12; /**< Decrement. */

// Logical operations
const InstructionId instrAnd = 13; /**< Logical AND. */
const InstructionId instrOr = 14; /**< Logical OR. */
const InstructionId instrNot = 15; /**< Logical NOT. */
const InstructionId instrXor = 16; /**< Logical XOR. */

// Flow control
const InstructionId instrJmp = 18; /**< Unconditional jump. */
const InstructionId instrJe = 19; /**< Jump if equal. */
const InstructionId instrJne = 20; /**< Jump if not equal. */