#pragma once

#include "kern/ILogger.h"

namespace kern {

/**
 * \brief Logger that discards everything given to it.
 */
class NullLogger : public ILogger {
protected:
  void onError(const std::string &message) override;
};

} // namespace kern
