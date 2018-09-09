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

#include "process.hpp"

bool enableDebugPrivilege()
{
	HANDLE hProcess = GetCurrentProcess();

	HANDLE hTokenTmp = nullptr;

	if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hTokenTmp))
	{
		return false;
	}

	Deleter<HANDLE> hToken(hTokenTmp, CloseHandle);

	LUID luid;

	if (!LookupPrivilegeValue(0, SE_DEBUG_NAME, &luid))
	{
		return false;
	}

	TOKEN_PRIVILEGES privileges;
	privileges.PrivilegeCount = 1;
	privileges.Privileges->Luid = luid;
	privileges.Privileges->Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, false, &privileges, 0, 0, 0))
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

ProcessInfo::ProcessInfo(DWORD dwId, const std::tstring& strName, const Architecture& architecture) : dwId(dwId), strName(strName), architecture(architecture)
{

}

bool ProcessInfo::operator==(const ProcessInfo& other) const
{
	return this->dwId == other.dwId && this->strName == other.strName && this->architecture == other.architecture;
}

bool ProcessInfo::operator!=(const ProcessInfo& other) const
{
	return !this->operator==(other);
}

bool ProcessInfo::operator<(const ProcessInfo& other) const
{
	if (toLowerCase(this->strName) != toLowerCase(other.strName))
	{
		return toLowerCase(this->strName) < toLowerCase(other.strName);
	}

	if (this->architecture != other.architecture)
	{
		return this->architecture < other.architecture;
	}

	return this->dwId < other.dwId;
}

bool ProcessInfo::operator>(const ProcessInfo& other) const
{
	if (toLowerCase(this->strName) != toLowerCase(other.strName))
	{
		return toLowerCase(this->strName) > toLowerCase(other.strName);
	}

	if (this->architecture != other.architecture)
	{
		return this->architecture > other.architecture;
	}

	return this->dwId > other.dwId;
}

bool ProcessInfo::operator<=(const ProcessInfo& other) const
{
	return this->operator==(other) || this->operator<(other);
}

bool ProcessInfo::operator>=(const ProcessInfo& other) const
{
	return this->operator==(other) || this->operator>(other);
}

std::vector<ProcessInfo> enumerateProcesses()
{
	std::vector<ProcessInfo> processes;

	Deleter<HANDLE> hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0), CloseHandle, INVALID_HANDLE_VALUE);

	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		return processes;
	}

	PROCESSENTRY32 processEntry;

	ZeroMemory(&processEntry, sizeof(processEntry));

	processEntry.dwSize = sizeof(processEntry);

	if (Process32First(hSnapshot, &processEntry))
	{
		do
		{
			Deleter<HANDLE> hProcess(OpenProcess(PROCESS_ALL_ACCESS, 0, processEntry.th32ProcessID), CloseHandle);

			if (hProcess)
			{
				Architecture architecture = getProcessArchitecture(hProcess);

				processes.push_back(ProcessInfo(processEntry.th32ProcessID, processEntry.szExeFile, architecture));
			}
		} while (Process32Next(hSnapshot, &processEntry));
	}

	std::sort(processes.begin(), processes.end());

	return processes;
}

std::tstring executeCommand(const std::tstring& strCommandLine, LPDWORD lpExitCode)
{
	HANDLE hReadPipeTmp = nullptr;

	HANDLE hWritePipeTmp = nullptr;

	SECURITY_ATTRIBUTES securityAttributes;

	ZeroMemory(&securityAttributes, sizeof(securityAttributes));

	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.bInheritHandle = true;

	if (!CreatePipe(&hReadPipeTmp, &hWritePipeTmp, &securityAttributes, 4096))
	{
		return TEXT("Failed to create pipe.");
	}

	Deleter<HANDLE> hReadPipe(hReadPipeTmp, CloseHandle);

	Deleter<HANDLE> hWritePipe(hWritePipeTmp, CloseHandle);

	STARTUPINFO startupInfo;

	ZeroMemory(&startupInfo, sizeof(startupInfo));

	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = hWritePipe;
	startupInfo.hStdError = hWritePipe;

	PROCESS_INFORMATION processInformation;

	std::shared_ptr<TCHAR> pCommandLine(new TCHAR[strCommandLine.length() + 1]);

	std::memcpy(pCommandLine.get(), strCommandLine.c_str(), sizeof(TCHAR) * (strCommandLine.length() + 1));

	if (!CreateProcess(nullptr, pCommandLine.get(), nullptr, nullptr, true, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &startupInfo, &processInformation))
	{
		return TEXT("Failed to execute command \"") + std::tstring(pCommandLine.get()) + TEXT("\".");
	}

	hWritePipe.release();

	std::string strResult;

	char szBuffer[128];

	std::memset(szBuffer, 0, sizeof(szBuffer));

	DWORD dwBytesRead = 0;

	while (ReadFile(hReadPipe, szBuffer, 127, &dwBytesRead, nullptr))
	{
		strResult += szBuffer;

		std::memset(szBuffer, 0, sizeof(szBuffer));
	}

	Deleter<HANDLE> hProcess(processInformation.hProcess, CloseHandle);

	Deleter<HANDLE> hThread(processInformation.hThread, CloseHandle);

	WaitForSingleObject(hProcess, INFINITE);

	if (lpExitCode)
	{
		GetExitCodeProcess(hProcess, lpExitCode);
	}

	return std::tstring(strResult.begin(), strResult.end());
}

bool injectDLL(DWORD dwProcessId, const std::tstring& strPath, const std::tstring& strFunction, HWND hWnd)
{
	std::tstringstream stream;

	stream << TEXT("inject.exe ") << dwProcessId << TEXT(" \"") << strPath << TEXT("\"");

	if (strFunction != TEXT(""))
	{
		stream << TEXT(" \"") << strFunction << TEXT("\"");
	}

	DWORD dwExitCode = EXIT_FAILURE;

	std::tstring strStdOut = executeCommand(stream.str(), &dwExitCode);

	if (dwExitCode != EXIT_SUCCESS)
	{
		MessageBox(hWnd, strStdOut.c_str(), TEXT("Error"), MB_ICONERROR | MB_OK);

		return false;
	}

	return true;
}
