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

#include <Windows.h>
#include <Windowsx.h>
#include <commctrl.h>
#include <Richedit.h>
#include <psapi.h>
#include <Tlhelp32.h>
#include <Shlobj.h>

#include "resource.h"

#pragma comment (lib, "comctl32.lib")

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "../shared/process.hpp"

TCHAR szWndClassEx[MAX_PATH];
TCHAR szAppTitle[MAX_PATH];
TCHAR szFontFaceName[MAX_PATH];

TCHAR szColumnPID[MAX_PATH];
TCHAR szColumnPlatform[MAX_PATH];
TCHAR szColumnName[MAX_PATH];

HWND hWnd;

HFONT hFont;

#define IDE_DLL 100
HWND hEditDLL;
HWND hEditFunction;
#define IDB_BROWSE 101
HWND hButtonBrowse;

#define IDLV_PROCESSES 102
HWND hListViewProcesses;

#define IDE_PROCESS 103
HWND hEditProcess;
#define IDB_INJECT 104
HWND hButtonInject;

struct FileExtension
{
	FileExtension(const std::tstring& extension, const std::tstring& description) : extension(extension), description(description)
	{

	}

	std::tstring extension;
	std::tstring description;
};

std::tstring chooseFileToOpen(const std::tstring& strTitle, const std::vector<FileExtension>& vecExtensionFilter, std::size_t nDefaultExtension)
{
	TCHAR szBuffer[MAX_PATH];

	szBuffer[0] = TEXT('\0');

	std::vector<TCHAR> vecFilter;

	for (auto& pairExtension : vecExtensionFilter)
	{
		for (auto& c : pairExtension.description)
		{
			vecFilter.push_back(c);
		}

		vecFilter.push_back(TEXT('\0'));

		for (auto& c : pairExtension.extension)
		{
			vecFilter.push_back(c);
		}

		vecFilter.push_back(TEXT('\0'));
	}

	vecFilter.push_back(TEXT('\0'));

	if (!vecExtensionFilter.size())
	{
		vecFilter.push_back(TEXT('\0'));
	}

	std::tstring strDefaultExtension;

	if (nDefaultExtension < vecExtensionFilter.size())
	{
		strDefaultExtension = vecExtensionFilter[nDefaultExtension].extension;
	}

	OPENFILENAME openFileName;

	ZeroMemory(&openFileName, sizeof(openFileName));

	openFileName.lStructSize = sizeof(openFileName);
	openFileName.hwndOwner = hWnd;
	openFileName.lpstrTitle = strTitle.c_str();
	openFileName.lpstrFilter = vecFilter.data();
	openFileName.lpstrDefExt = strDefaultExtension.c_str();
	openFileName.lpstrFile = szBuffer;
	openFileName.nMaxFile = MAX_PATH;
	openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName(&openFileName))
	{
		return szBuffer;
	}

	return TEXT("");
}

void insertProcessItem(int iItem, const ProcessInfo& processInfo)
{
	TCHAR szBuff[MAX_PATH];

	LV_ITEM lvItem;

	ZeroMemory(&lvItem, sizeof(lvItem));

	lvItem.mask = LVIF_TEXT;
	lvItem.cchTextMax = MAX_PATH;
	lvItem.iItem = iItem;
	lvItem.pszText = szBuff;

	lvItem.iSubItem = 0;

	std::tstring strPID = toStr(processInfo.dwId);

	_tcscpy_s(szBuff, strPID.c_str());

	ListView_InsertItem(hListViewProcesses, &lvItem);

	lvItem.iSubItem = 1;

	std::tstring strPlatform;

	if (processInfo.architecture == Architecture::Amd64)
	{
		strPlatform = TEXT("64 Bit");
	}
	else if (processInfo.architecture == Architecture::Intel)
	{
		strPlatform = TEXT("32 Bit");
	}

	_tcscpy_s(szBuff, strPlatform.c_str());

	ListView_SetItem(hListViewProcesses, &lvItem);

	lvItem.iSubItem = 2;

	_tcscpy_s(szBuff, processInfo.strName.c_str());

	ListView_SetItem(hListViewProcesses, &lvItem);
}

void showProcessItems()
{
	static std::vector<ProcessInfo> lastProcesses;

	std::vector<ProcessInfo> processes = enumerateProcesses();

	for (std::size_t i = 0; i < processes.size(); i++)
	{
		bool found = false;
		std::size_t insertIndex = lastProcesses.size();

		for (std::size_t j = 0; j < lastProcesses.size(); j++)
		{
			if (processes[i] == lastProcesses[j])
			{
				found = true;

				break;
			}
			else if (processes[i] < lastProcesses[j])
			{
				insertIndex = j;

				break;
			}
		}

		if (!found)
		{
			lastProcesses.insert(lastProcesses.begin() + insertIndex, processes[i]);

			insertProcessItem(static_cast<int>(insertIndex), processes[i]);
		}
	}

	for (std::size_t i = 0; i < lastProcesses.size(); )
	{
		bool found = false;

		for (std::size_t j = 0; j < processes.size(); j++)
		{
			if (processes[j] == lastProcesses[i])
			{
				found = true;

				break;
			}
		}

		if (!found)
		{
			lastProcesses.erase(lastProcesses.begin() + i);

			ListView_DeleteItem(hListViewProcesses, static_cast<int>(i));
		}
		else
		{
			i++;
		}
	}
}

LRESULT CALLBACK wndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	static bool editDllEmpty = true;
	static bool editProcessEmpty = true;

	switch (uiMsg)
	{
	case WM_TIMER:
	{
		showProcessItems();

		break;
	}
	case WM_NOTIFY:
	{
		UINT code = (reinterpret_cast<LPNMHDR>(lParam))->code;
		UINT_PTR id = (reinterpret_cast<LPNMHDR>(lParam))->idFrom;

		switch (code)
		{
		case NM_CLICK:
		case NM_RETURN:
		{
			switch (id)
			{
			case IDLV_PROCESSES:
			{
				int iSlected = static_cast<int>(SendMessage(hListViewProcesses, LVM_GETNEXTITEM, -1, LVNI_FOCUSED));

				TCHAR szProcessID[MAX_PATH];
				
				szProcessID[0] = TEXT('\0');

				LV_ITEM lvItem;

				ZeroMemory(&lvItem, sizeof(lvItem));

				lvItem.mask = LVIF_TEXT;
				lvItem.iSubItem = 0;
				lvItem.pszText = szProcessID;
				lvItem.cchTextMax = MAX_PATH;
				lvItem.iItem = iSlected;

				SendMessage(hListViewProcesses, LVM_GETITEMTEXT, iSlected, reinterpret_cast<LPARAM>(&lvItem));

				TCHAR szProcessName[MAX_PATH];

				szProcessName[0] = TEXT('\0');

				ZeroMemory(&lvItem, sizeof(lvItem));

				lvItem.mask = LVIF_TEXT;
				lvItem.iSubItem = 2;
				lvItem.pszText = szProcessName;
				lvItem.cchTextMax = MAX_PATH;
				lvItem.iItem = iSlected;

				SendMessage(hListViewProcesses, LVM_GETITEMTEXT, iSlected, reinterpret_cast<LPARAM>(&lvItem));

				std::tstring strProcessInfo = std::tstring(szProcessID) + TEXT(" - ") + szProcessName;

				Edit_SetText(hEditProcess, strProcessInfo.c_str());

				break;
			}
			}

			break;
		}
		}

		break;
	}
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDB_BROWSE:
			{
				std::tstring path = chooseFileToOpen(TEXT("Choose a DLL"), { FileExtension(TEXT("*.dll"), TEXT("Dynamic Link Library (*.dll)")), FileExtension(TEXT("*.*"), TEXT("All Files")) }, 0);

				if (path != TEXT(""))
				{
					Edit_SetText(hEditDLL, path.c_str());
				}

				break;
			}
			case IDB_INJECT:
			{
				TCHAR szDLL[MAX_PATH];

				szDLL[0] = TEXT('\0');

				Edit_GetText(hEditDLL, szDLL, MAX_PATH);

				TCHAR szFunction[MAX_PATH];

				szFunction[0] = TEXT('\0');

				Edit_GetText(hEditFunction, szFunction, MAX_PATH);

				TCHAR szProcess[MAX_PATH];

				szProcess[0] = TEXT('\0');

				Edit_GetText(hEditProcess, szProcess, MAX_PATH);

				DWORD dwPID = 0;

				std::tstringstream stream(szProcess);

				stream >> dwPID;

				injectDLL(dwPID, szDLL, szFunction, hWnd);

				break;
			}
			}

			break;
		}
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDE_DLL:
			{
				editDllEmpty = Edit_GetTextLength(hEditDLL) <= 0;

				break;
			}
			case IDE_PROCESS:
			{
				editProcessEmpty = Edit_GetTextLength(hEditProcess) <= 0;

				break;
			}
			}

			Edit_Enable(hButtonInject, !editDllEmpty && !editProcessEmpty);

			break;
		}
		}
		break;
	}
	case WM_CLOSE:
	{
		DestroyWindow(hWnd);

		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(EXIT_SUCCESS);

		break;
	}
	}

	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int result = EXIT_FAILURE;

	MSG msg;

	LoadLibrary(TEXT("Msftedit.dll"));

	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	LoadString(hInstance, IDS_WND_CLASS_EX, szWndClassEx, MAX_PATH);
	LoadString(hInstance, IDS_APP_TITLE, szAppTitle, MAX_PATH);
	LoadString(hInstance, IDS_FONT_FACE_NAME, szFontFaceName, MAX_PATH);

	LoadString(hInstance, IDS_COLUMN_PID, szColumnPID, MAX_PATH);
	LoadString(hInstance, IDS_COLUMN_PLATFORM, szColumnPlatform, MAX_PATH);
	LoadString(hInstance, IDS_COLUMN_NAME, szColumnName, MAX_PATH);

	DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	int width = 480;
	int height = 360;

	RECT rect;
	rect.left = 0;
	rect.right = width;
	rect.top = 0;
	rect.bottom = height;

	AdjustWindowRect(&rect, dwStyle, false);

	WNDCLASSEX wndClassEx;

	ZeroMemory(&wndClassEx, sizeof(wndClassEx));

	wndClassEx.cbSize = sizeof(wndClassEx);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	wndClassEx.lpfnWndProc = wndProc;
	wndClassEx.lpszClassName = szWndClassEx;
	wndClassEx.lpszMenuName = nullptr;
	wndClassEx.cbClsExtra = 0;
	wndClassEx.cbWndExtra = 0;
	wndClassEx.hInstance = hInstance;
	wndClassEx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
	wndClassEx.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
	wndClassEx.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndClassEx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);

	if (FAILED(RegisterClassEx(&wndClassEx)))
	{
		MessageBox(nullptr, TEXT("Failed to register window class."), TEXT("Error"), MB_ICONERROR | MB_OK);

		goto exit;
	}

	hWnd = CreateWindowEx(0, szWndClassEx, szAppTitle, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, HWND_DESKTOP, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		MessageBox(nullptr, TEXT("Failed to create window."), TEXT("Error"), MB_ICONERROR | MB_OK);

		goto exit;
	}

	hFont = CreateFont(15, 0, 0, 0, FW_DONTCARE, false, false, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, szFontFaceName);

	hEditDLL = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, TEXT(""), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 10, width -10 - 10 - 130 - 10 - 75 - 10, 20, hWnd, reinterpret_cast<HMENU>(IDE_DLL), hInstance, nullptr);

	Edit_SetCueBannerText(hEditDLL, TEXT("Enter DLL path ..."));

	SendMessage(hEditDLL, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	hEditFunction = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, TEXT(""), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, width - 130 - 10 - 75 - 10, 10, 130, 20, hWnd, nullptr, hInstance, nullptr);

	Edit_SetCueBannerText(hEditFunction, TEXT("Function (optional)"));

	SendMessage(hEditFunction, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	hButtonBrowse = CreateWindow(WC_BUTTON, TEXT("Browse"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, width - 75 - 10, 10 - 1, 75, 20 + 2, hWnd, reinterpret_cast<HMENU>(IDB_BROWSE), hInstance, nullptr);

	SendMessage(hButtonBrowse, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	hListViewProcesses = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, TEXT("List View"), WS_CHILD | WS_VISIBLE | LVS_REPORT, 10, 40, width - 10 - 10, height - 10 - 20 - 10 - 10 - 20 - 10, hWnd, reinterpret_cast<HMENU>(IDLV_PROCESSES), hInstance, nullptr);

	SendMessage(hListViewProcesses, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	SendMessage(hListViewProcesses, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

	LV_COLUMN lvColumn;

	ZeroMemory(&lvColumn, sizeof(lvColumn));

	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	lvColumn.pszText = szColumnPID;
	lvColumn.cx = 50;

	ListView_InsertColumn(hListViewProcesses, 0, &lvColumn);

	lvColumn.pszText = szColumnPlatform;
	lvColumn.cx = 70;

	ListView_InsertColumn(hListViewProcesses, 1, &lvColumn);

	lvColumn.pszText = szColumnName;
	lvColumn.cx = width - 165;

	ListView_InsertColumn(hListViewProcesses, 2, &lvColumn);

	hEditProcess = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, TEXT(""), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, height - 30, width - 10 - 10 - 75 - 10, 20, hWnd, reinterpret_cast<HMENU>(IDE_PROCESS), hInstance, nullptr);

	Edit_SetCueBannerText(hEditProcess, TEXT("Choose process ..."));

	SendMessage(hEditProcess, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	hButtonInject = CreateWindow(WC_BUTTON, TEXT("Inject"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, width - 75 - 10, height - 30 - 1, 75, 20 + 2, hWnd, reinterpret_cast<HMENU>(IDB_INJECT), hInstance, nullptr);

	SendMessage(hButtonInject, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);

	Button_Enable(hButtonInject, false);

	SetTimer(hWnd, 0, 1000, nullptr);

	enableDebugPrivilege();

	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	SendMessage(hWnd, WM_TIMER, 0, 0);

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    result = static_cast<int>(msg.wParam);

exit:;

	DeleteObject(hFont);

	return result;
}
