/*
 * MIT License
 *
 * Copyright(c) 2018 Phil Badura
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "process.hpp"

namespace Process
{
	bool enableDebugPrivilege()
	{
		HANDLE hProcess = GetCurrentProcess();

		HANDLE hTokenTmp = nullptr;

		OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hTokenTmp);

		Base::Handle<HANDLE> hToken(hTokenTmp, CloseHandle);

		if (!hToken)
		{
			return false;
		}

		LUID luid;

		if (!LookupPrivilegeValue(0, SE_DEBUG_NAME, &luid))
		{
			return false;
		}

		TOKEN_PRIVILEGES privileges;
		privileges.PrivilegeCount = 1;
		privileges.Privileges->Luid = luid;
		privileges.Privileges->Attributes = SE_PRIVILEGE_ENABLED;

		if (!AdjustTokenPrivileges(*hToken, false, &privileges, 0, 0, 0))
		{
			return false;
		}

		return true;
	}

	Architecture getExecutableArchitecture()
	{
		#ifdef _WIN64

		return Architecture::Amd64;

		#elif _WIN32

		return Architecture::Intel;

		#else

		return Architecture::Unknown;

		#endif
	}

	Architecture getSystemArchitecture()
	{
		SYSTEM_INFO systemInfo;

		GetNativeSystemInfo(&systemInfo);

		if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
		{
			return Architecture::Amd64;
		}
		else if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		{
			return Architecture::Intel;
		}

		return Architecture::Unknown;
	}

	Architecture getProcessArchitecture(HANDLE hProcess)
	{
		Architecture systemArchitecture = getSystemArchitecture();

		if (systemArchitecture == Architecture::Amd64)
		{
			BOOL isWow64 = FALSE;

			IsWow64Process(hProcess, &isWow64);

			if (isWow64)
			{
				return Architecture::Intel;
			}

			return Architecture::Amd64;
		}
		else if (systemArchitecture == Architecture::Intel)
		{
			return Architecture::Intel;
		}

		return Architecture::Unknown;
	}

	std::tstring execute(const std::tstring& strCommandLine, LPDWORD lpExitCode)
	{
		HANDLE hReadPipeTmp = nullptr;

		HANDLE hWritePipeTmp = nullptr;

		SECURITY_ATTRIBUTES securityAttributes;

		ZeroMemory(&securityAttributes, sizeof(securityAttributes));

		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = true;

		CreatePipe(&hReadPipeTmp, &hWritePipeTmp, &securityAttributes, 4096);

		Base::Handle<HANDLE> hReadPipe(hReadPipeTmp, CloseHandle);

		Base::Handle<HANDLE> hWritePipe(hWritePipeTmp, CloseHandle);

		if (!hReadPipe || !hWritePipe)
		{
			return TEXT("Failed to create pipe.");
		}

		STARTUPINFO startupInfo;

		ZeroMemory(&startupInfo, sizeof(startupInfo));

		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = *hWritePipe;
		startupInfo.hStdError = *hWritePipe;

		PROCESS_INFORMATION processInformation;

		std::shared_ptr<TCHAR> pCommandLine(new TCHAR[strCommandLine.length() + 1]);

		std::memcpy(pCommandLine.get(), strCommandLine.c_str(), sizeof(TCHAR) * (strCommandLine.length() + 1));

		if (!CreateProcess(nullptr, pCommandLine.get(), nullptr, nullptr, true, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &startupInfo, &processInformation))
		{
			return TEXT("Failed to execute command \"") + std::tstring(pCommandLine.get()) + TEXT("\".");
		}

		hWritePipe.reset();

		std::string result;

		char buffer[128];

		std::memset(buffer, 0, sizeof(buffer));

		DWORD dwBytesRead = 0;

		while (ReadFile(*hReadPipe, buffer, 127, &dwBytesRead, nullptr))
		{
			result += buffer;

			std::memset(buffer, 0, sizeof(buffer));
		}
		
		Base::Handle<HANDLE> hProcess(processInformation.hProcess, CloseHandle);

		Base::Handle<HANDLE> hThread(processInformation.hThread, CloseHandle);

		WaitForSingleObject(*hProcess, INFINITE);

		if (lpExitCode)
		{
			GetExitCodeProcess(*hProcess, lpExitCode);
		}

		return std::tstring(result.begin(), result.end());
	}
}
