#pragma once

#include <cstdint>
#include <string>

/**
* \brief Stores a value of any of the supported types.
*/
class CValue
{
public:
  enum class EType
  {
    Integer,
    UnsignedInteger,
    Float,
    String,

    Invalid
  };

  CValue();
  CValue(const CValue &value);
  CValue(CValue &&value);
  CValue(int32_t value);
  CValue(uint32_t value);
  CValue(float value);
  CValue(const std::string &value);

  ~CValue();

  bool convert(int32_t &value) const;
  bool convert(uint32_t &value) const;
  bool convert(float &value) const;
  bool convert(std::string &value) const;

  EType getType() const;

  std::string toString() const;

  /**
  * \brief Returns true for the types arithmetic and comparison operate on.
  *
  * Strings and invalid values are not numeric. Arithmetic on them leaves the
  * value unchanged, so callers that must not silently drop an operand check
  * this first.
  */
  bool isNumeric() const;

  CValue &operator=(const CValue &rhs);
  CValue &operator=(CValue &&rhs);

  CValue &operator+=(const CValue &rhs);
  CValue &operator-=(const CValue &rhs);
  CValue &operator*=(const CValue &rhs);
  CValue &operator/=(const CValue &rhs);

  bool operator==(const CValue &rhs) const;
  bool operator!=(const CValue &rhs) const;

  bool operator<=(const CValue &rhs) const;

  bool operator<(const CValue &rhs) const;

  bool operator>=(const CValue &rhs) const;

  bool operator>(const CValue &rhs) const;

private:
  /**
  * \brief Converts to a double, used when the operand types differ.
  */
  static double asDouble(const CValue &value);

  /**
  * \brief Compares two numeric values.
  * \return Negative, zero or positive as lhs is less, equal or greater.
  */
  static int compare(const CValue &lhs, const CValue &rhs);

  /**
  * \brief Stores a computed value of the given type.
  */
  void store(EType type, double value);

  /**
  * \brief Converts to a bool, used by the logical operators.
  */
  static bool asBool(const CValue &value);

  union
  {
    int32_t m_integer;
    uint32_t m_unsignedInteger;
    float m_float;
    char *m_string = nullptr;
  };
  EType m_type = EType::Invalid;
};
