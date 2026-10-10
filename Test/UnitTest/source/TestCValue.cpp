#include <catch2/catch_test_macros.hpp>

#include "vm/CValue.h"

TEST_CASE("CValue carries the type it was constructed with", "[unit][CValue]")
{
  CValue integer(static_cast<int32_t>(-7));
  REQUIRE(integer.getType() == CValue::EType::Integer);

  CValue unsignedInteger(static_cast<uint32_t>(7));
  REQUIRE(unsignedInteger.getType() == CValue::EType::UnsignedInteger);

  CValue floatValue(1.25f);
  REQUIRE(floatValue.getType() == CValue::EType::Float);

  CValue stringValue(std::string("text"));
  REQUIRE(stringValue.getType() == CValue::EType::String);
}

TEST_CASE("Default constructed CValue is invalid", "[unit][CValue]")
{
  CValue value;
  REQUIRE(value.getType() == CValue::EType::Invalid);
}

TEST_CASE("CValue converts integer to the other arithmetic types",
          "[unit][CValue]")
{
  CValue value(static_cast<int32_t>(42));

  int32_t asInteger = 0;
  REQUIRE(value.convert(asInteger));
  REQUIRE(asInteger == 42);

  uint32_t asUnsigned = 0;
  REQUIRE(value.convert(asUnsigned));
  REQUIRE(asUnsigned == 42u);

  float asFloat = 0.f;
  REQUIRE(value.convert(asFloat));
  REQUIRE(asFloat == 42.f);

  std::string asString;
  REQUIRE(value.convert(asString));
  REQUIRE(asString == "42");
}

TEST_CASE("CValue converts float to the other arithmetic types",
          "[unit][CValue]")
{
  CValue value(2.5f);

  int32_t asInteger = 0;
  REQUIRE(value.convert(asInteger));
  REQUIRE(asInteger == 2);

  float asFloat = 0.f;
  REQUIRE(value.convert(asFloat));
  REQUIRE(asFloat == 2.5f);

  std::string asString;
  REQUIRE(value.convert(asString));
  REQUIRE(asString == "2.5");
}

TEST_CASE("CValue rejects conversions it does not implement", "[unit][CValue]")
{
  CValue value(std::string("text"));

  int32_t asInteger = 0;
  REQUIRE_FALSE(value.convert(asInteger));

  uint32_t asUnsigned = 0;
  REQUIRE_FALSE(value.convert(asUnsigned));

  float asFloat = 0.f;
  REQUIRE_FALSE(value.convert(asFloat));

  std::string asString;
  REQUIRE(value.convert(asString));
  REQUIRE(asString == "text");
}

TEST_CASE("CValue converts unsigned integer", "[unit][CValue]")
{
  CValue value(static_cast<uint32_t>(7));

  int32_t asInteger = 0;
  REQUIRE(value.convert(asInteger));
  REQUIRE(asInteger == 7);

  float asFloat = 0.f;
  REQUIRE(value.convert(asFloat));
  REQUIRE(asFloat == 7.f);

  std::string asString;
  REQUIRE(value.convert(asString));
  REQUIRE(asString == "7");
}

TEST_CASE("Invalid CValue does not convert", "[unit][CValue]")
{
  CValue value;

  int32_t asInteger = 0;
  REQUIRE_FALSE(value.convert(asInteger));

  std::string asString;
  REQUIRE_FALSE(value.convert(asString));

  REQUIRE(value.toString() == "Invalid");
}

TEST_CASE("CValue compares integers", "[unit][CValue]")
{
  CValue left(static_cast<int32_t>(1));
  CValue right(static_cast<int32_t>(2));

  REQUIRE(left == left);
  REQUIRE_FALSE(left == right);
  REQUIRE(left != right);
  REQUIRE(left <= left);
  REQUIRE(left <= right);
}

TEST_CASE("CValue adds integers", "[unit][CValue]")
{
  CValue value(static_cast<int32_t>(1));
  CValue operand(static_cast<int32_t>(2));

  value += operand;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3);
}

TEST_CASE("CValue subtracts integers", "[unit][CValue]")
{
  CValue value(static_cast<int32_t>(5));
  CValue operand(static_cast<int32_t>(2));

  value -= operand;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3);
}

TEST_CASE("CValue copies strings", "[unit][CValue]")
{
  CValue value(std::string("copy"));
  CValue copy(value);

  std::string copyText;
  REQUIRE(copy.convert(copyText));
  REQUIRE(copyText == "copy");

  CValue assigned;
  assigned = value;

  std::string assignedText;
  REQUIRE(assigned.convert(assignedText));
  REQUIRE(assignedText == "copy");
}

TEST_CASE("CValue self assignment leaves the value intact", "[unit][CValue]")
{
  CValue value(std::string("self"));

  CValue &reference = value;
  value = reference;

  std::string text;
  REQUIRE(value.convert(text));
  REQUIRE(text == "self");
}

TEST_CASE("CValue moves strings", "[unit][CValue]")
{
  CValue value(std::string("move"));
  CValue moved(std::move(value));

  std::string text;
  REQUIRE(moved.convert(text));
  REQUIRE(text == "move");

  CValue assigned(std::string("other"));
  assigned = std::move(moved);

  std::string movedText;
  REQUIRE(assigned.convert(movedText));
  REQUIRE(movedText == "move");
}

TEST_CASE("CValue assignment replaces the previous string", "[unit][CValue]")
{
  CValue value(std::string("first"));
  value = CValue(std::string("second"));

  std::string text;
  REQUIRE(value.convert(text));
  REQUIRE(text == "second");
}
