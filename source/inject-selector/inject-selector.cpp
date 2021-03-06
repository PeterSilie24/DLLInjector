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

	Deleter<HANDLE> hProcess(OpenProcess(PROCESS_ALL_ACCESS, false, dwProcessId), CloseHandle);

	if (!hProcess)
	{
		std::tcout << TEXT("Failed to open the process width id ") << dwProcessId << "." << std::endl;

		return EXIT_FAILURE;
	}

	Architecture architecture = getProcessArchitecture(hProcess);

	std::tstring result;

	DWORD dwExitCode = EXIT_FAILURE;

	if (architecture == Architecture::Amd64)
	{
		result = executeCommand(TEXT("inject-x64.exe ") + args[1] + TEXT(" \"") + args[2] + TEXT("\"") + ((args.size() > 3) ? (TEXT(" \"") + args[3] + TEXT("\"")) : TEXT("")), &dwExitCode);
	}
	else if (architecture == Architecture::Intel)
	{
		result = executeCommand(TEXT("inject-x86.exe ") + args[1] + TEXT(" \"") + args[2] + TEXT("\"") + ((args.size() > 3) ? (TEXT(" \"") + args[3] + TEXT("\"")) : TEXT("")), &dwExitCode);
	}

	std::tcout << result << std::flush;

    return dwExitCode;
}
