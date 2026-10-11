#pragma once

#include <cstdint>
#include <string>

/**
* \brief Stores a value of any of the supported types.
*/
class Value
{
public:
  enum class Type
  {
    Integer,
    UnsignedInteger,
    Float,
    String,

    Invalid
  };

  Value();
  Value(const Value &value);
  Value(Value &&value);
  Value(int32_t value);
  Value(uint32_t value);
  Value(float value);
  Value(const std::string &value);

  ~Value();

  bool convert(int32_t &value) const;
  bool convert(uint32_t &value) const;
  bool convert(float &value) const;
  bool convert(std::string &value) const;

  Type getType() const;

  std::string toString() const;

  /**
  * \brief Returns true for the types arithmetic and comparison operate on.
  *
  * Strings and invalid values are not numeric. Arithmetic with a non-numeric
  * operand leaves the value unchanged, so callers that must not silently
  * drop an operand check this first. Numeric failures throw
  * kern::RuntimeError instead: integer overflow and out-of-range conversions
  * never wrap or narrow silently.
  */
  bool isNumeric() const;

  /**
  * \brief Returns true for a numeric value that equals zero.
  *
  * Non numeric values report false, they never reach a divisor check.
  */
  bool isZero() const;

  Value &operator=(const Value &rhs);
  Value &operator=(Value &&rhs);

  Value &operator+=(const Value &rhs);
  Value &operator-=(const Value &rhs);
  Value &operator*=(const Value &rhs);
  Value &operator/=(const Value &rhs);

  bool operator==(const Value &rhs) const;
  bool operator!=(const Value &rhs) const;

  bool operator<=(const Value &rhs) const;

  bool operator<(const Value &rhs) const;

  bool operator>=(const Value &rhs) const;

  bool operator>(const Value &rhs) const;

private:
  /**
  * \brief Converts to a double, used when the operand types differ.
  */
  static double asDouble(const Value &value);

  /**
  * \brief Compares two numeric values.
  * \return Negative, zero or positive as lhs is less, equal or greater.
  */
  static int compare(const Value &lhs, const Value &rhs);

  /**
  * \brief Stores a computed value of the given type.
  */
  void store(Type type, double value);

  /**
  * \brief Converts to a bool, used by the logical operators.
  */
  static bool asBool(const Value &value);

  union
  {
    int32_t m_integer;
    uint32_t m_unsignedInteger;
    float m_float;
    char *m_string = nullptr;
  };
  Type m_type = Type::Invalid;
};
