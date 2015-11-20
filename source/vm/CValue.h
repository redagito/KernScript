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

  CValue &operator=(const CValue &rhs);
  CValue &operator=(CValue &&rhs);

  CValue &operator+=(const CValue &rhs);
  CValue &operator-=(const CValue &rhs);

  bool operator==(const CValue &rhs) const;
  bool operator!=(const CValue &rhs) const;

  bool operator<=(const CValue &rhs) const;

private:
  union
  {
    int32_t m_integer;
    uint32_t m_unsignedInteger;
    float m_float;
    char *m_string = nullptr;
  };
  EType m_type = EType::Invalid;
};
