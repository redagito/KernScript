#pragma once

#include <istream>
#include <string>
#include <memory>
#include "FunctionBinding.h"

namespace kern
{

class IExternFunction; /**< Forward declare, needed for cyclic dependency. */

/**
 * \brief Virtual machine interface class.
 *
 * The virtual machine provides functionality for executing byte code
 *(compiled/assembled scripts)
 * and interfacing with the host application.
 */
class IVirtualMachine
{
public:
    /**
     * \brief Virtual destructor for interface class.
     */
    virtual ~IVirtualMachine() = default;

    /**
     * \brief Loads byte code from input stream.
     *
     * Replaces any previously loaded script. If loading fails, either by
     * returning false or by throwing kern::RuntimeError, no script remains
     * loaded; load again before calling a function.
     */
    virtual bool load(std::istream &byteCode) = 0;

    /**
     * \brief Clears script data.
     *
     * Does not clear extern functions.
     */
    virtual void clearScripts() = 0;

    /**
     * \brief Calls script function.
     *
     * Only zero-argument, void entry functions are supported.
     * Returns false when the function does not exist. Throws kern::RuntimeError
     * when the script cannot continue, for example on stack underflow, a type
     * mismatch or division by zero. Normal termination through ret or exit does
     * not throw. Calls must not nest: calling callFunction again from inside an
     * extern function throws kern::RuntimeError. Each call runs at most
     * VMConfig::maxExecutionSteps instructions, counted from zero on every
     * call; exceeding the budget throws kern::RuntimeError, which also stops
     * a script that jumps to itself forever.
     */
    virtual bool callFunction(const std::string &functionName) = 0;

    /**
     * \brief Registers an extern function by name.
     * \param functionName Name scripts use in calle instructions. Must not be
     *        empty, matching the wire format which refuses empty names.
     * \param function Implementation of the extern function. Must not be null.
     *        Ownership passes to
     *        the virtual machine only on success, which deletes it on
     *        removeFunction or on destruction, so the caller must not delete it.
     *        On failure (a reentrant call, an empty name or a null function)
     *        ownership stays with
     *        the caller, which must delete it. Anything the function refers to
     *        must outlive the virtual machine. Registering a name again
     *        replaces the previous registration: the virtual machine deletes
     *        the previously registered object.
     */
    virtual void addFunction(const ::std::string &functionName, IExternFunction *function) = 0;

    /** Registers a typed C++ function; ownership transfers only after successful registration. */
    template <typename ReturnType, typename... Arguments>
    void addFunction(const std::string &name, ReturnType (*function)(Arguments...))
    {
        std::unique_ptr<IExternFunction> binding(createExternFunction(function));
        addFunction(name, binding.get());
        binding.release();
    }
    virtual void removeFunction(const ::std::string &functionName) = 0;
};
} // namespace kern
