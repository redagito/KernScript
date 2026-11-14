#pragma once
#include "FunctionSignature.h"
#include "IExternCall.h"
namespace kern
{
class IExternFunction
{
public:
    virtual ~IExternFunction() = default;
    virtual FunctionSignature getSignature() const = 0;
    virtual bool call(IExternCall &) = 0;
};
} // namespace kern
