#pragma once

#include <functional>

#include "IExternFunction.h"
#include "IVirtualMachine.h"


/**
* \brief Provides function wrappers for easy registration of C++ functions to the VM.
*/
namespace kern
{

template <typename ReturnType, typename ... ArgumentTypes>
class CExternFunction : public IExternFunction
{
	// Empty
};

template <typename ReturnType>
class CExternFunction<ReturnType> : public IExternFunction
{
public:
	CExternFunction(ReturnType (*function)(void))
		:
		m_function(function)
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 0;
	}

	bool call(IVirtualMachine& vm)
	{
		ReturnValue retVal = m_function();
		vm.setReturnValue(retVal);
		return true;
	}
	
private:
	::std::function<ReturnType(void)> m_function;
};

template<>
class CExternFunction<void> : public IExternFunction
{
public:
	CExternFunction(void (*function)(void))
		:
		m_function(function)
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 0;
	}
	
	bool call(IVirtualMachine& vm)
	{
		m_function();
		vm.setReturnValue();
		return true;
	}
	
private:
	::std::function<void(void)> m_function;
};

template <typename ReturnType, typename ArgType0>
class CExternFunction<ReturnType, ArgType0> : public IExternFunction
{
public:
	CExternFunction(ReturnType (*function)(ArgType0))
		:
		m_function(function)
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 1;
	}
	
	bool call(IVirtualMachine& vm)
	{
		ArgType0 arg0;
		if (!vm.popParameter(arg0))
		{
			return false;
		}
		ReturnValue retVal = m_function(arg0);
		vm.setReturnValue(retVal);
		return true;
	}
	
private:
	::std::function<ReturnType(ArgType0)> m_function;
};

template <typename ArgType0>
class CExternFunction<void, ArgType0> : public IExternFunction
{
public:
	CExternFunction(void (*function)(ArgType0))
		:
		m_function(function)
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 1;
	}
	
	bool call(IVirtualMachine& vm)
	{
		ArgType0 arg0;
		if (!vm.popParameter(arg0))
		{
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
class CExternFunction<ReturnType, ArgType0, ArgType1> : public IExternFunction
{
public:
	CExternFunction(ReturnType (*function)(ArgType0, ArgType1))
		:
		m_function(function)
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 2;
	}
	
	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		if (!ok)
		{
			return false;
		}
		
		ReturnValue retVal = m_function(arg0, arg1);
		vm.setReturnValue(retVal);
		return true;
	}
	
private:
	::std::function<ReturnType(ArgType0, ArgType1)> m_function;
};

template <typename ArgType0, typename ArgType1>
class CExternFunction<void, ArgType0, ArgType1> : public IExternFunction
{
public:
	CExternFunction(void (*function)(ArgType0, ArgType1))
		:
		m_function(function);
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 2;
	}
	
	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		if (!ok)
		{
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
class CExternFunction<ReturnType, ArgType0, ArgType1, ArgType2> : public IExternFunction
{
public:
	CExternFunction(ReturnType (*function)(ArgType0, ArgType1, ArgType2))
		:
		m_function(function);
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 3;
	}

	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		ArgType2 arg2;
		ok &= vm.popParameter(arg2);
		
		if (!ok)
		{
			return false;
		}
		
		ReturnValue retVal = m_function(arg0, arg1, arg2);
		vm.setReturnValue(retVal);
		return true;
	}
	
private:
	::std::function<ReturnType(ArgType0, ArgType1, ArgType2)> m_function;
};

template <typename ArgType0, typename ArgType1, typename ArgType2>
class CExternFunction<void, ArgType0, ArgType1, ArgType2> : public IExternFunction
{
public:
	CExternFunction(void (*function)(ArgType0, ArgType1, ArgType2))
		:
		m_function(function);
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 3;
	}
	
	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		ArgType2 arg2;
		ok &= vm.popParameter(arg2);
		
		if (!ok)
		{
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
class CExternFunction<ReturnType, ArgType0, ArgType1, ArgType2, ArgType3> : public IExternFunction
{
public:
	CExternFunction(ReturnType (*function)(ArgType0, ArgType1, ArgType2, ArgType3))
		:
		m_function(function);
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 4;
	}
	
	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		ArgType2 arg2;
		ok &= vm.popParameter(arg2);
		
		ArgType3 arg3;
		ok &= vm.popParameter(arg3);
		
		if (!ok)
		{
			return false;
		}
		
		ReturnValue retVal = m_function(arg0, arg1, arg2, arg3);
		vm.setReturnValue(retVal);
		return true;
	}
	
private:
	::std::function<ReturnType(ArgType0, ArgType1, ArgType2, ArgType3)> m_function;
};

template <typename ArgType0, typename ArgType1, typename ArgType2, typename ArgType3>
class CExternFunction<void, ArgType0, ArgType1, ArgType2, ArgType3> : public IExternFunction
{
public:
	CExternFunction(void (*function)(ArgType0, ArgType1, ArgType2, ArgType3))
		:
		m_function(function);
	{
		// Empty
	}

	unsigned int getArgumentCount() const
	{
		return 4;
	}
	
	bool call(IVirtualMachine& vm)
	{
		bool ok = true;
		ArgType0 arg0;
		ok = vm.popParameter(arg0);
		
		ArgType1 arg1;
		ok &= vm.popParameter(arg1);
		
		ArgType2 arg2;
		ok &= vm.popParameter(arg2);
		
		ArgType3 arg3;
		ok &= vm.popParameter(arg3);
		
		if (!ok)
		{
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
template <typename ReturnType>
IExternFunction* createExternFunction(ReturnType(*externFunction)(void))
{
	return new CExternFunction<ReturnType>(externFunction);
}

template <typename ReturnType, typename ArgType0>
IExternFunction* createExternFunction(ReturnType(*externFunction)(ArgType0))
{
	return new CExternFunction<ReturnType, ArgType0>(externFunction);
}

template <typename ReturnType, typename ArgType0, typename ArgType1>
IExternFunction* createExternFunction(ReturnType(*externFunction)(ArgType0, ArgType1))
{
	return new CExternFunction<ReturnType, ArgType0, ArgType1>(externFunction);
}

template <typename ReturnType, typename ArgType0, typename ArgType1, typename ArgType2>
IExternFunction* createExternFunction(ReturnType(*externFunction)(ArgType0, ArgType1, ArgType2))
{
	return new CExternFunction<ReturnType, ArgType0, ArgType1, ArgType2>(externFunction);
}

}