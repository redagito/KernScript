#include "kern/RuntimeError.h"

kern::RuntimeError::RuntimeError(const std::string &message) : std::runtime_error(message) {}
