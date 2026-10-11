#pragma once

#include <string>

#include <fmt/format.h>

namespace kern
{

/**
* \brief Interface for receiving diagnostics from the library.
*
* The library does not write to the standard streams itself. Diagnostics are
* passed to a logger instead, which the host application supplies and owns, so
* that where they end up stays under control of whoever embeds the library.
*
* Messages are formatted with fmt, so the format string has to be a compile
* time literal, as with spdlog:
*
*     logger->error("Unknown variable: {}", name);
*
* The logger is owned by the caller and has to outlive every object it is
* passed to. Passing a null pointer selects the null logger, which discards
* everything.
*/
class ILogger
{
public:
  virtual ~ILogger() = default;

  /**
  * \brief Logs a formatted error message.
  */
  template <typename... Args>
  void error(fmt::format_string<Args...> format, Args &&...args)
  {
    onError(fmt::format(format, std::forward<Args>(args)...));
  }

protected:
  /**
  * \brief Receives an already formatted error message.
  *
  * Implement this instead of error(). The formatting entry point is not
  * virtual so that every implementation goes through fmt.
  *
  * One pure virtual per level. A new level adds another one, which is
  * intentionally a breaking change: it forces every implementation to decide
  * what to do with the new level instead of silently routing it into an
  * existing one.
  */
  virtual void onError(const std::string &message) = 0;
};
}
