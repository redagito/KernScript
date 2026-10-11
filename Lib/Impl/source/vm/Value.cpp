#include "Value.h"

#include "kern/RuntimeError.h"

#include <cmath>
#include <cstring>
#include <sstream>
#include <cassert>
#include <limits>
#include <utility>

namespace
{

/**
* \brief Range-checks a widened result before narrowing back to int32.
*
* Signed overflow is undefined, so same-type integer arithmetic widens to
* 64 bit first and only narrows when the value fits.
*/
void checkIntRange(int64_t value)
{
  if (value < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
      value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
  {
    throw kern::RuntimeError("Signed integer overflow.");
  }
}

/**
* \brief Range-checks a double before narrowing to int32.
*
* Out-of-range float-to-int conversion is undefined, so the check runs on the
* widened value where every int32 boundary is exactly representable.
*/
void checkDoubleToIntRange(double value)
{
  if (!std::isfinite(value) ||
      value < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
      value > static_cast<double>(std::numeric_limits<int32_t>::max()))
  {
    throw kern::RuntimeError("Cannot convert out-of-range float to integer.");
  }
}

/**
* \brief Range-checks a double before narrowing to uint32.
*/
void checkDoubleToUintRange(double value)
{
  if (!std::isfinite(value) || value < 0.0 ||
      value >
          static_cast<double>(std::numeric_limits<uint32_t>::max()))
  {
    throw kern::RuntimeError(
        "Cannot convert out-of-range float to unsigned integer.");
  }
}

}

Value::Value()
{
  // Empty
}

Value::Value(const Value &value) { (*this) = value; }

Value::Value(Value &&value) { (*this) = std::move(value); }

Value::Value(int32_t value)
{
  m_type = Type::Integer;
  m_integer = value;
}

Value::Value(uint32_t value)
{
  m_type = Type::UnsignedInteger;
  m_unsignedInteger = value;
}

Value::Value(float value)
{
  m_type = Type::Float;
  m_float = value;
}

Value::Value(const std::string &value)
{
  m_type = Type::String;
  m_string = new char[value.length() + 1];
  memcpy(m_string, value.data(), value.length());
  m_string[value.length()] = '\0';
}

Value::~Value()
{
  if (m_type == Type::String && m_string != nullptr)
  {
    delete[] m_string;
  }
}

bool Value::convert(int32_t &value) const
{
  switch (m_type)
  {
  case Type::Integer:
    value = m_integer;
    return true;
  case Type::UnsignedInteger:
    value = (int32_t)m_unsignedInteger;
    return true;
  case Type::Float:
  {
    checkDoubleToIntRange(static_cast<double>(m_float));
    value = static_cast<int32_t>(m_float);
    return true;
  }
  case Type::String:
    throw kern::RuntimeError("Cannot convert string to integer.");
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool Value::convert(uint32_t &value) const
{
  switch (m_type)
  {
  case Type::Integer:
    value = (uint32_t)m_integer;
    return true;
  case Type::UnsignedInteger:
    value = m_unsignedInteger;
    return true;
  case Type::Float:
  {
    checkDoubleToUintRange(static_cast<double>(m_float));
    value = static_cast<uint32_t>(m_float);
    return true;
  }
  case Type::String:
    throw kern::RuntimeError("Cannot convert string to unsigned integer.");
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool Value::convert(float &value) const
{
  switch (m_type)
  {
  case Type::Integer:
    value = (float)m_integer;
    return true;
  case Type::UnsignedInteger:
    value = (float)m_unsignedInteger;
    return true;
  case Type::Float:
    value = m_float;
    return true;
  case Type::String:
    throw kern::RuntimeError("Cannot convert string to float.");
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool Value::convert(std::string &value) const
{
  // TODO Slow!!
  std::stringstream ss;
  switch (m_type)
  {
  case Type::Integer:
    ss << m_integer;
    value = ss.str();
    return true;
  case Type::UnsignedInteger:
    ss << m_unsignedInteger;
    value = ss.str();
    return true;
  case Type::Float:
    ss << m_float;
    value = ss.str();
    return true;
  case Type::String:
    value = std::string(m_string);
    return true;
  default:
    // Unknown or invalid conversion
    return false;
  }
}

Value::Type Value::getType() const { return m_type; }

std::string Value::toString() const
{
  std::stringstream ss;
  switch (m_type)
  {
  case Value::Type::Float:
    ss << "Float";
    break;

  case Value::Type::Integer:
    ss << "Integer";
    break;

  case Value::Type::Invalid:
    return "Invalid";
    break;

  case Value::Type::String:
    ss << "String";
    break;

  case Value::Type::UnsignedInteger:
    ss << "UnsignedInteger";
    break;
  }
  std::string val;
  assert(convert(val));
  ss << " : " << val;
  return ss.str();
}

Value &Value::operator=(const Value &rhs)
{
  // Same object?
  if (this == &rhs)
  {
    return *this;
  }

  // Clean up internal string memory
  if (m_type == Type::String)
  {
    delete[] m_string;
    m_string = nullptr;
  }

  m_type = rhs.m_type;
  switch (m_type)
  {
  case Type::Integer:
    m_integer = rhs.m_integer;
    break;
  case Type::UnsignedInteger:
    m_unsignedInteger = rhs.m_unsignedInteger;
    break;
  case Type::Float:
    m_float = rhs.m_float;
    break;
  case Type::String:
  {
    size_t length = strlen(rhs.m_string) + 1;
    m_string = new char[length];
    memcpy(m_string, rhs.m_string, length);
  }
  break;
  default:
    // Invalid
    break;
  }
  return *this;
}

Value &Value::operator=(Value &&rhs)
{
  if (this == &rhs)
  {
    // Same object, return
    return *this;
  }

  if (m_type == Type::String)
  {
    delete[] m_string;
    m_string = nullptr;
  }

  m_type = rhs.m_type;
  switch (m_type)
  {
  case Type::Integer:
    m_integer = rhs.m_integer;
    rhs.m_integer = 0;
    break;
  case Type::UnsignedInteger:
    m_unsignedInteger = rhs.m_unsignedInteger;
    rhs.m_unsignedInteger = 0;
    break;
  case Type::Float:
    m_float = rhs.m_float;
    rhs.m_float = 0.f;
    break;
  case Type::String:
  {
    m_string = rhs.m_string;
    rhs.m_string = nullptr;
  }
  break;
  default:
    // Invalid
    break;
  }
  rhs.m_type = Type::Invalid;

  return *this;
}

bool Value::isNumeric() const
{
  return m_type == Type::Integer || m_type == Type::UnsignedInteger ||
         m_type == Type::Float;
}

bool Value::isZero() const
{
  switch (m_type)
  {
  case Type::Integer:
    return m_integer == 0;
  case Type::UnsignedInteger:
    return m_unsignedInteger == 0;
  case Type::Float:
    return m_float == 0.f;
  default:
    return false;
  }
}

double Value::asDouble(const Value &value)
{
  switch (value.m_type)
  {
  case Type::Integer:
    return static_cast<double>(value.m_integer);
  case Type::UnsignedInteger:
    return static_cast<double>(value.m_unsignedInteger);
  case Type::Float:
    return static_cast<double>(value.m_float);
  default:
    return 0.0;
  }
}

bool Value::asBool(const Value &value)
{
  switch (value.m_type)
  {
  case Type::Integer:
    return value.m_integer != 0;
  case Type::UnsignedInteger:
    return value.m_unsignedInteger != 0;
  case Type::Float:
    return value.m_float != 0.f;
  default:
    return false;
  }
}

void Value::store(Type type, double value)
{
  m_type = type;
  switch (type)
  {
  case Type::Integer:
    // Narrowing an out-of-range double to int32 is undefined, so check first.
    checkDoubleToIntRange(value);
    m_integer = static_cast<int32_t>(value);
    break;
  case Type::UnsignedInteger:
    checkDoubleToUintRange(value);
    m_unsignedInteger = static_cast<uint32_t>(value);
    break;
  case Type::Float:
    m_float = static_cast<float>(value);
    break;
  default:
    // Not a numeric result type, the caller checked already.
    break;
  }
}

namespace
{

/**
* \brief Widens two numeric types to their common type.
*
* Float wins over the integers, matching how the conversions widen. Two
* unsigned integers stay unsigned, everything else becomes signed.
*/
Value::Type widen(Value::Type lhs, Value::Type rhs)
{
  if (lhs == Value::Type::Float || rhs == Value::Type::Float)
  {
    return Value::Type::Float;
  }
  if (lhs == Value::Type::UnsignedInteger &&
      rhs == Value::Type::UnsignedInteger)
  {
    return Value::Type::UnsignedInteger;
  }
  return Value::Type::Integer;
}
}

Value &Value::operator+=(const Value &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const Type result = widen(m_type, rhs.m_type);

    // Same-type operands stay exact by widening to 64 bit first. Mixed types
    // go through double, which narrows for integers beyond 2^53.
    if (m_type == rhs.m_type && m_type == Type::Integer)
    {
      const int64_t result = static_cast<int64_t>(m_integer) +
                             static_cast<int64_t>(rhs.m_integer);
      checkIntRange(result);
      m_integer = static_cast<int32_t>(result);
    }
    else if (m_type == rhs.m_type && m_type == Type::UnsignedInteger)
    {
      m_unsignedInteger += rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == Type::Float)
    {
      m_float += rhs.m_float;
    }
    else
    {
      store(result, asDouble(*this) + asDouble(rhs));
    }
  }
  return *this;
}

Value &Value::operator-=(const Value &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const Type result = widen(m_type, rhs.m_type);

    if (m_type == rhs.m_type && m_type == Type::Integer)
    {
      const int64_t result = static_cast<int64_t>(m_integer) -
                             static_cast<int64_t>(rhs.m_integer);
      checkIntRange(result);
      m_integer = static_cast<int32_t>(result);
    }
    else if (m_type == rhs.m_type && m_type == Type::UnsignedInteger)
    {
      m_unsignedInteger -= rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == Type::Float)
    {
      m_float -= rhs.m_float;
    }
    else
    {
      store(result, asDouble(*this) - asDouble(rhs));
    }
  }
  return *this;
}

Value &Value::operator*=(const Value &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const Type result = widen(m_type, rhs.m_type);

    if (m_type == rhs.m_type && m_type == Type::Integer)
    {
      const int64_t result = static_cast<int64_t>(m_integer) *
                             static_cast<int64_t>(rhs.m_integer);
      checkIntRange(result);
      m_integer = static_cast<int32_t>(result);
    }
    else if (m_type == rhs.m_type && m_type == Type::UnsignedInteger)
    {
      m_unsignedInteger *= rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == Type::Float)
    {
      m_float *= rhs.m_float;
    }
    else
    {
      store(result, asDouble(*this) * asDouble(rhs));
    }
  }
  return *this;
}

Value &Value::operator/=(const Value &rhs)
{
  // Division by zero leaves the operand unchanged, matching how the other
  // operators ignore non numeric operands. The caller checks for it.
  if (!isNumeric() || !rhs.isNumeric())
  {
    return *this;
  }

  if (m_type == rhs.m_type && m_type == Type::Integer)
  {
    if (rhs.m_integer == 0)
    {
      return *this;
    }
    // INT_MIN / -1 overflows int32 and traps on x86, so reject it here.
    if (m_integer == std::numeric_limits<int32_t>::min() &&
        rhs.m_integer == -1)
    {
      throw kern::RuntimeError("Signed integer overflow.");
    }
    m_integer /= rhs.m_integer;
    return *this;
  }
  if (m_type == rhs.m_type && m_type == Type::UnsignedInteger)
  {
    if (rhs.m_unsignedInteger == 0)
    {
      return *this;
    }
    m_unsignedInteger /= rhs.m_unsignedInteger;
    return *this;
  }
  if (m_type == rhs.m_type && m_type == Type::Float)
  {
    m_float /= rhs.m_float;
    return *this;
  }

  const Type result = widen(m_type, rhs.m_type);
  const double divisor = asDouble(rhs);
  if (divisor == 0.0)
  {
    return *this;
  }
  store(result, asDouble(*this) / divisor);
  return *this;
}

bool Value::operator==(const Value &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    // Only equal when the same value was built the same way.
    if (m_type != rhs.m_type)
    {
      return false;
    }
    return m_type == Type::String ? strcmp(m_string, rhs.m_string) == 0
                                   : false;
  }

  if (m_type == rhs.m_type)
  {
    switch (m_type)
    {
    case Type::Integer:
      return m_integer == rhs.m_integer;
    case Type::UnsignedInteger:
      return m_unsignedInteger == rhs.m_unsignedInteger;
    case Type::Float:
      return m_float == rhs.m_float;
    default:
      break;
    }
  }
  return asDouble(*this) == asDouble(rhs);
}

bool Value::operator!=(const Value &rhs) const { return !((*this) == rhs); }

int Value::compare(const Value &lhs, const Value &rhs)
{
  if (lhs.m_type == rhs.m_type)
  {
    switch (lhs.m_type)
    {
    case Type::Integer:
      return lhs.m_integer < rhs.m_integer
                 ? -1
                 : (lhs.m_integer > rhs.m_integer ? 1 : 0);
    case Type::UnsignedInteger:
      return lhs.m_unsignedInteger < rhs.m_unsignedInteger
                 ? -1
                 : (lhs.m_unsignedInteger > rhs.m_unsignedInteger ? 1 : 0);
    case Type::Float:
      return lhs.m_float < rhs.m_float ? -1
                                       : (lhs.m_float > rhs.m_float ? 1 : 0);
    default:
      break;
    }
  }
  const double a = asDouble(lhs);
  const double b = asDouble(rhs);
  return a < b ? -1 : (a > b ? 1 : 0);
}

bool Value::operator<=(const Value &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) <= 0;
}

bool Value::operator<(const Value &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) < 0;
}

bool Value::operator>=(const Value &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) >= 0;
}

bool Value::operator>(const Value &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) > 0;
}
