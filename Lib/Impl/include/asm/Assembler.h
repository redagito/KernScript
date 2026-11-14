#pragma once
#include "kern/IAssembler.h"
#include "kern/ILogger.h"
class Assembler : public kern::IAssembler
{
public:
    explicit Assembler(kern::ILogger *logger);
    bool assemble(std::istream &, std::ostream &) override;

private:
    kern::ILogger *m_logger;
};
