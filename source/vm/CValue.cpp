#include "CValue.h"

#include <cstring>
#include <sstream>

CValue::CValue()
{
	// Empty
}

CValue::CValue(const CValue& value)
{
	(*this) = value;
}

CValue::CValue(CValue&& value)
{
	(*this) = value;
}

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

CValue::CValue(const std::string& value)
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

bool CValue::convert(int32_t& value) const
{
	switch (m_type)
	{
	case EType::Integer:
		value = m_integer;
		return true;
	case EType::UnsignedInteger:
		value = (int32_t) m_unsignedInteger;
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

bool CValue::convert(uint32_t& value) const
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
		value = (uint32_t) m_float;
		return true;
	case EType::String:
		// String conversion TODO
		return false;
	default:
		// Unknown or invalid conversion
		return false;
	}
}

bool CValue::convert(float& value) const
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

bool CValue::convert(std::string& value) const
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

CValue& CValue::operator=(const CValue& rhs)
{
	if (this == &rhs)
	{
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

CValue& CValue::operator=(CValue&& rhs)
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


CValue& CValue::operator+=(const CValue& rhs)
{
	// Arithmetic add
	switch (m_type)
	{
	case EType::Integer:
		switch (rhs.m_type)
		{
		case EType::Integer:
			// Integer with integer
			m_integer += rhs.m_integer;
		}
	}
	return *this;
}

CValue& CValue::operator-=(const CValue& rhs)
{
	// Arithmetic subtract
	switch (m_type)
	{
	case EType::Integer:
		switch (rhs.m_type)
		{
		case EType::Integer:
			// Integer with integer
			m_integer -= rhs.m_integer;
		}
	}
	return *this;
}