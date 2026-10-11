#include <catch2/catch_test_macros.hpp>

#include "kern/RuntimeError.h"
#include "vm/Value.h"

TEST_CASE("Value carries the type it was constructed with", "[unit][Value]")
{
  Value integer(static_cast<int32_t>(-7));
  REQUIRE(integer.getType() == Value::Type::Integer);

  Value unsignedInteger(static_cast<uint32_t>(7));
  REQUIRE(unsignedInteger.getType() == Value::Type::UnsignedInteger);

  Value floatValue(1.25f);
  REQUIRE(floatValue.getType() == Value::Type::Float);

  Value stringValue(std::string("text"));
  REQUIRE(stringValue.getType() == Value::Type::String);
}

TEST_CASE("Value arithmetic widens to the wider type", "[unit][Value]")
{
  Value integer(static_cast<int32_t>(1));
  Value fraction(2.5f);

  integer += fraction;

  REQUIRE(integer.getType() == Value::Type::Float);
  float result = 0.f;
  REQUIRE(integer.convert(result));
  REQUIRE(result == 3.5f);
}

TEST_CASE("Value addition overflow throws", "[unit][Value][c2]")
{
  Value value(static_cast<int32_t>(2147483647));
  Value operand(static_cast<int32_t>(1));

  REQUIRE_THROWS_AS(value += operand, kern::RuntimeError);
}

TEST_CASE("Value division of INT_MIN by minus one throws", "[unit][Value][c2]")
{
  Value value(static_cast<int32_t>(-2147483647 - 1));
  Value operand(static_cast<int32_t>(-1));

  REQUIRE_THROWS_AS(value /= operand, kern::RuntimeError);
}

TEST_CASE("Value huge float to int conversion throws", "[unit][Value][c2]")
{
  Value value(1e30f);

  int32_t asInteger = 0;
  REQUIRE_THROWS_AS(value.convert(asInteger), kern::RuntimeError);
}

TEST_CASE("Value out of range addition throws instead of wrapping",
          "[unit][Value]")
{
  Value value(static_cast<int32_t>(2000000000));
  Value operand(static_cast<int32_t>(2000000000));

  // The sum does not fit in an int32, so the operation throws instead of
  // wrapping around.
  REQUIRE_THROWS_AS(value += operand, kern::RuntimeError);
}

TEST_CASE("Value multiplies", "[unit][Value]")
{
  Value value(static_cast<int32_t>(3));
  Value operand(static_cast<int32_t>(4));

  value *= operand;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 12);
}

TEST_CASE("Value multiplies floats", "[unit][Value]")
{
  Value value(2.5f);
  Value operand(4.f);

  value *= operand;

  REQUIRE(value.getType() == Value::Type::Float);
  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 10.f);
}

TEST_CASE("Value multiplies across types", "[unit][Value]")
{
  Value value(2.5f);
  Value operand(static_cast<int32_t>(4));

  value *= operand;

  REQUIRE(value.getType() == Value::Type::Float);
  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 10.f);
}

TEST_CASE("Value divides integers", "[unit][Value]")
{
  Value value(static_cast<int32_t>(17));
  Value operand(static_cast<int32_t>(5));

  value /= operand;

  // Integer division truncates.
  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3);
}

TEST_CASE("Value divides floats", "[unit][Value]")
{
  Value value(1.f);
  Value operand(4.f);

  value /= operand;

  REQUIRE(value.getType() == Value::Type::Float);
  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 0.25f);
}

TEST_CASE("Value division by zero leaves the value unchanged",
          "[unit][Value]")
{
  Value value(static_cast<int32_t>(10));
  Value zero(static_cast<int32_t>(0));

  value /= zero;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 10);
}

TEST_CASE("Value float division by zero does not trap", "[unit][Value]")
{
  Value value(1.f);
  Value zero(0.f);

  value /= zero;

  float result = 0.f;
  REQUIRE(value.convert(result));
  // IEEE infinity, which is what float division produces.
  REQUIRE(result > 1e30f);
}

TEST_CASE("Value arithmetic ignores non numeric operands",
          "[unit][Value]")
{
  Value value(static_cast<int32_t>(7));
  Value text(std::string("seven"));

  value += text;
  value *= text;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 7);
}

TEST_CASE("Value compares across types", "[unit][Value]")
{
  Value integer(static_cast<int32_t>(2));
  Value fraction(2.5f);

  REQUIRE(integer < fraction);
  REQUIRE(integer <= fraction);
  REQUIRE_FALSE(integer == fraction);
  REQUIRE(integer != fraction);
  REQUIRE(fraction > integer);
  REQUIRE(fraction >= integer);
}

TEST_CASE("Value orders equal values", "[unit][Value]")
{
  Value value(static_cast<int32_t>(5));

  REQUIRE(value <= value);
  REQUIRE(value >= value);
  REQUIRE(value == value);
  REQUIRE_FALSE(value < value);
  REQUIRE_FALSE(value > value);
}

TEST_CASE("Value compares strings by value", "[unit][Value]")
{
  Value left(std::string("same"));
  Value right(std::string("same"));
  Value other(std::string("different"));

  REQUIRE(left == right);
  REQUIRE(left != other);
}

TEST_CASE("Value compares strings of different types as unequal",
          "[unit][Value]")
{
  Value text(std::string("7"));
  Value number(static_cast<int32_t>(7));

  REQUIRE(text != number);
  REQUIRE_FALSE(text == number);
}

TEST_CASE("Value does not assert on comparisons", "[unit][Value]")
{
  // These used to reach assert(false) and abort the process.
  Value floatValue(1.5f);
  Value otherFloat(2.5f);

  REQUIRE_FALSE(floatValue == otherFloat);
  REQUIRE(floatValue <= otherFloat);

  Value integer(static_cast<int32_t>(1));
  REQUIRE_FALSE(floatValue == integer);
  REQUIRE(floatValue >= integer);

  Value invalid;
  REQUIRE_FALSE(invalid == floatValue);
  REQUIRE_FALSE(floatValue == invalid);
}

TEST_CASE("Value adds floats", "[unit][Value]")
{
  Value value(1.5f);
  Value operand(2.25f);

  value += operand;

  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3.75f);
}

TEST_CASE("Value subtracts floats", "[unit][Value]")
{
  Value value(2.5f);
  Value operand(0.5f);

  value -= operand;

  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 2.f);
}

TEST_CASE("Value reports whether it is numeric", "[unit][Value]")
{
  REQUIRE(Value(static_cast<int32_t>(1)).isNumeric());
  REQUIRE(Value(static_cast<uint32_t>(1)).isNumeric());
  REQUIRE(Value(1.5f).isNumeric());

  Value text(std::string("text"));
  REQUIRE_FALSE(text.isNumeric());

  Value invalid;
  REQUIRE_FALSE(invalid.isNumeric());
}

TEST_CASE("Value unsigned arithmetic wraps", "[unit][Value]")
{
  Value value(static_cast<uint32_t>(0));
  Value operand(static_cast<uint32_t>(1));

  value -= operand;

  REQUIRE(value.getType() == Value::Type::UnsignedInteger);
  uint32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 4294967295u);
}

TEST_CASE("Default constructed Value is invalid", "[unit][Value]")
{
  Value value;
  REQUIRE(value.getType() == Value::Type::Invalid);
}

TEST_CASE("Value converts integer to the other arithmetic types",
          "[unit][Value]")
{
  Value value(static_cast<int32_t>(42));

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

TEST_CASE("Value converts float to the other arithmetic types",
          "[unit][Value]")
{
  Value value(2.5f);

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

TEST_CASE("Value refuses string to numeric conversion", "[unit][Value]")
{
  Value value(std::string("text"));

  int32_t asInteger = 0;
  REQUIRE_THROWS_AS(value.convert(asInteger), kern::RuntimeError);

  uint32_t asUnsigned = 0;
  REQUIRE_THROWS_AS(value.convert(asUnsigned), kern::RuntimeError);

  float asFloat = 0.f;
  REQUIRE_THROWS_AS(value.convert(asFloat), kern::RuntimeError);

  std::string asString;
  REQUIRE(value.convert(asString));
  REQUIRE(asString == "text");
}

TEST_CASE("Value converts unsigned integer", "[unit][Value]")
{
  Value value(static_cast<uint32_t>(7));

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

TEST_CASE("Invalid Value does not convert", "[unit][Value]")
{
  Value value;

  int32_t asInteger = 0;
  REQUIRE_FALSE(value.convert(asInteger));

  std::string asString;
  REQUIRE_FALSE(value.convert(asString));

  REQUIRE(value.toString() == "Invalid");
}

TEST_CASE("Value compares integers", "[unit][Value]")
{
  Value left(static_cast<int32_t>(1));
  Value right(static_cast<int32_t>(2));

  REQUIRE(left == left);
  REQUIRE_FALSE(left == right);
  REQUIRE(left != right);
  REQUIRE(left <= left);
  REQUIRE(left <= right);
}

TEST_CASE("Value adds integers", "[unit][Value]")
{
  Value value(static_cast<int32_t>(1));
  Value operand(static_cast<int32_t>(2));

  value += operand;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3);
}

TEST_CASE("Value subtracts integers", "[unit][Value]")
{
  Value value(static_cast<int32_t>(5));
  Value operand(static_cast<int32_t>(2));

  value -= operand;

  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 3);
}

TEST_CASE("Value move leaves the moved-from value invalid", "[unit][Value][m7]")
{
  Value value(std::string("move"));
  Value moved(std::move(value));

  std::string text;
  REQUIRE(moved.convert(text));
  REQUIRE(text == "move");

  // The source must have given up its string, so it reports invalid and can
  // be destroyed and reassigned without freeing anything twice.
  REQUIRE(value.getType() == Value::Type::Invalid);

  value = Value(static_cast<int32_t>(1));
  int32_t result = 0;
  REQUIRE(value.convert(result));
  REQUIRE(result == 1);
}

TEST_CASE("Value mixed-type overflow throws", "[unit][Value][m8]")
{
  Value value(static_cast<int32_t>(2147483647));
  Value operand(static_cast<uint32_t>(1));

  // Widens to signed, whose range the true sum leaves.
  REQUIRE_THROWS_AS(value += operand, kern::RuntimeError);
}

TEST_CASE("Value mixed-type arithmetic keeps float precision",
          "[unit][Value][m8]")
{
  Value value(static_cast<int32_t>(16777217));
  Value operand(0.0f);

  value += operand;

  // Float wins, so the result rounds to what a float can hold.
  REQUIRE(value.getType() == Value::Type::Float);
  float result = 0.f;
  REQUIRE(value.convert(result));
  REQUIRE(result == 16777216.f);
}

TEST_CASE("Value copies strings", "[unit][Value]")
{
  Value value(std::string("copy"));
  Value copy(value);

  std::string copyText;
  REQUIRE(copy.convert(copyText));
  REQUIRE(copyText == "copy");

  Value assigned;
  assigned = value;

  std::string assignedText;
  REQUIRE(assigned.convert(assignedText));
  REQUIRE(assignedText == "copy");
}

TEST_CASE("Value self assignment leaves the value intact", "[unit][Value]")
{
  Value value(std::string("self"));

  Value &reference = value;
  value = reference;

  std::string text;
  REQUIRE(value.convert(text));
  REQUIRE(text == "self");
}

TEST_CASE("Value moves strings", "[unit][Value]")
{
  Value value(std::string("move"));
  Value moved(std::move(value));

  std::string text;
  REQUIRE(moved.convert(text));
  REQUIRE(text == "move");

  Value assigned(std::string("other"));
  assigned = std::move(moved);

  std::string movedText;
  REQUIRE(assigned.convert(movedText));
  REQUIRE(movedText == "move");
}

TEST_CASE("Value assignment replaces the previous string", "[unit][Value]")
{
  Value value(std::string("first"));
  value = Value(std::string("second"));

  std::string text;
  REQUIRE(value.convert(text));
  REQUIRE(text == "second");
}
