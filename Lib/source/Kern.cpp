#include "kern/Kern.h"

#include "asm/CAssembler.h"
#include "comp/CCompiler.h"
#include "vm/CVirtualMachine.h"
#include "common/Version.h"

kern::IAssembler *kern::createAssembler() { return new CAssembler; }

kern::ICompiler *kern::createCompiler() { return new CCompiler; }

kern::IVirtualMachine *kern::createVirtualMachine()
{
  return new CVirtualMachine;
}

unsigned int kern::getMajorVersion() { return majorVersion; }

unsigned int kern::getMinorVersion() { return minorVersion; }
