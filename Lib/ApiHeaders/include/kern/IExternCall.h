#pragma once
#include <string>
namespace kern
{
/** Arguments are indexed in source order. This context is valid only during call(). */
class IExternCall
{
public:
    virtual ~IExternCall() = default;
    virtual unsigned int getArgumentCount() const = 0;
    virtual bool getArgument(unsigned int, int &) const = 0;
    virtual bool getArgument(unsigned int, unsigned int &) const = 0;
    virtual bool getArgument(unsigned int, float &) const = 0;
    virtual bool getArgument(unsigned int, double &) const = 0;
    virtual bool getArgument(unsigned int, bool &) const = 0;
    virtual bool getArgument(unsigned int, std::string &) const = 0;
    virtual void setReturnValue(int) = 0;
    virtual void setReturnValue(unsigned int) = 0;
    virtual void setReturnValue(float) = 0;
    virtual void setReturnValue(double) = 0;
    virtual void setReturnValue(bool) = 0;
    virtual void setReturnValue(const std::string &) = 0;
};
} // namespace kern
