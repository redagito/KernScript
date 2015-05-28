#pragma once

#include <cstdint>
#include <string>

enum class EInstruction
{
	// System
	Nop = 0, /**< No operation. */
	Break, /**< Breakpoint, for debugging. */
	Exit, /**< Exit operation, immediately ends script. */

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
	Pop, /**< Pop value from stack. */
	Popv, /**< Pop value from stack into variable. */

	// Function calling
	Call, /**< Call script function. */
	Calle, /**< Call extern function. */
	Ret, /**< Return from script function without return value. */
	Retv, /**< Return from script function with variable as return value. */
	Reti, /**< Return from script function with integer as return value. */
	Retf, /**< Return from script function with float return value. */
	Rets, /**< Return from script function with string return value. */

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
	
	{ "movv", 2 },
	{ "movi", 2 },
	{ "movf", 2 },
	{ "movs", 2 },
	
	{ "pushv", 1 },
	{ "pushi", 1 },
	{ "pushu", 1 },
	{ "pushf", 1 },
	{ "pushs", 1 },
	{ "pop", 0 },
	{ "popv", 1 },
	
	{ "call", 1 },
	{ "calle", 1 },
	{ "ret", 0 },
	{ "retv", 1 },
	{ "reti", 1 },
	{ "retf", 1 },
	{ "rets", 1 },

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
