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

#include "base.hpp"

namespace std
{
	#ifdef UNICODE
	std::wostream& tcout = wcout;
	#else
	std::ostream& tcout = cout;
	#endif
}

std::vector<std::tstring> getArgs()
{
	std::vector<std::tstring> args;

	int iArgc = 0;

	std::shared_ptr<LPWSTR> lpArgv(CommandLineToArgvW(GetCommandLineW(), &iArgc), LocalFree);

	if (lpArgv)
	{
		for (int i = 0; i < iArgc; i++)
		{
			std::wstring arg(lpArgv.get()[i]);

			args.push_back(std::tstring(arg.begin(), arg.end()));
		}
	}

	return args;
}

std::tstring toLowerCase(std::tstring str)
{
	std::transform(str.begin(), str.end(), str.begin(), tolower);

	return str;
}

std::tstring toUpperCase(std::tstring str)
{
	std::transform(str.begin(), str.end(), str.begin(), toupper);

	return str;
}
