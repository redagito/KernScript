#pragma once

#include <stdexcept>
#include <string>

namespace kern
{

/**
 * \brief Error during script execution.
 *
 * Thrown by the virtual machine when a script cannot continue, for example on
 * stack underflow, a type mismatch or division by zero. Normal termination
 * through ret or exit does not throw.
 */
class RuntimeError : public std::runtime_error
{
public:
    explicit RuntimeError(const std::string &message);
};
} // namespace kern
