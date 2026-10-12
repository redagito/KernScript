#pragma once

#include <cstdint>
#include <istream>
#include <string>

/**
 * \brief Deserialize 8 bit integer value.
 */
bool deserialize(std::istream &stream, int8_t &value);

/**
 * \brief Deserialize 8 bit unsigned integer value.
 */
bool deserialize(std::istream &stream, uint8_t &value);

/**
 * \brief Deserialize 16 bit integer value.
 */
bool deserialize(std::istream &stream, int16_t &value);

/**
 * \brief Deserialize 16 bit unsigned integer value.
 */
bool deserialize(std::istream &stream, uint16_t &value);

/**
 * \brief Deserialize 32 bit integer value.
 */
bool deserialize(std::istream &stream, int32_t &value);

/**
 * \brief Deserialize 32 bit unsigned integer value.
 */
bool deserialize(std::istream &stream, uint32_t &value);

/**
 * \brief Deserialize 64 bit unsigned integer value.
 */
bool deserialize(std::istream &stream, uint64_t &value);

/**
 * \brief Deserialize 32 bit floating value.
 */
bool deserialize(std::istream &stream, float &value);

/**
 * \brief Deserialize string.
 * \param maxLength Longest accepted content length. Longer prefixes throw
 *        kern::RuntimeError instead of reserving memory for corrupt input.
 */
bool deserialize(std::istream &stream, std::string &str, size_t maxLength);