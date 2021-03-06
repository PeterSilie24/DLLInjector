/*
 * MIT License
 *
 * Copyright (c) 2018 Phil Badura
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../shared/process.hpp"

typedef HMODULE (__stdcall *FPLoadLibrary)(LPCTSTR);
typedef FARPROC (__stdcall *FPGetProcAddress)(HMODULE, LPCSTR);

struct InjectorData
{
	InjectorData(FPLoadLibrary pLoadLibrary, FPGetProcAddress pGetProcAddress, const std::tstring& strPath, const std::tstring strFunction) : pLoadLibrary(pLoadLibrary), pGetProcAddress(pGetProcAddress)
	{
		_tcscpy_s(this->path, strPath.c_str());

		std::string strFunctionTmp(strFunction.begin(), strFunction.end());

		strcpy_s(this->function, strFunctionTmp.c_str());
	}

	FPLoadLibrary pLoadLibrary;
	FPGetProcAddress pGetProcAddress;

	TCHAR path[MAX_PATH];
	char function[MAX_PATH];
};

typedef void(*FPFunction)();

enum class InjectorResult : DWORD
{
	DLLLoadFailed = 0,
	FunctionLoadFailed = 1,
	Success = 2,
};

#pragma code_seg(push, "INJECTOR")

InjectorResult WINAPI injectorBegin(InjectorData* injectorData)
{
	FPLoadLibrary pLoadLibrary = injectorData->pLoadLibrary;

	FPGetProcAddress pGetProcAddress = injectorData->pGetProcAddress;

	LPCTSTR lpPath = injectorData->path;

	LPCSTR lpFunction = nullptr;

	if (injectorData->function[0] != '\0')
	{
		lpFunction = injectorData->function;
	}

	HMODULE hModule = pLoadLibrary(lpPath);

	if (!hModule)
	{
		return InjectorResult::DLLLoadFailed;
	}

	if (lpFunction)
	{
		FPFunction fpFunction = reinterpret_cast<FPFunction>(pGetProcAddress(hModule, lpFunction));

		if (!fpFunction)
		{
			return InjectorResult::FunctionLoadFailed;
		}

		fpFunction();
	}

	return InjectorResult::Success;
}

void WINAPI injectorEnd()
{

}

#pragma code_seg(pop)

bool inject(DWORD dwProcessId, const std::tstring& strPath, const std::tstring& strFunction = TEXT(""))
{
	Deleter<HANDLE> hProcess(OpenProcess(PROCESS_ALL_ACCESS, false, dwProcessId), CloseHandle);

	if (!hProcess)
	{
		std::tcout << TEXT("Failed to open the process width id ") << dwProcessId << "." << std::endl;

		return false;
	}

	Architecture executableArchitecture = getExecutableArchitecture();

	Architecture processArchitecture = getProcessArchitecture(hProcess);

	if (executableArchitecture == Architecture::Unknown)
	{
		std::tcout << TEXT("The system architecture is not supported.") << std::endl;

		return false;
	}

	if (processArchitecture != executableArchitecture)
	{
		std::tcout << TEXT("The process architecture must match the architecture of the injector process.") << std::endl;

		return false;
	}

	#ifdef UNICODE

	#define LOAD_LIBRARY "LoadLibraryW"

	#else

	#define LOAD_LIBRARY "LoadLibraryA"

	#endif

	FPLoadLibrary pLoadLibrary = reinterpret_cast<FPLoadLibrary>(GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), LOAD_LIBRARY));

	if (!pLoadLibrary)
	{
		std::string strLoadLibrary(LOAD_LIBRARY);

		std::tcout << TEXT("Failed to get the address of procedure ") << std::tstring(strLoadLibrary.begin(), strLoadLibrary.end()) << TEXT(" in module kernel32.dll.") << std::endl;

		return false;
	}

	FPGetProcAddress pGetProcAddress = reinterpret_cast<FPGetProcAddress>(GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProcAddress"));

	if (!pGetProcAddress)
	{
		std::tcout << TEXT("Failed to get the address of procedure GetProcAddress in module kernel32.dll.") << std::endl;

		return false;
	}

	InjectorData injectorData(pLoadLibrary, pGetProcAddress, strPath, strFunction);

	std::size_t begin = reinterpret_cast<std::size_t>(injectorBegin);

	std::size_t end = reinterpret_cast<std::size_t>(injectorEnd);

	if (begin > end)
	{
		std::tcout << TEXT("The injector function is not aligned in memory as expected, therefore the size cannot be determined.") << std::endl;

		return false;
	}

	std::size_t injectorSize = end - begin;

	std::size_t size = sizeof(InjectorData) + injectorSize;

	Deleter<PVOID> pVirtualMemory(VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE), [=](LPVOID pVirtualMemory) { VirtualFreeEx(hProcess, pVirtualMemory, 0, MEM_RELEASE); });

	if (!pVirtualMemory)
	{
		std::tcout << TEXT("Failed to allocate memory to the target process.") << std::endl;

		return false;
	}

	LPVOID lpInjectorData = pVirtualMemory;

	if (!WriteProcessMemory(hProcess, lpInjectorData, &injectorData, sizeof(InjectorData), nullptr))
	{
		std::tcout << TEXT("Failed to write to the target process memory.") << std::endl;

		return false;
	}

	LPVOID lpInjector = reinterpret_cast<LPVOID>(reinterpret_cast<std::size_t>(lpInjectorData) + sizeof(InjectorData));

	if (!WriteProcessMemory(hProcess, lpInjector, injectorBegin, injectorSize, nullptr))
	{
		std::tcout << TEXT("Failed to write to the target process memory.") << std::endl;

		return false;
	}

	Deleter<HANDLE> hRemoteProcess(CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(lpInjector), lpInjectorData, 0, nullptr), CloseHandle);

	if (!hRemoteProcess)
	{
		std::tcout << TEXT("Failed to create a remote thread into the target process.") << std::endl;

		return false;
	}

	WaitForSingleObject(hRemoteProcess, INFINITE);

	DWORD dwExitCode;

	GetExitCodeThread(hRemoteProcess, &dwExitCode);

	InjectorResult result = static_cast<InjectorResult>(dwExitCode);

	if (result == InjectorResult::DLLLoadFailed)
	{
		std::tcout << TEXT("Failed to load the dll at the target process.\n\nMake sure the dll exists and has the same architecture as the process.") << std::endl;

		return false;
	}
	else if (result == InjectorResult::FunctionLoadFailed)
	{
		std::tcout << TEXT("The dll was injected, but the function ") << strFunction << TEXT(" could not be found in the dll.") << std::endl;

		return false;
	}

	return true;
}

int main()
{
	std::vector<std::tstring> args = getArgs();

	if (args.size() < 3)
	{
		std::tcout << TEXT("Usage: inject pid dllfile [function]") << std::endl;

		return EXIT_FAILURE;
	}

	DWORD dwProcessId = fromStr<DWORD>(args[1]);

	std::tstring strPath = args[2];

	std::tstring strFunction;

	if (args.size() > 3)
	{
		strFunction = args[3];
	}

	enableDebugPrivilege();

	if (!inject(dwProcessId, strPath, strFunction))
	{
		return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}
