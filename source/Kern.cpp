#include "kern/Kern.h"

#include "asm/CAssembler.h"
#include "comp/CCompiler.h"
#include "vm/CVirtualMachine.h"

kern::IAssembler *kern::createAssembler() { return new CAssembler; }

kern::ICompiler *kern::createCompiler() { return new CCompiler; }

kern::IVirtualMachine *kern::createVirtualMachine()
{
  return new CVirtualMachine;
}