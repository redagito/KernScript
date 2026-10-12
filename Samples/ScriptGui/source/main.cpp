#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <windows.h>

#include <kern/Kern.h>

namespace
{

constexpr int kClickButtonId = 1;
constexpr int kResetButtonId = 2;

constexpr const char *kWindowTitle = "KernScript Clicker";
constexpr const char *kReadyText = "Ready - press Click me";

int g_count = 0;
int g_code = 0;
int g_next = 0;
HWND g_label = nullptr;
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
    SetWindowTextA(g_label, text.c_str());
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

void onClick()
{
    if (!callScript("on_click"))
    {
        return;
    }
    g_count = g_next;
    showCount(g_count);
    logClick();
}

void onReset()
{
    g_count = 0;
    if (!callScript("on_reset"))
    {
        return;
    }
    SetWindowTextA(g_label, kReadyText);
    std::cout << "reset: count 0\n";
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COMMAND)
    {
        const int id = LOWORD(wparam);
        if (id == kClickButtonId)
        {
            onClick();
            return 0;
        }
        if (id == kResetButtonId)
        {
            onReset();
            return 0;
        }
    }
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

} // namespace

int main()
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

        HINSTANCE instance = GetModuleHandleA(nullptr);
        WNDCLASSA windowClass = {};
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = "ScriptGuiClicker";
        if (!RegisterClassA(&windowClass))
        {
            std::cerr << "register class failed\n";
            return 1;
        }
        HWND window = CreateWindowExA(0, "ScriptGuiClicker", kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                      CW_USEDEFAULT, 340, 180, nullptr, nullptr, instance, nullptr);
        if (!window)
        {
            std::cerr << "create window failed\n";
            return 1;
        }
        HWND clickButton =
            CreateWindowExA(0, "BUTTON", "Click me", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 120, 30, window,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kClickButtonId)), instance, nullptr);
        if (!clickButton)
        {
            std::cerr << "create click button failed\n";
            DestroyWindow(window);
            return 1;
        }
        HWND resetButton =
            CreateWindowExA(0, "BUTTON", "Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 20, 120, 30, window,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kResetButtonId)), instance, nullptr);
        if (!resetButton)
        {
            std::cerr << "create reset button failed\n";
            DestroyWindow(window);
            return 1;
        }
        g_label = CreateWindowExA(0, "STATIC", kReadyText, WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 70, 280, 24, window,
                                  nullptr, instance, nullptr);
        if (!g_label)
        {
            std::cerr << "create label failed\n";
            DestroyWindow(window);
            return 1;
        }

        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);

        MSG message;
        while (GetMessage(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
