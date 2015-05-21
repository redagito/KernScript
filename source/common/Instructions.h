#pragma once

#include <cstdint>
#include <string>

typedef uint16_t InstructionId;

// System
const InstructionId instrNop = 0; /**< No operation. */
const InstructionId instrPause = 1; /**< Pause operation (breakpoint, for debugging). */
const InstructionId instrExit = 2; /**< Exit operation, immediately ends script. */

// Stack management
const InstructionId instrPushv = 3; /**< Push value to stack. */
const InstructionId instrPushi = 4; /**< Push integer constant to stack. */
const InstructionId instrPushf = 5; /**< Push float constant to stack. */
const InstructionId instrPushs = 6; /**< Push string constant to stack. */
const InstructionId instrPop = 7; /**< Pop value from stack. */

// Function calling
const InstructionId instrCall = 8; /**< Call script function. */
const InstructionId instrCalle = 9; /**< Call extern function. */

// Arithmetic operations
const InstructionId instrAdd = 10; /**< Add. */
const InstructionId instrSub = 11; /**< Subtract. */
const InstructionId instrMul = 12; /**< Multiply. */
const InstructionId instrDiv = 13; /**< Divide. */
const InstructionId instrInc = 14; /**< Increment. */
const InstructionId instrDec = 15; /**< Decrement. */

// Logical operations
const InstructionId instrAnd = 16; /**< Logical AND. */
const InstructionId instrOr = 17; /**< Logical OR. */
const InstructionId instrNot = 18; /**< Logical NOT. */
const InstructionId instrXor = 19; /**< Logical XOR. */

// Flow control
const InstructionId instrJmp = 20; /**< Unconditional jump. */
const InstructionId instrJe = 21; /**< Jump if equal. */
const InstructionId instrJne = 22; /**< Jump if not equal. */

/**
* \brief Provides instruction specific information for id based lookup.
*/
struct SInstructionLookup
{
	std::string mnemonic;
	unsigned int argCount;
};

const SInstructionLookup instructions[] = {
	{ "nop", 0 },
	{ "pause", 0 },
	{ "exit", 0 },
	
	{ "pushv", 1 },
	{ "pushi", 1 },
	{ "pushf", 1 },
	{ "pushs", 1 },
	{ "pop", 0 },
	
	{ "call", 1 },
	{ "calle", 1 },
	{ "add", 2 },
	{ "sub", 2 },
	{ "mul", 2 },
	{ "div", 2 },

	{ "inc", 1 },
	{ "dec", 1 },

	{ "and", 2 },
	{ "or", 2 },
	{ "not", 2 },
	{ "xor", 2 },

	{ "jmp", 1 },
	{ "je", 1 },
	{ "jne", 1 }
}; 
