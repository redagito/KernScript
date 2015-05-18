#pragma once

#include <cstdint>
#include <string>

class CValue
{
public:
	enum class EType
	{
		Integer,
		Float,
		String,

		Invalid
	};

	CValue();
	CValue(const CValue& value);
	CValue(int32_t value);
	CValue(float value);
	CValue(const std::string& value);

	~CValue();

	bool convert(int32_t& value) const;
	bool convert(float& value) const;
	bool convert(std::string& value) const;

private:
	union
	{
		int32_t m_integer;
		float m_float;
		char* m_string = nullptr;
	};
	EType m_type;
};