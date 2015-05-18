#include "CValue.h"

#include <cstring>
#include <sstream>

CValue::CValue()
{
	// Empty
}

CValue::CValue(const CValue& value)
{
	m_type = value.m_type;
	switch (m_type)
	{
	case EType::Integer:
		m_integer = value.m_integer;
		break;
	case EType::Float:
		m_float = value.m_float;
		break;
	case EType::String:
		{
			size_t length = strlen(value.m_string) + 1;
			m_string = new char[length];
			memcpy(m_string, value.m_string, length);
			break;
		}	
	default:
		// Invalid
		break;
	}
}

CValue::CValue(int32_t value)
{
	m_type = EType::Integer;
	m_integer = value;
}

CValue::CValue(float value)
{
	m_type = EType::Float;
	m_float = value;	
}

CValue::CValue(const std::string& value)
{
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
	case EType::Float:
		value = (int32_t) m_float;
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
		value = (float) m_integer;
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