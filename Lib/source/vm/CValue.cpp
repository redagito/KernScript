#include "CValue.h"

#include <cstring>
#include <sstream>
#include <cassert>

CValue::CValue()
{
  // Empty
}

CValue::CValue(const CValue &value) { (*this) = value; }

CValue::CValue(CValue &&value) { (*this) = value; }

CValue::CValue(int32_t value)
{
  m_type = EType::Integer;
  m_integer = value;
}

CValue::CValue(uint32_t value)
{
  m_type = EType::UnsignedInteger;
  m_unsignedInteger = value;
}

CValue::CValue(float value)
{
  m_type = EType::Float;
  m_float = value;
}

CValue::CValue(const std::string &value)
{
  m_type = EType::String;
  m_string = new char[value.length() + 1];
  memcpy(m_string, value.data(), value.length());
  m_string[value.length()] = '\0';
}

CValue::~CValue()
{
  if (m_type == EType::String && m_string != nullptr)
  {
    delete[] m_string;
  }
}

bool CValue::convert(int32_t &value) const
{
  switch (m_type)
  {
  case EType::Integer:
    value = m_integer;
    return true;
  case EType::UnsignedInteger:
    value = (int32_t)m_unsignedInteger;
    return true;
  case EType::Float:
    value = (int32_t)m_float;
    return true;
  case EType::String:
    // String conversion TODO
    return false;
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool CValue::convert(uint32_t &value) const
{
  switch (m_type)
  {
  case EType::Integer:
    value = (uint32_t)m_integer;
    return true;
  case EType::UnsignedInteger:
    value = m_unsignedInteger;
    return true;
  case EType::Float:
    value = (uint32_t)m_float;
    return true;
  case EType::String:
    // String conversion TODO
    return false;
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool CValue::convert(float &value) const
{
  switch (m_type)
  {
  case EType::Integer:
    value = (float)m_integer;
    return true;
  case EType::UnsignedInteger:
    value = (float)m_unsignedInteger;
    return true;
  case EType::Float:
    value = m_float;
    return true;
  case EType::String:
    // String conversion TODO
    return false;
  default:
    // Unknown or invalid conversion
    return false;
  }
}

bool CValue::convert(std::string &value) const
{
  // TODO Slow!!
  std::stringstream ss;
  switch (m_type)
  {
  case EType::Integer:
    ss << m_integer;
    value = ss.str();
    return true;
  case EType::UnsignedInteger:
    ss << m_unsignedInteger;
    value = ss.str();
    return true;
  case EType::Float:
    ss << m_float;
    value = ss.str();
    return true;
  case EType::String:
    value = std::string(m_string);
    return true;
  default:
    // Unknown or invalid conversion
    return false;
  }
}

CValue::EType CValue::getType() const { return m_type; }

std::string CValue::toString() const
{
  std::stringstream ss;
  switch (m_type)
  {
  case CValue::EType::Float:
    ss << "Float";
    break;

  case CValue::EType::Integer:
    ss << "Integer";
    break;

  case CValue::EType::Invalid:
    return "Invalid";
    break;

  case CValue::EType::String:
    ss << "String";
    break;

  case CValue::EType::UnsignedInteger:
    ss << "UnsignedInteger";
    break;
  }
  std::string val;
  assert(convert(val));
  ss << " : " << val;
  return ss.str();
}

CValue &CValue::operator=(const CValue &rhs)
{
  // Same object?
  if (this == &rhs)
  {
    return *this;
  }

  // Clean up internal string memory
  if (m_type == EType::String)
  {
    delete[] m_string;
    m_string = nullptr;
  }

  m_type = rhs.m_type;
  switch (m_type)
  {
  case EType::Integer:
    m_integer = rhs.m_integer;
    break;
  case EType::UnsignedInteger:
    m_unsignedInteger = rhs.m_unsignedInteger;
    break;
  case EType::Float:
    m_float = rhs.m_float;
    break;
  case EType::String:
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

CValue &CValue::operator=(CValue &&rhs)
{
  if (this == &rhs)
  {
    // Same object, return
    return *this;
  }

  if (m_type == EType::String)
  {
    delete[] m_string;
    m_string = nullptr;
  }

  m_type = rhs.m_type;
  switch (m_type)
  {
  case EType::Integer:
    m_integer = rhs.m_integer;
    rhs.m_integer = 0;
    break;
  case EType::UnsignedInteger:
    m_unsignedInteger = rhs.m_unsignedInteger;
    rhs.m_unsignedInteger = 0;
    break;
  case EType::Float:
    m_float = rhs.m_float;
    rhs.m_float = 0.f;
    break;
  case EType::String:
  {
    m_string = rhs.m_string;
    rhs.m_string = nullptr;
  }
  break;
  default:
    // Invalid
    break;
  }
  rhs.m_type = EType::Invalid;

  return *this;
}

bool CValue::isNumeric() const
{
  return m_type == EType::Integer || m_type == EType::UnsignedInteger ||
         m_type == EType::Float;
}

bool CValue::isZero() const
{
  switch (m_type)
  {
  case EType::Integer:
    return m_integer == 0;
  case EType::UnsignedInteger:
    return m_unsignedInteger == 0;
  case EType::Float:
    return m_float == 0.f;
  default:
    return false;
  }
}

double CValue::asDouble(const CValue &value)
{
  switch (value.m_type)
  {
  case EType::Integer:
    return static_cast<double>(value.m_integer);
  case EType::UnsignedInteger:
    return static_cast<double>(value.m_unsignedInteger);
  case EType::Float:
    return static_cast<double>(value.m_float);
  default:
    return 0.0;
  }
}

bool CValue::asBool(const CValue &value)
{
  switch (value.m_type)
  {
  case EType::Integer:
    return value.m_integer != 0;
  case EType::UnsignedInteger:
    return value.m_unsignedInteger != 0;
  case EType::Float:
    return value.m_float != 0.f;
  default:
    return false;
  }
}

void CValue::store(EType type, double value)
{
  m_type = type;
  switch (type)
  {
  case EType::Integer:
    m_integer = static_cast<int32_t>(value);
    break;
  case EType::UnsignedInteger:
    m_unsignedInteger = static_cast<uint32_t>(value);
    break;
  case EType::Float:
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
CValue::EType widen(CValue::EType lhs, CValue::EType rhs)
{
  if (lhs == CValue::EType::Float || rhs == CValue::EType::Float)
  {
    return CValue::EType::Float;
  }
  if (lhs == CValue::EType::UnsignedInteger &&
      rhs == CValue::EType::UnsignedInteger)
  {
    return CValue::EType::UnsignedInteger;
  }
  return CValue::EType::Integer;
}
}

CValue &CValue::operator+=(const CValue &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const EType result = widen(m_type, rhs.m_type);

    // Same-type operands stay exact. Mixed types go through double, which
    // narrows for integers beyond 2^53.
    if (m_type == rhs.m_type && m_type == EType::Integer)
    {
      m_integer = static_cast<int32_t>(m_integer + rhs.m_integer);
    }
    else if (m_type == rhs.m_type && m_type == EType::UnsignedInteger)
    {
      m_unsignedInteger += rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == EType::Float)
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

CValue &CValue::operator-=(const CValue &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const EType result = widen(m_type, rhs.m_type);

    if (m_type == rhs.m_type && m_type == EType::Integer)
    {
      m_integer = static_cast<int32_t>(m_integer - rhs.m_integer);
    }
    else if (m_type == rhs.m_type && m_type == EType::UnsignedInteger)
    {
      m_unsignedInteger -= rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == EType::Float)
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

CValue &CValue::operator*=(const CValue &rhs)
{
  if (isNumeric() && rhs.isNumeric())
  {
    const EType result = widen(m_type, rhs.m_type);

    if (m_type == rhs.m_type && m_type == EType::Integer)
    {
      m_integer = static_cast<int32_t>(m_integer * rhs.m_integer);
    }
    else if (m_type == rhs.m_type && m_type == EType::UnsignedInteger)
    {
      m_unsignedInteger *= rhs.m_unsignedInteger;
    }
    else if (m_type == rhs.m_type && m_type == EType::Float)
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

CValue &CValue::operator/=(const CValue &rhs)
{
  // Division by zero leaves the operand unchanged, matching how the other
  // operators ignore non numeric operands. The caller checks for it.
  if (!isNumeric() || !rhs.isNumeric())
  {
    return *this;
  }

  if (m_type == rhs.m_type && m_type == EType::Integer)
  {
    if (rhs.m_integer == 0)
    {
      return *this;
    }
    m_integer /= rhs.m_integer;
    return *this;
  }
  if (m_type == rhs.m_type && m_type == EType::UnsignedInteger)
  {
    if (rhs.m_unsignedInteger == 0)
    {
      return *this;
    }
    m_unsignedInteger /= rhs.m_unsignedInteger;
    return *this;
  }
  if (m_type == rhs.m_type && m_type == EType::Float)
  {
    m_float /= rhs.m_float;
    return *this;
  }

  const EType result = widen(m_type, rhs.m_type);
  const double divisor = asDouble(rhs);
  if (divisor == 0.0)
  {
    return *this;
  }
  store(result, asDouble(*this) / divisor);
  return *this;
}

bool CValue::operator==(const CValue &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    // Only equal when the same value was built the same way.
    if (m_type != rhs.m_type)
    {
      return false;
    }
    return m_type == EType::String ? strcmp(m_string, rhs.m_string) == 0
                                   : false;
  }

  if (m_type == rhs.m_type)
  {
    switch (m_type)
    {
    case EType::Integer:
      return m_integer == rhs.m_integer;
    case EType::UnsignedInteger:
      return m_unsignedInteger == rhs.m_unsignedInteger;
    case EType::Float:
      return m_float == rhs.m_float;
    default:
      break;
    }
  }
  return asDouble(*this) == asDouble(rhs);
}

bool CValue::operator!=(const CValue &rhs) const { return !((*this) == rhs); }

int CValue::compare(const CValue &lhs, const CValue &rhs)
{
  if (lhs.m_type == rhs.m_type)
  {
    switch (lhs.m_type)
    {
    case EType::Integer:
      return lhs.m_integer < rhs.m_integer
                 ? -1
                 : (lhs.m_integer > rhs.m_integer ? 1 : 0);
    case EType::UnsignedInteger:
      return lhs.m_unsignedInteger < rhs.m_unsignedInteger
                 ? -1
                 : (lhs.m_unsignedInteger > rhs.m_unsignedInteger ? 1 : 0);
    case EType::Float:
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

bool CValue::operator<=(const CValue &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) <= 0;
}

bool CValue::operator<(const CValue &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) < 0;
}

bool CValue::operator>=(const CValue &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) >= 0;
}

bool CValue::operator>(const CValue &rhs) const
{
  if (!isNumeric() || !rhs.isNumeric())
  {
    return false;
  }
  return compare(*this, rhs) > 0;
}
