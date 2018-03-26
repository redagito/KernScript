#pragma once

#include "kern/IAssembler.h"
#include "kern/ICompiler.h"
#include "kern/IVirtualMachine.h"

#include "kern/IExternFunction.h"
#include "kern/FunctionBinding.h"

namespace kern
{

IAssembler *createAssembler();
ICompiler *createCompiler();
IVirtualMachine *createVirtualMachine();

/**
 * Provides version information.
 */
unsigned int getMajorVersion();
unsigned int getMinorVersion();
}
