#pragma once

#include <istream>
#include <string>
#include <cstdint>

/**
* \brief Deserialize 16 bit integer value.
*/
bool deserialize(std::istream& stream, int8_t& value);

/**
* \brief Deserialize 16 bit unsigned integer value.
*/
bool deserialize(std::istream& stream, uint8_t& value);

/**
* \brief Deserialize 16 bit integer value.
*/
bool deserialize(std::istream& stream, int16_t& value);

/**
* \brief Deserialize 16 bit unsigned integer value.
*/
bool deserialize(std::istream& stream, uint16_t& value);

/**
* \brief Deserialize 32 bit integer value.
*/
bool deserialize(std::istream& stream, int32_t& value);

/**
* \brief Deserialize 32 bit unsigned integer value.
*/
bool deserialize(std::istream& stream, uint32_t& value);

/**
* \brief Deserialize 32 bit floating value.
*/
bool deserialize(std::istream& stream, float& value);

/**
* \brief Deserialize string.
*/
bool deserialize(std::istream& stream, std::string& str);