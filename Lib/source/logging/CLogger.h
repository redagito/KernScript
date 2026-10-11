#pragma once

#include <kern/ILogger.h>

class CLogger;

/**
* \brief Returns a shared logger that discards everything.
*
* Used to turn a null logger pointer into a usable object, so that no caller
* has to check for null.
*/
kern::ILogger *nullLogger();

/**
* \brief Logger that forwards diagnostics to the spdlog default logger.
*
* Used by the factory overloads that take no logger. The default logger is
* configured the way spdlog configures it, which can be changed through spdlog
* itself before the factory is called, or replaced entirely by passing a
* different ILogger implementation.
*
* Holds no state, so a single shared instance is used everywhere.
*/
class CLogger : public kern::ILogger
{
protected:
  void log(const std::string &message) override;
};
