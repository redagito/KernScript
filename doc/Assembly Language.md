KernScript Assembly Language
===========================

String literals
---------------

A string literal is delimited by double quotes ('"'). The quotes are
not part of the lexeme. Implemented in Lexer::lexString
(Lib/Impl/source/common/Lexer.cpp).

Backslash escapes are kept verbatim: the lexer stores the backslash
and the character after it exactly as written, without translating
them. For example the source text "a\"b" lexes to the 4-character
value a\"b (a, backslash, quote, b). A \n sequence stays two
characters (backslash, n); it is NOT converted to a newline.

The backslash only controls termination: a quote preceded by an odd
run of escapes does not close the string, so an escaped quote stays
inside the value instead of ending it.

A string must close on the same line. A newline or end of stream
before the closing quote yields an Invalid token, and the value is
discarded. An unterminated string at end of stream is Invalid as well.

Verbatim values flow unchanged through the assembler string table
(movs, pushs, rets) into the bytecode and into Value at runtime: what
the lexer kept is what the script compares and prints.

See TestCLexer.cpp "Lexer keeps backslash escapes inside string
literals", which asserts this retention. Do not change the lexer to
translate escapes without updating this document and that test.

Modules and functions
---------------------

A script is assembly text assembled by Assembler::assemble
(Lib/Impl/source/asm/Assembler.cpp) into bytecode, then run by
VirtualMachine::callFunction. An optional single `module <name>`
declaration names the script; declaring it twice is an error.

Each script function has the form:

  func <name> { ... }

Redefining a function name is an error. Calls (`call <name>`) and
extern calls (`calle <name>`) may refer to functions declared later
in the script: they are resolved after the full parse, so forward
references and mutual recursion assemble.

Parameters and variables
------------------------

Inside a function body:

  param <name>   declares a parameter slot
  var <name>     declares one variable slot
  vara <name> <size>  declares an array of <size> slots (<size> >= 1)

Parameters share the variable namespace: redeclaring any existing
name (param or var) is an error. The frame size is the total of all
declared slots; parameters are the first parameterSize slots.
A caller pushes argument values onto the runtime stack and then
executes `call`; the callee's frame base is set so the pushed values
land in its parameter slots. Values are read and written with
movv/movi/movf/movs, pushv, popv; see the opcode list in
Lib/Impl/source/common/Instructions.h.

Extern declarations and call discipline
---------------------------------------

An extern function is declared as:

  extern <name> <argCount>

where <argCount> is an integer 0..255 (it serializes as one byte).
Redeclaring an extern name is an error.

At runtime, `calle` requires exactly <argCount> values pushed above
the current frame; fewer or more throws kern::RuntimeError. The
extern must be registered on the host side with
IVirtualMachine::addFunction, else the call throws; an extern whose
call() method returns false also stops the script with
kern::RuntimeError. popParameter and setReturnValue may only be used
inside an extern call; any other use throws.

Return chaining: return values a previous extern call left on the
stack count toward the next call's argument count. Scripts must
consume (or pop) values they do not need before pushing the next
call's arguments. A return value flowing directly into the next
extern call (for example `calle get_value` followed by
`calle println`) is accepted by this stack-contents count. See the
"Calle" comment block in VirtualMachine::execute
(Lib/Impl/source/vm/VirtualMachine.cpp).

Labels and jumps
----------------

  label <name>   marks the next instruction index

Duplicate labels in one function are an error; jumping to an unknown
label is an error. Conditional jumps pop two values and branch on
the comparison:

  jmp <label>    unconditional
  je <label>     jump if x == y
  jne <label>    jump if x != y
  jg <label>     jump if x > y
  jge <label>    jump if x >= y
  jl <label>     jump if x < y
  jle <label>    jump if x <= y

A jump whose target is the end of the function (a trailing label that
resolves to instructionSize) is legal: the VM treats an instruction
index past the last instruction as an implicit return, so a function
ending in a label needs no redundant ret. Falling off the end of a
function likewise returns implicitly. `ret` without a caller ends the
script; `exit` ends it immediately.

Comparisons and strings
-----------------------

Equality (je/jne, Value::operator==/!= in Lib/Impl/source/vm/Value.cpp)
accepts strings: two strings compare by value (strcmp), but a string
and a non-string are never equal. Ordered comparisons
(jle/jg/jge/jl, Value::operator<=/</>=/>) return false for any
non-numeric operand pair, including strings (and NaN): the jump is
simply not taken and execution falls through. So string scripts must
branch with je/jne only.

Execution step budget
---------------------

Each callFunction run executes at most VMConfig::maxExecutionSteps
instructions (default 10000000, see
Lib/ApiHeaders/include/kern/VMConfig.h); the count resets on every
call. Exceeding the budget throws kern::RuntimeError, which also
stops a script that jumps to itself forever. Other VMConfig limits
(maxStackBytes, maxCallDepth, maxStringBytes, maxTableEntries) bound
the runtime stack, call depth and bytecode tables the same way.
