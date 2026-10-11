#include "kern/Kern.h"

#include "asm/Assembler.h"
#include "comp/Compiler.h"
#include "vm/VirtualMachine.h"
#include "common/Version.h"
#include "logging/Logger.h"

namespace
{

/**
* \brief Returns the shared logger used when no logger is supplied.
*
* Logger holds no state, so one instance is enough. It is a function local
* static to avoid depending on static initialization order.
*/
Logger &defaultLogger()
{
  static Logger logger;
  return logger;
}
}

kern::IAssembler *kern::createAssembler()
{
  return createAssembler(&defaultLogger());
}

kern::IAssembler *kern::createAssembler(kern::ILogger *logger)
{
  return new Assembler(logger);
}

kern::ICompiler *kern::createCompiler()
{
  return createCompiler(&defaultLogger());
}

kern::ICompiler *kern::createCompiler(kern::ILogger *logger)
{
  return new Compiler(logger);
}

kern::IVirtualMachine *kern::createVirtualMachine()
{
  return createVirtualMachine(&defaultLogger());
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::ILogger *logger)
{
  return new VirtualMachine(logger);
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::ILogger *logger,
                                                  kern::VMConfig config)
{
  return new VirtualMachine(logger, config);
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::VMConfig config)
{
  return createVirtualMachine(&defaultLogger(), config);
}

unsigned int kern::getMajorVersion() { return majorVersion; }

unsigned int kern::getMinorVersion() { return minorVersion; }
