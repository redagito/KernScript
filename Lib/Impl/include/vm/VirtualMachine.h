#pragma once

#include <cstdint>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

#include "kern/IExternFunction.h"
#include "kern/ILogger.h"
#include "kern/IVirtualMachine.h"
#include "kern/VMConfig.h"

#include "common/ExternFunction.h"
#include "common/Instruction.h"
#include "vm/Value.h"

#ifdef KERNSCRIPT_PROFILING
#include <array>
#include <chrono>
#include <iosfwd>
#endif

class VirtualMachine : public kern::IVirtualMachine
{
public:
    /**
     * \brief Constructs a virtual machine with default limits.
     * \param logger Receives diagnostics. A null pointer discards them.
     */
    explicit VirtualMachine(kern::ILogger *logger);

    /**
     * \brief Constructs a virtual machine with explicit limits.
     * \param logger Receives diagnostics. A null pointer discards them.
     * \param config Bounds the runtime stack and the call depth.
     */
    VirtualMachine(kern::ILogger *logger, kern::VMConfig config);

    bool load(std::istream &byteCode);

    void clearScripts();

    bool callFunction(const std::string &functionName);

    void addFunction(const std::string &functionName, kern::IExternFunction *function);
    void removeFunction(const std::string &functionName);

    bool popParameter(int &value);
    bool popParameter(float &value);
    bool popParameter(std::string &value);
    bool popParameter(double &value);
    bool popParameter(unsigned int &value);
    bool popParameter(bool &value);

    void setReturnValue();
    void setReturnValue(int value);
    void setReturnValue(float value);
    void setReturnValue(const std::string &value);
    void setReturnValue(double value);
    void setReturnValue(unsigned int value);
    void setReturnValue(bool value);

#ifdef KERNSCRIPT_PROFILING
    /**
     * \brief Clears all profiling counters.
     *
     * Enabled builds only; the method does not exist otherwise.
     */
    void resetProfile();

    /**
     * \brief Prints per-opcode counts and cycle shares, hottest first.
     *
     * Rows cover executed opcodes plus a fetch/decode row, sorted by cycles
     * descending with the raw opcode value breaking ties. Timings are
     * RELATIVE (the probe clocks roughly double loop cost), never absolute.
     */
    void dumpProfile(std::ostream &output);
#endif

protected:
    /**
     * \brief Byte code deserialization.
     *
     * Reads the header and all tables into the member vectors. Reports
     * truncations with false and limit violations by throwing; either way the
     * caller (load) wipes the tables, so a failure never leaves partial data.
     */
    bool loadTables(std::istream &byteCode);
    bool deserializeStrings(std::istream &stream);
    bool deserializeExternFunctions(std::istream &stream);
    bool deserializeFunctions(std::istream &stream);

    bool getFunctionIndexByName(const std::string &functionName, uint32_t &index);

    bool execute();

    /**
     * \brief Pushes value on top of runtime stack.
     */
    void pushValue(const Value &value);

    /**
     * \brief Pops value from runtime stack into supplied variable.
     */
    bool popValue(Value &val);

private:
    /**
     * \brief Assembled function.
     */
    struct Function
    {
        std::string name;                      /**< Function name. */
        uint32_t stackSize = 0;                /**< Stack size needed for local variables and passed parameters. */
        uint32_t parameterSize = 0;            /**< Stack size needed for parameters. */
        std::vector<Instruction> instructions; /**< Assembled instructions. */
        uint32_t instructionSize = 0;          /**< Cached instruction size. */
    };

    /**
     * \brief Represents function and instruction index to return to, after
     * instruction ret is executed.
     */
    struct FunctionFrame
    {
        FunctionFrame();
        FunctionFrame(uint32_t funcIndex, uint32_t instrIndex, uint32_t stackBaseIndex);
        uint32_t functionIndex = 0;
        uint32_t instructionIndex = 0;
        uint32_t runtimeStackBaseIndex = 0;
    };

    std::vector<std::string> m_strings; /**< String constants. */
    std::vector<Function> m_functions;  /**< Assembled script functions. */
    std::vector<ExternFunction> m_externFunctions;

    std::unordered_map<std::string, std::unique_ptr<kern::IExternFunction>>
        m_externFunctionsMap; /**< Extern functions. */

    std::vector<Value> m_runtimeStack;     /**< Active runtime stack. */
    std::stack<FunctionFrame> m_callStack; /**< Function call stack for instruction ret. */

    uint32_t m_currentRuntimeStackBaseIndex = 0; /**< Runtime stack base index of current function. */
    uint32_t m_currentFunctionIndex = 0;         /**< Index of active function. */
    uint32_t m_currentInstructionIndex = 0;      /**< Index of active instruction. */
    kern::ILogger *m_logger = nullptr;           /**< Receives diagnostics. */
    size_t m_maxStackSize = 0;                   /**< Runtime stack budget in slots. */
    uint32_t m_maxCallDepth = 0;                 /**< Nested call budget in frames. */
    size_t m_maxStringBytes = 0;                 /**< Longest single string or name accepted from byte code. */
    uint32_t m_maxTableEntries = 0;              /**< Most entries accepted in one byte code table. */
    uint64_t m_maxExecutionSteps = 0;            /**< Instruction budget per callFunction, reset on every call. */
    bool m_running = false;                      /**< Reentrancy guard for execute, reset on every exit. */

#ifdef KERNSCRIPT_PROFILING
    /**
     * \brief One counter pair per profiling bucket.
     *
     * Cycles hold steady_clock nanoseconds: RELATIVE shares, never absolute
     * production timings.
     */
    struct ProfileEntry
    {
        uint64_t count = 0;
        uint64_t cycles = 0;
    };

    // One bucket per opcode, indexed by the raw opcode value. Hardcoded so the
    // storage is a plain array; the asserts pin it to the Opcode enum and the
    // mnemonic table, so adding an opcode without updating this fails loudly.
    static constexpr size_t kProfileOpcodeCount = 38;
    static_assert(static_cast<size_t>(Opcode::Jle) + 1 == kProfileOpcodeCount,
                  "kProfileOpcodeCount must match the Opcode enum.");
    static_assert(sizeof(instructions) / sizeof(instructions[0]) == kProfileOpcodeCount,
                  "Opcode mnemonic table out of sync.");

    /**
     * \brief RAII timer attributing one iteration's body time to its opcode.
     *
     * The destructor stamps the end time, so returns and exception unwinds
     * attribute alike. Unknown ids (corrupt bytecode) have no bucket; their
     * body time joins the fetch/decode bucket instead of vanishing.
     */
    class BodyProbe
    {
    public:
        BodyProbe(VirtualMachine &owner, size_t rawOpcode, bool knownOpcode,
                  std::chrono::steady_clock::time_point bodyStart);
        ~BodyProbe();

        BodyProbe(const BodyProbe &) = delete;
        BodyProbe &operator=(const BodyProbe &) = delete;

    private:
        VirtualMachine &m_owner;
        size_t m_rawOpcode;
        bool m_knownOpcode;
        std::chrono::steady_clock::time_point m_bodyStart;
    };

    // Per-instance storage, no globals: the VM is single-threaded by its
    // m_running reentrancy contract. Guarded, so when KERNSCRIPT_PROFILING is
    // off the class layout is byte-identical to the uninstrumented VM.
    std::array<ProfileEntry, kProfileOpcodeCount> m_profileEntries{};
    ProfileEntry m_profileFetchDecode{};
    uint64_t m_profileLoopIterations = 0;
    uint64_t m_profileCallEntries = 0;
#endif
};
