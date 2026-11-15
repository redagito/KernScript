#pragma once
#include "common/Program.h"
#include "kern/IExternFunction.h"
#include "kern/ILogger.h"
#include "kern/IVirtualMachine.h"
#include "kern/VMConfig.h"
#include "vm/Value.h"
#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
class VirtualMachine : public kern::IVirtualMachine
{
public:
    explicit VirtualMachine(kern::ILogger *);
    VirtualMachine(kern::ILogger *, kern::VMConfig);
    ~VirtualMachine() override = default;
    VirtualMachine(VirtualMachine &&) noexcept = default;
    VirtualMachine &operator=(VirtualMachine &&) noexcept = default;
    VirtualMachine(const VirtualMachine &) = delete;
    VirtualMachine &operator=(const VirtualMachine &) = delete;
    bool load(std::istream &) override;
    void clearScripts() override;
    bool callFunction(const std::string &) override;
    using kern::IVirtualMachine::addFunction;
    void addFunction(const std::string &, kern::IExternFunction *) override;
    void removeFunction(const std::string &) override;
    void resetProfile();
    void dumpProfile(std::ostream &);

private:
    struct Frame
    {
        uint32_t function;
        size_t ip;
        size_t localsBase;
        size_t operandBase;
    };
    struct Profile
    {
        std::array<uint64_t, std::size(instructions)> count{};
        std::array<uint64_t, std::size(instructions)> ns{};
        uint64_t calls = 0, steps = 0, fetchNs = 0;
    };
    kern::ILogger *m_logger;
    kern::VMConfig m_config;
    bool m_running = false;
    std::optional<VerifiedProgram> m_program;
    std::vector<Value> m_constants;
    std::vector<Value> m_locals;
    std::vector<Value> m_operands;
    std::vector<Frame> m_frames;
    std::shared_ptr<Value::StringBudget> m_stringBudget;
    std::unordered_map<std::string, std::unique_ptr<kern::IExternFunction>> m_bindings;
    std::unique_ptr<Profile> m_profile;
    // Cold lookup data stays after the runtime state used by the dispatch loop.
    // Sorted indices preserve bytecode call targets and own no function names.
    std::vector<uint32_t> m_entryIndex;
    void idle(const char *) const;
    [[noreturn]] void fail(const std::string &) const;
    void checkStack(size_t locals, size_t operands) const;
    void push(Value);
    Value pop();
    Value &local(int32_t);
    void enter(uint32_t);
    bool step();
};
