#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <windows.h>

#include <kern/Kern.h>

namespace
{

constexpr int kButtonId = 1;
constexpr int kClicks = 12;

int g_count = 0;
int g_code = 0;
int g_next = 0;
HWND g_label = nullptr;
kern::IVirtualMachine *g_vm = nullptr;
bool g_failed = false;

int get_count() { return g_count; }

void emit(int code, int next)
{
    g_code = code;
    g_next = next;
}

void pumpMessages()
{
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COMMAND && LOWORD(wparam) == kButtonId)
    {
        try
        {
            if (!g_vm->callFunction("on_click"))
            {
                std::cerr << "function on_click not found\n";
                g_failed = true;
                PostQuitMessage(1);
                return 0;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cerr << "runtime error: " << error.what() << '\n';
            g_failed = true;
            PostQuitMessage(1);
            return 0;
        }
        g_count = g_next;
        const std::string text = "Clicks: " + std::to_string(g_count);
        SetWindowTextA(g_label, text.c_str());
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
        return 0;
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
    if (next == 10) {
      code = 2;
    } else {
      code = 0;
    }
  }
  emit(code, next);
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
        windowClass.lpszClassName = "ScriptGuiHidden";
        if (!RegisterClassA(&windowClass))
        {
            std::cerr << "register class failed\n";
            return 1;
        }
        HWND window = CreateWindowExA(0, "ScriptGuiHidden", "ScriptGui", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                      CW_USEDEFAULT, 320, 160, nullptr, nullptr, instance, nullptr);
        if (!window)
        {
            std::cerr << "create window failed\n";
            return 1;
        }
        HWND button =
            CreateWindowExA(0, "BUTTON", "Click me", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 120, 30, window,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kButtonId)), instance, nullptr);
        if (!button)
        {
            std::cerr << "create button failed\n";
            DestroyWindow(window);
            return 1;
        }
        g_label = CreateWindowExA(0, "STATIC", "Clicks: 0", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 70, 260, 24, window,
                                  nullptr, instance, nullptr);
        if (!g_label)
        {
            std::cerr << "create label failed\n";
            DestroyWindow(window);
            return 1;
        }
        for (int i = 0; i < kClicks; ++i)
        {
            SendMessage(button, BM_CLICK, 0, 0);
            pumpMessages();
            if (g_failed)
            {
                DestroyWindow(window);
                return 1;
            }
        }
        pumpMessages();
        DestroyWindow(window);
        pumpMessages();
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
