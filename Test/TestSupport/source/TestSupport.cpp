#include "testutil/TestSupport.hpp"

#include <fstream>
#include <sstream>

// Assert failures in the library hit the C runtime default report mode, which
// raises a modal dialog. Route them to stderr so a failing test aborts without
// blocking on a message box.
#ifdef _WIN32
#include <crtdbg.h>

namespace {

struct ReportModeInitializer {
  ReportModeInitializer() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  }
};

const ReportModeInitializer g_reportModeInitializer;
} // namespace
#endif

namespace {

// The active recorder the host functions forward to. Host functions are
// registered as plain function pointers, so the recorder has to be reachable
// from a file static. Tests are single threaded.
testutil::OutputRecorder *g_recorder = nullptr;
} // namespace

namespace host {

// Deliberately namespaced: "print" and "log" collide with the C library
// functions of the same name that <sstream> pulls into the global namespace,
// which makes taking their address ambiguous.
namespace {

void print(std::string text) { g_recorder->print(text); }

void println(std::string text) { g_recorder->println(text); }

void log(std::string severity, std::string message) { g_recorder->log(severity, message); }

int get_value() { return 35; }

int fast_fib(int v) {
  if (v < 2)
    return v;
  return fast_fib(v - 1) + fast_fib(v - 2);
}
} // namespace
} // namespace host

testutil::OutputRecorder::~OutputRecorder() {}

const std::vector<std::string> &testutil::RecordingLogger::messages() const { return m_messages; }

bool testutil::RecordingLogger::empty() const { return m_messages.empty(); }

void testutil::RecordingLogger::onError(const std::string &message) { m_messages.push_back(message); }

void testutil::StringRecorder::print(const std::string &text) { m_text += text; }

void testutil::StringRecorder::println(const std::string &text) {
  m_text += text;
  m_text += '\n';
}

void testutil::StringRecorder::log(const std::string &severity, const std::string &message) {
  m_text += '[';
  m_text += severity;
  m_text += "]: ";
  m_text += message;
  m_text += '\n';
}

const std::string &testutil::StringRecorder::text() const { return m_text; }

void testutil::StringRecorder::clear() { m_text.clear(); }

void testutil::DiscardRecorder::print(const std::string &text) {}

void testutil::DiscardRecorder::println(const std::string &text) {}

void testutil::DiscardRecorder::log(const std::string &severity, const std::string &message) {}

void testutil::registerHostFunctions(kern::IVirtualMachine &vm, OutputRecorder &recorder) {
  g_recorder = &recorder;

  vm.addFunction("print", kern::createExternFunction(&host::print));
  vm.addFunction("println", kern::createExternFunction(&host::println));
  vm.addFunction("log", kern::createExternFunction(&host::log));
  vm.addFunction("get_value", kern::createExternFunction(&host::get_value));
  vm.addFunction("fast_fib", kern::createExternFunction(&host::fast_fib));
}

bool testutil::assemble(const std::string &asmSource, std::stringstream &byteCode) {
  std::stringstream source(asmSource);

  std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
  if (!assembler->assemble(source, byteCode)) {
    return false;
  }
  return true;
}

bool testutil::runScript(const std::string &asmSource, const std::string &functionName, OutputRecorder &recorder) {
  std::stringstream byteCode;
  if (!assemble(asmSource, byteCode)) {
    return false;
  }

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  registerHostFunctions(*vm, recorder);

  if (!vm->load(byteCode)) {
    return false;
  }

  if (!vm->callFunction(functionName)) {
    return false;
  }
  return true;
}

std::string testutil::readFile(const std::string &path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    return std::string();
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}
