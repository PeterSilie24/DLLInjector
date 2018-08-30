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

#pragma once

#include <Windows.h>
#include <tchar.h>

#include <string>
#include <fstream>
#include <sstream>

#include <iostream>

#include <vector>
#include <memory>

namespace std
{
	#ifdef UNICODE
	extern std::wostream& tcout;
	#else
	extern std::ostream& tcout;
	#endif

	typedef basic_string<TCHAR>         tstring;

	typedef basic_ostream<TCHAR>        tostream;
	typedef basic_istream<TCHAR>        tistream;
	typedef basic_iostream<TCHAR>       tiostream;

	typedef basic_ifstream<TCHAR>       tifstream;
	typedef basic_ofstream<TCHAR>       tofstream;
	typedef basic_fstream<TCHAR>        tfstream;

	typedef basic_stringstream<TCHAR>   tstringstream;
}

namespace Base
{
	std::vector<std::tstring> getArgs();

	template <typename T>
	T fromStr(const std::tstring& str)
	{
		std::tstringstream stream(str);

		T t = T();

		stream >> t;

		return t;
	}

	template <typename T>
	std::tstring toStr(const T& t)
	{
		std::tstringstream stream;

		stream << t;

		return stream.str();
	}

	template <typename THandle>
	class Handle : public std::shared_ptr<THandle>
	{
	public:
		Handle()
		{

		}

		template <typename Deleter>
		Handle(const THandle& handle, Deleter deleter)
			: std::shared_ptr<THandle>([=]()
		{
			if (handle)
			{
				return new THandle(handle);
			}

			return reinterpret_cast<THandle*>(nullptr);
		} (),
			[=](THandle* pHandle)
		{
			if (pHandle)
			{
				if (*pHandle)
				{
					deleter(*pHandle);
				}

				delete pHandle;
			}
		})
		{

		}
	};
}
