#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <kern/FunctionBinding.h>
#include <kern/IExternCall.h>

namespace
{

int sub4(int a, int b, int c, int d) { return a - b - c - d; }

int orderedTen(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{
    REQUIRE(a == 1); REQUIRE(b == 2); REQUIRE(c == 3); REQUIRE(d == 4); REQUIRE(e == 5);
    REQUIRE(f == 6); REQUIRE(g == 7); REQUIRE(h == 8); REQUIRE(i == 9); REQUIRE(j == 10);
    return 55;
}
void consumeTen(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{ REQUIRE(orderedTen(a,b,c,d,e,f,g,h,i,j) == 55); }

void consume4(int a, int b, int c, int d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

double pi() { return 3.25; }

unsigned int answer() { return 3000000000u; }

bool truthy() { return true; }

/**
 * \brief Minimal virtual machine for binding tests.
 *
 * Hands out preloaded integers and records which setReturnValue overload the
 * binding dispatches to.
 */
class FakeVM : public kern::IExternCall
{
public:
    std::vector<int> intParams;
    int intResult = 0;
    bool intReturned = false;
    double doubleResult = 0.0;
    bool doubleReturned = false;
    unsigned int unsignedResult = 0;
    bool unsignedReturned = false;
    bool boolResult = false;
    bool boolReturned = false;

    unsigned int getArgumentCount() const override { return static_cast<unsigned int>(intParams.size()); }

    bool getArgument(unsigned int index, int &value) const override
    {
        value = intParams.at(index);
        return true;
    }

    bool getArgument(unsigned int index, float &value) const override
    {
        value = 0.f;
        return true;
    }

    bool getArgument(unsigned int index, std::string &value) const override
    {
        value.clear();
        return true;
    }

    bool getArgument(unsigned int index, double &value) const override
    {
        value = 0.0;
        return true;
    }

    bool getArgument(unsigned int index, unsigned int &value) const override
    {
        value = 0;
        return true;
    }

    bool getArgument(unsigned int index, bool &value) const override
    {
        value = false;
        return true;
    }

    void setReturnValue(int value) override
    {
        intResult = value;
        intReturned = true;
    }
    void setReturnValue(float) override {}
    void setReturnValue(const std::string &) override {}
    void setReturnValue(double value) override
    {
        doubleResult = value;
        doubleReturned = true;
    }
    void setReturnValue(unsigned int value) override
    {
        unsignedResult = value;
        unsignedReturned = true;
    }
    void setReturnValue(bool value) override
    {
        boolResult = value;
        boolReturned = true;
    }
};

} // namespace

TEST_CASE("createExternFunction supports four arguments", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&sub4));
    REQUIRE(fn->getSignature().argumentCount == 4);

    FakeVM vm;
    vm.intParams = {1, 2, 3, 4};
    REQUIRE(fn->call(vm));

    // Indexed reads preserve source order, so 1 - 2 - 3 - 4 proves the order.
    REQUIRE(vm.intReturned);
    REQUIRE(vm.intResult == -8);
}

TEST_CASE("Bindings support larger valued and void signatures in source order", "[unit][binding]")
{
    FakeVM call;
    call.intParams = {1,2,3,4,5,6,7,8,9,10};
    std::unique_ptr<kern::IExternFunction> valued(kern::createExternFunction(&orderedTen));
    REQUIRE(valued->getSignature().argumentCount == 10);
    REQUIRE(valued->call(call));
    REQUIRE(call.intResult == 55);
    call.intReturned = false;
    std::unique_ptr<kern::IExternFunction> empty(kern::createExternFunction(&consumeTen));
    REQUIRE(empty->getSignature().argumentCount == 10);
    REQUIRE(empty->getSignature().returnKind == kern::ReturnKind::Void);
    REQUIRE(empty->call(call));
    REQUIRE_FALSE(call.intReturned);
}

TEST_CASE("createExternFunction supports four void arguments", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&consume4));
    REQUIRE(fn->getSignature().argumentCount == 4);

    FakeVM vm;
    vm.intParams = {1, 2, 3, 4};
    REQUIRE(fn->call(vm));
}

TEST_CASE("Extern functions return doubles", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&pi));

    FakeVM vm;
    REQUIRE(fn->call(vm));

    REQUIRE(vm.doubleReturned);
    REQUIRE(vm.doubleResult == 3.25);
}

TEST_CASE("Extern functions return unsigned integers", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&answer));

    FakeVM vm;
    REQUIRE(fn->call(vm));

    // Beyond the signed range, so the int overload could never hold it.
    REQUIRE(vm.unsignedReturned);
    REQUIRE(vm.unsignedResult == 3000000000u);
}

TEST_CASE("Extern functions return booleans", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&truthy));

    FakeVM vm;
    REQUIRE(fn->call(vm));

    REQUIRE(vm.boolReturned);
    REQUIRE(vm.boolResult);
}

TEST_CASE("Null host function pointers are refused before binding", "[unit][binding]")
{
    REQUIRE_THROWS_AS(kern::createExternFunction(static_cast<void (*)()>(nullptr)), kern::RuntimeError);
}

TEST_CASE("Bindings expose both argument and return contracts", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> valued(kern::createExternFunction(&sub4));
    std::unique_ptr<kern::IExternFunction> empty(kern::createExternFunction(&consume4));
    REQUIRE(valued->getSignature().returnKind == kern::ReturnKind::Value);
    REQUIRE(empty->getSignature().returnKind == kern::ReturnKind::Void);
}
