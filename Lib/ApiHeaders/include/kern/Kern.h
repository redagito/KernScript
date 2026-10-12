#pragma once

#include "kern/IAssembler.h"
#include "kern/ICompiler.h"
#include "kern/IVirtualMachine.h"

#include "kern/FunctionBinding.h"
#include "kern/IExternFunction.h"
#include "kern/ILogger.h"
#include "kern/NullLogger.h"
#include "kern/RuntimeError.h"
#include "kern/VMConfig.h"

namespace kern
{

/**
 * \brief Creates an assembler that logs to the spdlog default logger.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IAssembler *createAssembler();

/**
 * \brief Creates an assembler.
 * \param logger Receives diagnostics, owned by the caller. A null pointer
 *        discards them.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IAssembler *createAssembler(ILogger *logger);

/**
 * \brief Creates a compiler that logs to the spdlog default logger.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
ICompiler *createCompiler();

/**
 * \brief Creates a compiler.
 * \param logger Receives diagnostics, owned by the caller. A null pointer
 *        discards them.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
ICompiler *createCompiler(ILogger *logger);

/**
 * \brief Creates a virtual machine that logs to the spdlog default logger.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IVirtualMachine *createVirtualMachine();

/**
 * \brief Creates a virtual machine.
 * \param logger Receives diagnostics, owned by the caller. A null pointer
 *        discards them.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IVirtualMachine *createVirtualMachine(ILogger *logger);

/**
 * \brief Creates a virtual machine with explicit limits.
 * \param logger Receives diagnostics, owned by the caller. A null pointer
 *        discards them.
 * \param config Bounds the runtime stack and the call depth.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IVirtualMachine *createVirtualMachine(ILogger *logger, VMConfig config);

/**
 * \brief Creates a virtual machine with explicit limits that logs to the
 * spdlog default logger.
 * \param config Bounds the runtime stack and the call depth.
 * \return An owning pointer; the caller takes ownership and must delete it.
 */
IVirtualMachine *createVirtualMachine(VMConfig config);

/**
 * \brief Provides version information.
 */
unsigned int getMajorVersion();
unsigned int getMinorVersion();
} // namespace kern
