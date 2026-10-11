#pragma once

#include <functional>
#include <type_traits>

#include "IExternFunction.h"
#include "IVirtualMachine.h"

/**
 * \brief Provides function wrappers for easy registration of C++ functions to
 * the VM.
 */
namespace kern {

template <typename ReturnType, typename... ArgumentTypes> class ExternFunction : public IExternFunction {
  // Empty
};

template <typename ReturnType> class ExternFunction<ReturnType> : public IExternFunction {
public:
  ExternFunction(ReturnType (*function)(void)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 0; }

  bool call(IVirtualMachine &vm) {
    ReturnType retVal = m_function();
    vm.setReturnValue(retVal);
    return true;
  }

private:
  ::std::function<ReturnType(void)> m_function;
};

template <> class ExternFunction<void> : public IExternFunction {
public:
  ExternFunction(void (*function)(void)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 0; }

  bool call(IVirtualMachine &vm) {
    m_function();
    vm.setReturnValue();
    return true;
  }

private:
  ::std::function<void(void)> m_function;
};

template <typename ReturnType, typename ArgType0> class ExternFunction<ReturnType, ArgType0> : public IExternFunction {
public:
  ExternFunction(ReturnType (*function)(ArgType0)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 1; }

  bool call(IVirtualMachine &vm) {
    ArgType0 arg0;
    if (!vm.popParameter(arg0)) {
      return false;
    }
    ReturnType retVal = m_function(arg0);
    vm.setReturnValue(retVal);
    return true;
  }

private:
  ::std::function<ReturnType(ArgType0)> m_function;
};

template <typename ArgType0> class ExternFunction<void, ArgType0> : public IExternFunction {
public:
  ExternFunction(void (*function)(ArgType0)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 1; }

  bool call(IVirtualMachine &vm) {
    ArgType0 arg0;
    if (!vm.popParameter(arg0)) {
      return false;
    }
    m_function(arg0);
    vm.setReturnValue();
    return true;
  }

private:
  ::std::function<void(ArgType0)> m_function;
};

template <typename ReturnType, typename ArgType0, typename ArgType1>
class ExternFunction<ReturnType, ArgType0, ArgType1> : public IExternFunction {
public:
  ExternFunction(ReturnType (*function)(ArgType0, ArgType1)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 2; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType1 arg1;
    ok = vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    ReturnType retVal = m_function(arg0, arg1);
    vm.setReturnValue(retVal);
    return true;
  }

private:
  ::std::function<ReturnType(ArgType0, ArgType1)> m_function;
};

template <typename ArgType0, typename ArgType1>
class ExternFunction<void, ArgType0, ArgType1> : public IExternFunction {
public:
  ExternFunction(void (*function)(ArgType0, ArgType1)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 2; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType1 arg1;
    ok = vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    m_function(arg0, arg1);
    vm.setReturnValue();
    return true;
  }

private:
  ::std::function<void(ArgType0, ArgType1)> m_function;
};

template <typename ReturnType, typename ArgType0, typename ArgType1, typename ArgType2>
class ExternFunction<ReturnType, ArgType0, ArgType1, ArgType2> : public IExternFunction {
public:
  ExternFunction(ReturnType (*function)(ArgType0, ArgType1, ArgType2)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 3; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType2 arg2;
    ok = vm.popParameter(arg2);

    ArgType1 arg1;
    ok &= vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    ReturnType retVal = m_function(arg0, arg1, arg2);
    vm.setReturnValue(retVal);
    return true;
  }

private:
  ::std::function<ReturnType(ArgType0, ArgType1, ArgType2)> m_function;
};

template <typename ArgType0, typename ArgType1, typename ArgType2>
class ExternFunction<void, ArgType0, ArgType1, ArgType2> : public IExternFunction {
public:
  ExternFunction(void (*function)(ArgType0, ArgType1, ArgType2)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 3; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType2 arg2;
    ok = vm.popParameter(arg2);

    ArgType1 arg1;
    ok &= vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    m_function(arg0, arg1, arg2);
    vm.setReturnValue();
    return true;
  }

private:
  ::std::function<void(ArgType0, ArgType1, ArgType2)> m_function;
};

template <typename ReturnType, typename ArgType0, typename ArgType1, typename ArgType2, typename ArgType3>
class ExternFunction<ReturnType, ArgType0, ArgType1, ArgType2, ArgType3> : public IExternFunction {
public:
  ExternFunction(ReturnType (*function)(ArgType0, ArgType1, ArgType2, ArgType3)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 4; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType3 arg3;
    ok = vm.popParameter(arg3);

    ArgType2 arg2;
    ok &= vm.popParameter(arg2);

    ArgType1 arg1;
    ok &= vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    ReturnType retVal = m_function(arg0, arg1, arg2, arg3);
    vm.setReturnValue(retVal);
    return true;
  }

private:
  ::std::function<ReturnType(ArgType0, ArgType1, ArgType2, ArgType3)> m_function;
};

template <typename ArgType0, typename ArgType1, typename ArgType2, typename ArgType3>
class ExternFunction<void, ArgType0, ArgType1, ArgType2, ArgType3> : public IExternFunction {
public:
  ExternFunction(void (*function)(ArgType0, ArgType1, ArgType2, ArgType3)) : m_function(function) {
    // Empty
  }

  unsigned int getArgumentCount() const { return 4; }

  bool call(IVirtualMachine &vm) {
    bool ok = true;
    // The script pushes arguments in source order, so the last argument is on
    // top of the runtime stack and has to be popped first.
    ArgType3 arg3;
    ok = vm.popParameter(arg3);

    ArgType2 arg2;
    ok &= vm.popParameter(arg2);

    ArgType1 arg1;
    ok &= vm.popParameter(arg1);

    ArgType0 arg0;
    ok &= vm.popParameter(arg0);

    if (!ok) {
      return false;
    }

    m_function(arg0, arg1, arg2, arg3);
    vm.setReturnValue();
    return true;
  }

private:
  ::std::function<void(ArgType0, ArgType1, ArgType2, ArgType3)> m_function;
};

/**
 * \brief Creates extern function with parameters.
 */
template <typename ReturnType> IExternFunction *createExternFunction(ReturnType (*externFunction)(void)) {
  return new ExternFunction<ReturnType>(externFunction);
}

template <typename ReturnType, typename ArgType0>
IExternFunction *createExternFunction(ReturnType (*externFunction)(ArgType0)) {
  return new ExternFunction<ReturnType, ArgType0>(externFunction);
}

template <typename ReturnType, typename ArgType0, typename ArgType1>
IExternFunction *createExternFunction(ReturnType (*externFunction)(ArgType0, ArgType1)) {
  return new ExternFunction<ReturnType, ArgType0, ArgType1>(externFunction);
}

template <typename ReturnType, typename ArgType0, typename ArgType1, typename ArgType2>
IExternFunction *createExternFunction(ReturnType (*externFunction)(ArgType0, ArgType1, ArgType2)) {
  return new ExternFunction<ReturnType, ArgType0, ArgType1, ArgType2>(externFunction);
}

template <typename ReturnType, typename ArgType0, typename ArgType1, typename ArgType2, typename ArgType3>
IExternFunction *createExternFunction(ReturnType (*externFunction)(ArgType0, ArgType1, ArgType2, ArgType3)) {
  return new ExternFunction<ReturnType, ArgType0, ArgType1, ArgType2, ArgType3>(externFunction);
}

/**
 * \brief Rejects extern functions with more than four arguments.
 *
 * The enable_if keeps this overload out of overload resolution for up to four
 * arguments, so the overloads above are never ambiguous. Anything larger lands
 * here and fails with the message below instead of a deduction backtrace.
 */
template <typename ReturnType, typename... ArgTypes, typename std::enable_if<(sizeof...(ArgTypes) > 4), int>::type = 0>
IExternFunction *createExternFunction(ReturnType (*externFunction)(ArgTypes...)) {
  static_assert(sizeof...(ArgTypes) <= 4, "Extern functions support at most 4 arguments.");
  return nullptr;
}
} // namespace kern
