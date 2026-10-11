#pragma once

#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>
#include <kern/ILogger.h>

namespace testutil
{

/**
* \brief Logger that records every message it receives.
*
* Tests assert on the diagnostics the library emits, which cannot be observed
* through the standard streams once the library logs through ILogger.
*
* Assert through loggedAny() and loggedContaining() rather than matching
* messages by hand. Matching exact wording couples a test to prose that can be
* reworded without any change in behaviour, and a case sensitive substring
* check on that prose is a third source of drift. The durable facts are that a
* message arrived at all, and optionally that it mentions a given subject.
*/
class RecordingLogger : public kern::ILogger
{
public:
  const std::vector<std::string> &messages() const;

  bool empty() const;

  std::string text() const;

  void clear();

  /**
  * \brief Returns true if at least one message was logged.
  */
  bool loggedAny() const;

  /**
  * \brief Returns true if any message contains the given subject.
  *
  * \param subject Text to look for, matched case insensitively. Keep it to a
  *        stable keyword such as "zero" or "numeric", never a full sentence.
  */
  bool loggedContaining(const std::string &subject) const;

protected:
  void onError(const std::string &message) override;

private:
  std::vector<std::string> m_messages;
};

/**
* \brief Sink for everything a script writes through host functions.
*
* The library itself does not write to std::cout, so everything a script
* "prints" arrives here. Tests assert on this sink instead of scraping the
* standard streams.
*/
class OutputRecorder
{
public:
  virtual ~OutputRecorder();

  virtual void print(const std::string &text) = 0;
  virtual void println(const std::string &text) = 0;
  virtual void log(const std::string &severity,
                   const std::string &message) = 0;
};

/**
* \brief Records script output into a string.
*/
class StringRecorder : public OutputRecorder
{
public:
  void print(const std::string &text) override;
  void println(const std::string &text) override;
  void log(const std::string &severity,
           const std::string &message) override;

  const std::string &text() const;

  void clear();

private:
  std::string m_text;
};

/**
* \brief Discards everything the script writes.
*
* Used by the helper overloads where only success or failure is interesting.
*/
class DiscardRecorder : public OutputRecorder
{
public:
  void print(const std::string &text) override;
  void println(const std::string &text) override;
  void log(const std::string &severity,
           const std::string &message) override;
};

/**
* \brief Registers the host functions the assembly fixtures rely on.
*
* Extern functions are owned by the virtual machine once registered. The
* recorder must outlive the virtual machine.
*/
void registerHostFunctions(kern::IVirtualMachine &vm,
                           OutputRecorder &recorder);

/**
* \brief Assembles assembly source code into a byte code string stream.
* \return True on success, false on assembly error.
*/
bool assemble(const std::string &asmSource, std::stringstream &byteCode);

/**
* \brief Assembles and executes assembly source code.
*
* \return True if the assembly, the load and the call succeeded.
*/
bool runScript(const std::string &asmSource, const std::string &functionName,
               OutputRecorder &recorder);

/**
* \brief Reads a test fixture file.
*/
std::string readFile(const std::string &path);
}
