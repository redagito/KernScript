#include "Logger.h"

#include "kern/NullLogger.h"

#include <spdlog/spdlog.h>

kern::ILogger *nullLogger()
{
  static kern::NullLogger logger;
  return &logger;
}

void Logger::onError(const std::string &message)
{
  // The message arrives already formatted, so it is passed as an argument
  // rather than as the format string.
  spdlog::error("{}", message);
}
