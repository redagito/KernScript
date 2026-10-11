#pragma once

#include <string>
#include <istream>

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
  * extern function throws kern::RuntimeError.
  */
  virtual bool callFunction(const std::string &functionName) = 0;

  /**
  * \brief Registers an extern function by name.
  * \param functionName Name scripts use in calle instructions.
  * \param function Implementation of the extern function. Ownership passes to
  *        the virtual machine, which deletes it on removeFunction or on
  *        destruction, so the caller must not delete it. Anything the
  *        function refers to must outlive the virtual machine.
  */
  virtual void addFunction(const ::std::string &functionName,
                           IExternFunction *function) = 0;
  virtual void removeFunction(const ::std::string &functionName) = 0;

  /**
  * \brief Interface for extern functions.
  *
  * Each overload pops one value from the runtime stack. Throws
  * kern::RuntimeError when the stack is empty and when a string value is
  * converted to a numeric type; a throw leaves the value on the stack.
  * Returns false only when conversion is inapplicable (an invalid value);
  * out-of-range numeric conversions throw instead of returning false.
  */
  virtual bool popParameter(int &value) = 0;
  virtual bool popParameter(float &value) = 0;
  virtual bool popParameter(::std::string &value) = 0;

  /**
  * \brief Sets empty return value for functions without return type.
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
}
