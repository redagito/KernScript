#pragma once

#include <cstdint>
#include <string>

typedef uint16_t InstructionId;

// System
const InstructionId instrNop = 0; /**< No operation. */
const InstructionId instrPause = 1; /**< Pause operation (breakpoint, for debugging). */
const InstructionId instrExit = 2; /**< Exit operation, immediately ends script. */

// Stack management
const InstructionId instrPushv = 10; /**< Push value to stack. */
const InstructionId instrPushi = 11; /**< Push integer constant to stack. */
const InstructionId instrPushf = 12; /**< Push float constant to stack. */
const InstructionId instrPushs = 13; /**< Push string constant to stack. */
const InstructionId instrPop = 14; /**< Pop value from stack. */

// Function calling
const InstructionId instrCall = 30; /**< Call script function. */
const InstructionId instrCalle = 31; /**< Call extern function. */

// Arithmetic operations
const InstructionId instrAdd = 50; /**< Add. */
const InstructionId instrSub = 51; /**< Subtract. */
const InstructionId instrMul = 52; /**< Multiply. */
const InstructionId instrDiv = 53; /**< Divide. */
const InstructionId instrInc = 54; /**< Increment. */
const InstructionId instrDec = 55; /**< Decrement. */

// Logical operations
const InstructionId instrAnd = 70; /**< Logical AND. */
const InstructionId instrOr = 71; /**< Logical OR. */
const InstructionId instrNot = 72; /**< Logical NOT. */
const InstructionId instrXor = 73; /**< Logical XOR. */

// Flow control
const InstructionId instrJmp = 90; /**< Unconditional jump. */
const InstructionId instrJe = 91; /**< Jump if equal. */
const InstructionId instrJne = 92; /**< Jump if not equal. */