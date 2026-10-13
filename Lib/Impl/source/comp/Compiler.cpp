#include "comp/Compiler.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "comp/Ast.h"
#include "comp/Parser.h"
#include "logging/Logger.h"

#include "comp/Lower.h"

namespace
{

// Slice 3 import resolution: search paths, file loading, identity, cycles.
// Resolution merges every reachable module's externs and funcs into one
// program table; lowering stays as-is (unqualified, so qualified calls still
// lower unqualified here) and only load failures fail the compile now.
// Visibility and mangling arrive in a later slice.

// Cap on the active import chain. Each level is C++ recursion through
// resolve/load/parse, so a runaway chain fails clean like the parser nesting
// bound instead of overflowing the host stack.
constexpr std::size_t kMaxImportDepth = 128;

std::string displayPath(const std::filesystem::path &path) { return path.generic_string(); }

// Identity key for "same file twice": the canonical path, falling back to the
// absolute path when the file cannot be canonicalized (it was just found, so
// this only guards exotic I/O failures).
std::string canonicalKey(const std::filesystem::path &path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path key = fs::canonical(path, ec);
    if (ec)
    {
        ec.clear();
        key = fs::absolute(path, ec);
    }
    if (ec)
    {
        return displayPath(path);
    }
    return key.generic_string();
}

// Exact, case-sensitive lookup of "<name>.ks" across the search paths in
// order. Directory entries are compared by exact file name instead of opening
// a candidate path, which would follow the host case-insensitive match on
// Windows and accept "math.ks" for "import Math;". Returns an empty path on
// a miss; unreadable entries fail the whole compile later as a miss.
std::filesystem::path findImportFile(const std::string &name, const std::vector<std::string> &searchPaths)
{
    namespace fs = std::filesystem;
    const std::string want = name + ".ks";
    for (const std::string &dir : searchPaths)
    {
        std::error_code ec;
        fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
        if (ec)
        {
            continue;
        }
        const fs::directory_iterator end;
        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                break;
            }
            const bool regular = it->is_regular_file(ec);
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (regular && it->path().filename().string() == want)
            {
                return it->path();
            }
        }
    }
    return std::filesystem::path();
}

bool loadModuleSource(const std::filesystem::path &path, std::string &text)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

std::string joinPaths(const std::vector<std::string> &paths)
{
    std::string out;
    for (std::size_t i = 0; i < paths.size(); ++i)
    {
        if (i != 0)
        {
            out += ", ";
        }
        out += paths[i];
    }
    return out;
}

struct ImportResolver
{
    kern::ILogger *logger = nullptr;
    std::vector<std::string> searchPaths;
    // Canonical key of every fully loaded module: the same file twice parses
    // once and is shared (diamonds, repeated imports).
    std::map<std::string, bool> loaded;
    // Module name to display path (or root source name) of the file that
    // claimed it first. The same name from two different files is an error,
    // never a silent first-wins.
    std::map<std::string, std::string> moduleSource;
    // Active resolution chain for cycle reports, outer to inner.
    std::vector<std::string> stackNames;
    std::vector<std::string> stackKeys;
    // Function name to defining file display, so a cross-file duplicate can
    // name both definitions. Merged lowering is one unqualified table, so a
    // silent overwrite here would change which body a call runs.
    std::map<std::string, std::string> funcSource;

    // Resolves one import of "name" written at namePos in the file shown as
    // importerDisplay, appending the loaded subtree to merged in post-order
    // (every dependency lands before its importer). depth counts the active
    // chain from the root's direct imports.
    bool resolveImport(const std::string &name, SrcPos namePos, const std::string &importerDisplay,
                       ast::Program &merged, std::size_t depth)
    {
        if (depth > kMaxImportDepth)
        {
            reportError(logger, importerDisplay, namePos, "Import nesting too deep.", "shorten the import chain");
            return false;
        }
        if (searchPaths.empty())
        {
            reportError(logger, importerDisplay, namePos,
                        "Cannot resolve import '" + name + "': no search paths configured.",
                        "pass search paths to the compile overload");
            return false;
        }
        const std::filesystem::path hit = findImportFile(name, searchPaths);
        if (hit.empty())
        {
            reportError(logger, importerDisplay, namePos,
                        "Cannot find module '" + name + "': file '" + name + ".ks" + "' not found in search paths (" +
                            joinPaths(searchPaths) + ").",
                        "add a directory containing '" + name + ".ks' to the search paths");
            return false;
        }
        const std::string display = displayPath(hit);
        const std::string key = canonicalKey(hit);
        if (loaded.find(key) != loaded.end())
        {
            return true;
        }
        for (std::size_t i = 0; i < stackKeys.size(); ++i)
        {
            if (stackKeys[i] == key)
            {
                std::string chain;
                for (std::size_t j = i; j < stackNames.size(); ++j)
                {
                    if (!chain.empty())
                    {
                        chain += " -> ";
                    }
                    chain += stackNames[j];
                }
                chain += " -> " + name;
                reportError(logger, importerDisplay, namePos, "Import cycle detected: " + chain + ".",
                            "remove one import to break the cycle");
                return false;
            }
        }
        std::string text;
        if (!loadModuleSource(hit, text))
        {
            reportError(logger, importerDisplay, namePos, "Cannot read file '" + display + "'.");
            return false;
        }
        std::istringstream input(text);
        std::vector<Tok> toks;
        if (!tokenize(input, logger, toks, display))
        {
            return false;
        }
        ast::Program module;
        if (!ast::parseProgram(toks, logger, display, module))
        {
            return false;
        }
        if (!module.hasModule)
        {
            reportError(logger, importerDisplay, namePos, "Imported file '" + display + "' has no module declaration.",
                        "only the root file may omit the module declaration");
            return false;
        }
        if (module.module.name != name)
        {
            reportError(logger, importerDisplay, namePos,
                        "Module name mismatch: file '" + display + "' declares module '" + module.module.name +
                            "', expected '" + name + "'.",
                        "make the file name match the module declaration");
            return false;
        }
        const auto claimed = moduleSource.find(name);
        if (claimed != moduleSource.end())
        {
            reportError(logger, importerDisplay, namePos,
                        "Duplicate module '" + name + "' from '" + display + "' (already loaded from '" +
                            claimed->second + "').",
                        "give each module a unique name");
            return false;
        }
        moduleSource[name] = display;
        stackNames.push_back(name);
        stackKeys.push_back(key);
        for (const ast::ImportDecl &child : module.imports)
        {
            if (!resolveImport(child.name, child.namePos, display, merged, depth + 1))
            {
                return false;
            }
        }
        stackNames.pop_back();
        stackKeys.pop_back();
        for (const ast::Extern &ext : module.externs)
        {
            bool known = false;
            for (const ast::Extern &have : merged.externs)
            {
                if (have.name == ext.name)
                {
                    known = true;
                    if (have.params != ext.params)
                    {
                        reportError(logger, importerDisplay, namePos,
                                    "Conflicting extern '" + ext.name + "' from '" + display + "'.",
                                    "declare each extern with one parameter list");
                        return false;
                    }
                    break;
                }
            }
            if (!known)
            {
                merged.externs.push_back(ext);
            }
        }
        for (const ast::Func &func : module.funcs)
        {
            const auto defined = funcSource.find(func.name);
            if (defined != funcSource.end())
            {
                reportError(logger, importerDisplay, namePos,
                            "Duplicate function '" + func.name + "' from '" + display + "' (already defined in '" +
                                defined->second + "').",
                            "use a different name");
                return false;
            }
        }
        for (ast::Func &func : module.funcs)
        {
            funcSource[func.name] = display;
            merged.funcs.push_back(std::move(func));
        }
        loaded[key] = true;
        return true;
    }
};

} // namespace

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger())
{
    // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode)
{
    return compile(sourceCode, asmCode, "<input>");
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName)
{
    return compile(sourceCode, asmCode, sourceName, std::vector<std::string>());
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName,
                       const std::vector<std::string> &searchPaths)
{
    // Orchestration only: tokenize, parse to an AST, resolve imports into one
    // merged table, lower, emit. The language contract lives on the ctor (see
    // Compiler.h); each phase lives in its own module (see Ast.h, Parser.h
    // and Lower.h). The root itself may be anonymous or named: no file name
    // is ever checked against its module declaration. Lowering sees the
    // merged table under the root source name (per-file lowering errors keep
    // their line and column but not their file).
    std::vector<Tok> toks;
    if (!tokenize(sourceCode, m_logger, toks, sourceName))
    {
        return false;
    }
    ast::Program program;
    if (!ast::parseProgram(toks, m_logger, sourceName, program))
    {
        return false;
    }
    if (!program.imports.empty())
    {
        ImportResolver resolver;
        resolver.logger = m_logger;
        resolver.searchPaths = searchPaths;
        if (program.hasModule)
        {
            resolver.moduleSource[program.module.name] = sourceName;
        }
        for (const ast::Func &func : program.funcs)
        {
            resolver.funcSource[func.name] = sourceName;
        }
        const std::vector<ast::ImportDecl> roots = program.imports;
        for (const ast::ImportDecl &root : roots)
        {
            if (!resolver.resolveImport(root.name, root.namePos, sourceName, program, 1))
            {
                return false;
            }
        }
    }
    std::vector<Lowered> lowered;
    if (!lowerProgram(program, m_logger, sourceName, lowered))
    {
        return false;
    }
    emitProgram(program, lowered, asmCode);
    return true;
}
