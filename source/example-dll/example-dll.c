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

#include "example-dll.h"

DWORD dwProcessId = 0;

BOOL CALLBACK enumWindowsProc(HWND hWnd, LPARAM lParam)
{
	HWND* hWndMain = (HWND)lParam;

	if (hWndMain)
	{
		DWORD dwWndProcessId = 0;

		GetWindowThreadProcessId(hWnd, &dwWndProcessId);

		HWND hWndOwner = GetWindow(hWnd, GW_OWNER);

		if (dwProcessId != dwWndProcessId || hWndOwner != HWND_DESKTOP || !IsWindowVisible(hWnd))
		{
			return TRUE;
		}

		*hWndMain = hWnd;
	}

	return FALSE;
}

void messageBoxLoop()
{
	if (!dwProcessId)
	{
		dwProcessId = GetCurrentProcessId();
	}

	HWND hWnd = HWND_DESKTOP;

	EnumWindows((WNDENUMPROC)enumWindowsProc, (LPARAM)(&hWnd));

	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		MessageBox(hWnd, TEXT("DLL was injected into this process!"), TEXT("DLL Injector"), MB_OK | MB_ICONINFORMATION);
	}
}

void message()
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)messageBoxLoop, NULL, 0, NULL);
}

void beep()
{
	Beep(1000, 1000);
}
