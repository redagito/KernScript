#include "testutil/TestSupport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
namespace
{
namespace fs = std::filesystem;
struct EventState
{
    int count = 0, code = 0, next = 0, emissions = 0;
} eventState;
int eventCount() { return eventState.count; }
void eventEmit(int code, int next)
{
    eventState.code = code;
    eventState.next = next;
    ++eventState.emissions;
}
void checkEventWorkflow(kern::IVirtualMachine &vm)
{
    eventState = {};
    vm.addFunction("get_count", &eventCount);
    vm.addFunction("emit", &eventEmit);
    for (int click = 1; click <= 25; ++click)
    {
        eventState.emissions = 0;
        REQUIRE(vm.callFunction("on_click"));
        CHECK(eventState.emissions == 1);
        CHECK(eventState.next == click);
        CHECK(eventState.code == (click == 1 ? 1 : click % 10 == 0 ? 2 : 0));
        eventState.count = eventState.next;
    }
    eventState.emissions = 0;
    REQUIRE(vm.callFunction("on_reset"));
    CHECK(eventState.emissions == 1);
    CHECK(eventState.code == 0);
    CHECK(eventState.next == 0);
    eventState.count = eventState.next;
    REQUIRE(vm.callFunction("on_click"));
    CHECK(eventState.code == 1);
    CHECK(eventState.next == 1);
}
fs::path samplesRoot()
{
    return fs::absolute(fs::path(__FILE__)).parent_path().parent_path().parent_path().parent_path() / "Samples";
}
std::vector<fs::path> sampleFiles(const std::string &extension)
{
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(samplesRoot()), end; it != end; ++it)
    {
        if (it->is_symlink())
        {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file() && it->path().extension() == extension)
            files.push_back(it->path());
    }
    return files;
}
void checkSource(const std::string &source, const fs::path &file, const std::vector<fs::path> &modules)
{
    CAPTURE(file.string());
    const auto relative = file.lexically_relative(samplesRoot());
    const auto root = samplesRoot() / (*relative.begin());
    std::set<std::string> directories;
    for (const auto &candidate : modules)
        if (candidate.lexically_relative(root).generic_string().find("..") != 0)
            directories.insert(candidate.parent_path().string());
    std::vector<std::string> paths{file.parent_path().string()};
    paths.insert(paths.end(), directories.begin(), directories.end());
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream first(source), second(source), direct, assembly, assembled;
    const bool compiled = compiler->compileBytecode(first, direct, file.string(), paths);
    std::ostringstream diagnostics;
    for (const auto &message : logger.messages())
        diagnostics << message << '\n';
    INFO(diagnostics.str());
    REQUIRE(compiled);
    REQUIRE(compiler->compile(second, assembly, file.string(), paths));
    REQUIRE(testutil::assemble(assembly.str(), assembled));
    REQUIRE(direct.str() == assembled.str());
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(nullptr));
    REQUIRE(vm->load(direct));
    if (root.filename() == "NativeEvents" || root.filename() == "FltkEvents")
        checkEventWorkflow(*vm);
}
} // namespace
TEST_CASE("All shipped sample script modules compile through equivalent verified paths", "[integration][samples]")
{
    const auto files = sampleFiles(".ks");
    REQUIRE_FALSE(files.empty());
    for (const auto &file : files)
        checkSource(testutil::readFile(file.string()), file, files);
}
TEST_CASE("Embedded sample scripts compile without launching GUI applications", "[integration][samples]")
{
    const auto modules = sampleFiles(".ks");
    size_t count = 0;
    for (const auto &file : sampleFiles(".cpp"))
    {
        const auto text = testutil::readFile(file.string());
        size_t cursor = 0;
        while ((cursor = text.find("R\"", cursor)) != std::string::npos)
        {
            const auto open = text.find('(', cursor + 2);
            if (open == std::string::npos)
                break;
            const auto delimiter = text.substr(cursor + 2, open - cursor - 2);
            if (delimiter.size() > 16 || delimiter.find_first_of(" \t\r\n") != std::string::npos)
            {
                cursor += 2;
                continue;
            }
            const auto closing = ")" + delimiter + "\"";
            const auto end = text.find(closing, open + 1);
            if (end == std::string::npos)
                break;
            const auto script = text.substr(open + 1, end - open - 1);
            cursor = end + closing.size();
            if (script.find("func ") == std::string::npos)
                continue;
            ++count;
            checkSource(script, file, modules);
        }
    }
    REQUIRE(count > 0);
}
