#pragma once

#include <istream>
#include <string>

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
    virtual void removeFunction(const ::std::string &functionName) = 0;

    /**
     * \brief Interface for extern functions.
     *
     * Each overload pops one value from the runtime stack. Throws
     * kern::RuntimeError when the stack is empty and when a string value is
     * converted to a numeric type; a throw leaves the value on the stack.
     * Returns false only when conversion is inapplicable (an invalid value);
     * out-of-range numeric conversions throw instead of returning false.
     * Calling popParameter outside an extern call throws, as does popping at
     * or below the current frame: only values pushed above the frame (the
     * call's arguments and earlier return values) may be taken.
     *
     * Extern call discipline: a script calle takes exactly the declared number
     * of argument values from above the current frame. Return values a previous
     * extern call left on the stack count toward the next call, so scripts must
     * consume or pop values they do not need before pushing the next call's
     * arguments.
     */
    virtual bool popParameter(int &value) = 0;
    virtual bool popParameter(float &value) = 0;
    virtual bool popParameter(::std::string &value) = 0;
    /**
     * \brief Pops a double value, widened from any numeric type.
     */
    virtual bool popParameter(double &value) = 0;
    /**
     * \brief Pops an unsigned integer value, throwing when a signed value is
     * negative.
     */
    virtual bool popParameter(unsigned int &value) = 0;
    /**
     * \brief Pops a boolean value; zero is the only false value.
     */
    virtual bool popParameter(bool &value) = 0;

    /**
     * \brief Sets empty return value for functions without return type.
     *
     * Calling any setReturnValue overload outside an extern call throws
     * kern::RuntimeError.
     */
    virtual void setReturnValue() = 0;
    virtual void setReturnValue(int value) = 0;
    virtual void setReturnValue(float value) = 0;
    virtual void setReturnValue(const std::string &value) = 0;
    /**
     * \brief Sets a double return value, narrowed to the runtime float type.
     */
    virtual void setReturnValue(double value) = 0;
    /**
     * \brief Sets an unsigned integer return value.
     */
    virtual void setReturnValue(unsigned int value) = 0;
    /**
     * \brief Sets a boolean return value, stored as 1 or 0 like the logical
     * operators produce.
     */
    virtual void setReturnValue(bool value) = 0;
};
} // namespace kern
