#include "kern/Kern.h"

#include "asm/CAssembler.h"
#include "comp/CCompiler.h"
#include "vm/CVirtualMachine.h"
#include "common/Version.h"
#include "logging/CLogger.h"

namespace
{

/**
* \brief Returns the shared logger used when no logger is supplied.
*
* CLogger holds no state, so one instance is enough. It is a function local
* static to avoid depending on static initialization order.
*/
CLogger &defaultLogger()
{
  static CLogger logger;
  return logger;
}
}

kern::IAssembler *kern::createAssembler()
{
  return createAssembler(&defaultLogger());
}

kern::IAssembler *kern::createAssembler(kern::ILogger *logger)
{
  return new CAssembler(logger);
}

kern::ICompiler *kern::createCompiler()
{
  return createCompiler(&defaultLogger());
}

kern::ICompiler *kern::createCompiler(kern::ILogger *logger)
{
  return new CCompiler(logger);
}

kern::IVirtualMachine *kern::createVirtualMachine()
{
  return createVirtualMachine(&defaultLogger());
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::ILogger *logger)
{
  return new CVirtualMachine(logger);
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::ILogger *logger,
                                                  kern::VMConfig config)
{
  return new CVirtualMachine(logger, config);
}

kern::IVirtualMachine *kern::createVirtualMachine(kern::VMConfig config)
{
  return createVirtualMachine(&defaultLogger(), config);
}

unsigned int kern::getMajorVersion() { return majorVersion; }

unsigned int kern::getMinorVersion() { return minorVersion; }
