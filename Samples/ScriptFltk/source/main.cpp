#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kReadyText = "Ready - press Click me";

int g_count = 0;
int g_code = 0;
int g_next = 0;
Fl_Box *g_label = nullptr;
kern::IVirtualMachine *g_vm = nullptr;

int get_count() { return g_count; }

void emit(int code, int next)
{
    g_code = code;
    g_next = next;
}

bool callScript(const char *name)
{
    try
    {
        if (!g_vm->callFunction(name))
        {
            std::cerr << "function " << name << " not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return false;
    }
    return true;
}

void showCount(int count)
{
    const std::string text = "Clicks: " + std::to_string(count);
    g_label->copy_label(text.c_str());
}

void logClick()
{
    std::cout << "click " << g_next << ": count " << g_next;
    if (g_code == 1)
    {
        std::cout << " first click";
    }
    else if (g_code == 2)
    {
        std::cout << " milestone";
    }
    std::cout << '\n';
}

void onClick(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!callScript("on_click"))
    {
        return;
    }
    g_count = g_next;
    showCount(g_count);
    logClick();
}

void onReset(Fl_Widget * /*widget*/, void * /*data*/)
{
    g_count = 0;
    if (!callScript("on_reset"))
    {
        return;
    }
    g_label->copy_label(kReadyText);
    std::cout << "reset: count 0\n";
}

} // namespace

int main(int argc, char **argv)
{
    const std::string source = R"(extern get_count();
extern emit(code, next);
func on_click() {
  var count = get_count();
  var next = count + 1;
  var code = 0;
  if (next == 1) {
    code = 1;
  } else {
    if (next / 10 * 10 == next) {
      code = 2;
    } else {
      code = 0;
    }
  }
  emit(code, next);
}
func on_reset() {
  emit(0, 0);
}
)";

    std::stringstream asmCode;
    {
        std::istringstream input(source);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode))
        {
            std::cerr << "compile failed\n";
            return 1;
        }
    }

    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed\n";
            return 1;
        }
    }

    try
    {
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        vm->addFunction("get_count", kern::createExternFunction(&get_count));
        vm->addFunction("emit", kern::createExternFunction(&emit));
        if (!vm->load(byteCode))
        {
            std::cerr << "load failed\n";
            return 1;
        }
        g_vm = vm.get();

        Fl_Window window(340, 180, "KernScript Clicker");
        Fl_Button clickButton(20, 20, 120, 30, "Click me");
        clickButton.callback(onClick);
        Fl_Button resetButton(150, 20, 120, 30, "Reset");
        resetButton.callback(onReset);
        Fl_Box label(20, 70, 280, 24, kReadyText);
        g_label = &label;
        window.end();
        window.show(argc, argv);
        Fl::run();
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
