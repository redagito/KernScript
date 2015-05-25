#pragma once

#include <cstdint>
#include <string>

enum class EInstructíon
{
	// System
	Nop = 0, /**< No operation. */
	Break, /**< Breakpoint, for debugging. */
	Exit, /**< Exit operation, immediately ends script. */

	// Runtime stack manipulation
	Pushv, /**< Push value to stack. */
	Pushi, /**< Push integer constant to stack. */
	Pushu, /**< Push unsigned integer constant to stack. */
	Pushf, /**< Push float constant to stack. */
	Pushs, /**< Push string constant to stack. */
	Pop, /**< Pop value from stack. */

	// Function calling
	Call, /**< Call script function. */
	Calle, /**< Call extern function. */
	Ret, /**< Return from script function. */

	// Arithmetic operations
	Add, /**< Add. */
	Sub, /**< Subtract. */
	Mul, /**< Multiply. */
	Div, /**< Divide. */
	Inc, /**< Increment. */
	Dec, /**< Decrement. */

	// Logical operations
	And, /**< Logical AND. */
	Or, /**< Logical OR. */
	Not, /**< Logical NOT. */
	Xor, /**< Logical XOR. */

	// Flow control
	Jmp, /**< Unconditional jump. */
	Je, /**< Jump if equal. */
	Jne, /**< Jump if not equal. */
	Jg, /**< Jump if greater. */
	Jge, /**< Jump if greater or equal. */
	Jl, /**< Jump if less. */
	Jle /**< Jump if less or equal. */
};

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
	{ "break", 0 },
	{ "exit", 0 },
	
	{ "pushv", 1 },
	{ "pushi", 1 },
	{ "pushu", 1 },
	{ "pushf", 1 },
	{ "pushs", 1 },
	{ "pop", 0 },
	
	{ "call", 1 },
	{ "calle", 1 },
	{ "ret", 0 },

	{ "add", 0 },
	{ "sub", 0 },
	{ "mul", 0 },
	{ "div", 0 },

	{ "inc", 0 },
	{ "dec", 0 },

	{ "and", 2 },
	{ "or", 2 },
	{ "not", 2 },
	{ "xor", 2 },

	{ "jmp", 1 },
	{ "je", 1 },
	{ "jne", 1 },
	{ "jg", 1 },
	{ "jge", 1 },
	{ "jl", 1 },
	{ "jle", 1 }
}; 
