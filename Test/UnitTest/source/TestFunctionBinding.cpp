#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <kern/FunctionBinding.h>
#include <kern/IVirtualMachine.h>

namespace
{

int sub4(int a, int b, int c, int d) { return a - b - c - d; }

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
class FakeVM : public kern::IVirtualMachine
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

    bool load(std::istream &) override { return true; }
    void clearScripts() override {}
    bool callFunction(const std::string &) override { return true; }
    void addFunction(const std::string &, kern::IExternFunction *) override {}
    void removeFunction(const std::string &) override {}

    bool popParameter(int &value) override
    {
        value = intParams.back();
        intParams.pop_back();
        return true;
    }

    bool popParameter(float &value) override
    {
        value = 0.f;
        return true;
    }

    bool popParameter(std::string &value) override
    {
        value.clear();
        return true;
    }

    bool popParameter(double &value) override
    {
        value = 0.0;
        return true;
    }

    bool popParameter(unsigned int &value) override
    {
        value = 0;
        return true;
    }

    bool popParameter(bool &value) override
    {
        value = false;
        return true;
    }

    void setReturnValue() override {}
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
    REQUIRE(fn->getArgumentCount() == 4);

    FakeVM vm;
    vm.intParams = {1, 2, 3, 4};
    REQUIRE(fn->call(vm));

    // Arguments pop in reverse push order, so 1 - 2 - 3 - 4 proves the order.
    REQUIRE(vm.intReturned);
    REQUIRE(vm.intResult == -8);
}

TEST_CASE("createExternFunction supports four void arguments", "[unit][binding]")
{
    std::unique_ptr<kern::IExternFunction> fn(kern::createExternFunction(&consume4));
    REQUIRE(fn->getArgumentCount() == 4);

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
