#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "IExternCall.h"
#include "IExternFunction.h"
#include "RuntimeError.h"

namespace kern
{

/** Wraps a C++ function, preserving source argument order and return type. */
template <typename ReturnType, typename... ArgumentTypes> class ExternFunction : public IExternFunction
{
    template <class T>
    static constexpr bool supported =
        std::is_same_v<std::remove_cvref_t<T>, int> || std::is_same_v<std::remove_cvref_t<T>, unsigned int> ||
        std::is_same_v<std::remove_cvref_t<T>, float> || std::is_same_v<std::remove_cvref_t<T>, double> ||
        std::is_same_v<std::remove_cvref_t<T>, bool> || std::is_same_v<std::remove_cvref_t<T>, std::string>;
    static_assert((supported<ArgumentTypes> && ...), "Unsupported host argument type.");
    static_assert(std::is_void_v<ReturnType> || supported<ReturnType>, "Unsupported host return type.");
    using Arguments = std::tuple<std::remove_cvref_t<ArgumentTypes>...>;

public:
    explicit ExternFunction(ReturnType (*function)(ArgumentTypes...)) : m_function(function)
    {
        if (!function)
            throw RuntimeError("Host function pointer must not be null.");
    }

    FunctionSignature getSignature() const override
    {
        return {sizeof...(ArgumentTypes), std::is_void_v<ReturnType> ? ReturnKind::Void : ReturnKind::Value};
    }

    bool call(IExternCall &call) override
    {
        Arguments arguments{};
        if (!readArguments<0>(call, arguments))
        {
            return false;
        }
        auto invoke = [this](auto &...values) -> decltype(auto)
        { return m_function(std::forward<ArgumentTypes>(values)...); };
        if constexpr (std::is_void_v<ReturnType>)
        {
            std::apply(invoke, arguments);
        }
        else
        {
            call.setReturnValue(std::apply(invoke, arguments));
        }
        return true;
    }

private:
    template <size_t Index> static bool readArguments(IExternCall &call, Arguments &arguments)
    {
        if constexpr (Index == sizeof...(ArgumentTypes))
        {
            return true;
        }
        else
        {
            // Indexed reads never mutate the VM operand stack.
            return call.getArgument(Index, std::get<Index>(arguments)) && readArguments<Index + 1>(call, arguments);
        }
    }

    ReturnType (*m_function)(ArgumentTypes...);
};

template <typename ReturnType, typename... ArgumentTypes>
IExternFunction *createExternFunction(ReturnType (*function)(ArgumentTypes...))
{
    return new ExternFunction<ReturnType, ArgumentTypes...>(function);
}

} // namespace kern
