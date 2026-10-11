#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler {
public:
  /**
   * \brief Constructs a compiler.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: explicitly unimplemented stub. compile() always returns false;
   * there is no scripting-language frontend yet (see Parser.h). The class
   * is kept because the public API references it: kern::createCompiler()
   * (Lib/Api/source/Kern.cpp) constructs it, and kern/ICompiler.h plus
   * kern/Kern.h declare the interface. Removing it would break the
   * product library build and desync the implementation from the public
   * headers. Implementing a compiler is out of scope here.
   */
  explicit Compiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};