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
  * not throw.
  */
  virtual bool callFunction(const std::string &functionName) = 0;

  /**
  * \brief Extern function management.
  */
  virtual void addFunction(const ::std::string &functionName,
                           IExternFunction *function) = 0;
  virtual void removeFunction(const ::std::string &functionName) = 0;

  /**
  * \brief Interface for extern functions.
  *
  * Each overload pops one value from the runtime stack. Returns false when
  * the value cannot be converted. Throws kern::RuntimeError when the stack
  * is empty.
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
};
}
