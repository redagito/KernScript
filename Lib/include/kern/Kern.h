#pragma once

#include "kern/IAssembler.h"
#include "kern/ICompiler.h"
#include "kern/IVirtualMachine.h"

#include "kern/IExternFunction.h"
#include "kern/ILogger.h"
#include "kern/FunctionBinding.h"

namespace kern
{

/**
* \brief Creates an assembler that logs to the spdlog default logger.
*/
IAssembler *createAssembler();

/**
* \brief Creates an assembler.
* \param logger Receives diagnostics, owned by the caller. A null pointer
*        discards them.
*/
IAssembler *createAssembler(ILogger *logger);

/**
* \brief Creates a compiler that logs to the spdlog default logger.
*/
ICompiler *createCompiler();

/**
* \brief Creates a compiler.
* \param logger Receives diagnostics, owned by the caller. A null pointer
*        discards them.
*/
ICompiler *createCompiler(ILogger *logger);

/**
* \brief Creates a virtual machine that logs to the spdlog default logger.
*/
IVirtualMachine *createVirtualMachine();

/**
* \brief Creates a virtual machine.
* \param logger Receives diagnostics, owned by the caller. A null pointer
*        discards them.
*/
IVirtualMachine *createVirtualMachine(ILogger *logger);

/**
* \brief Provides version information.
*/
unsigned int getMajorVersion();
unsigned int getMinorVersion();
}
