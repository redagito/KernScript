#pragma once
#include <cstdint>
namespace kern
{
enum class ReturnKind : uint8_t
{
    Void = 0,
    Value = 1
};
struct FunctionSignature
{
    uint32_t argumentCount = 0;
    ReturnKind returnKind = ReturnKind::Void;
    bool operator==(const FunctionSignature &) const = default;
};
inline uint32_t returnCount(FunctionSignature signature) { return signature.returnKind == ReturnKind::Value ? 1u : 0u; }
} // namespace kern
