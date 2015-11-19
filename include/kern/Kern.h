#pragma once

#include "IAssembler.h"
#include "ICompiler.h"
#include "IVirtualMachine.h"

#include "IExternFunction.h"
#include "FunctionBinding.h"

namespace kern
{

IAssembler *createAssembler();
ICompiler *createCompiler();
IVirtualMachine *createVirtualMachine();
}
