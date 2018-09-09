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

#pragma once

#include <Windows.h>
#include <tchar.h>

#include <string>
#include <fstream>
#include <sstream>
#include <functional>

#include <iostream>

#include <vector>
#include <memory>
#include <algorithm>

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

extern std::vector<std::tstring> getArgs();

extern std::tstring toLowerCase(std::tstring str);

extern std::tstring toUpperCase(std::tstring str);

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

template <typename T>
class Deleter
{
public:
	Deleter(const T& invalid = T(0)) : invalid(invalid)
	{

	}

	template <typename DeleteFunctor>
	Deleter(const T& t, DeleteFunctor deleteFunctor, const T& invalid = T(0)) : invalid(invalid)
	{
		if (t != invalid)
		{
			T* pT = new T(t);

			std::function<void(T*)> deleter([=](T* pT)
			{
				if (pT)
				{
					if (*pT != invalid)
					{
						deleteFunctor(*pT);
					}
				}

				delete pT;
			});

			this->t = std::shared_ptr<T>(pT, deleter);
		}
	}

	operator T () const
	{
		if (this->t)
		{
			return *this->t;
		}

		return this->invalid;
	}

	void release()
	{
		this->t.reset();
	}

private:
	std::shared_ptr<T> t;

	T invalid;
};
