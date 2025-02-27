/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
Copyright (c) 2007 Artem Shpynov
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

typedef HRESULT(STDAPICALLTYPE* pfnDrawThemeTextEx)(HTHEME, HDC, int, int, LPCWSTR, int, uint32_t, LPRECT, const struct _DTTOPTS*);
static pfnDrawThemeTextEx drawThemeTextEx;

typedef HRESULT(STDAPICALLTYPE* pfnSetWindowThemeAttribute)(HWND, enum WINDOWTHEMEATTRIBUTETYPE, PVOID, uint32_t);
static pfnSetWindowThemeAttribute setWindowThemeAttribute;

typedef HRESULT(STDAPICALLTYPE* pfnDwmExtendFrameIntoClientArea)(HWND hwnd, const MARGINS* margins);
static pfnDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;

typedef HRESULT(STDAPICALLTYPE* pfnDwmIsCompositionEnabled)(BOOL*);
static pfnDwmIsCompositionEnabled dwmIsCompositionEnabled;

BOOL IsAeroMode()
{
	BOOL result;
	return dwmIsCompositionEnabled && (dwmIsCompositionEnabled(&result) == S_OK) && result;
}

static BOOL IsVSMode()
{
	return IsWinVerVistaPlus() && IsThemeActive();
}

////////////////////////////////////////////////////////////////////////////////////
// Internals

// structure is used for storing list of tab info
struct MHeaderbarCtrl : public MZeroedObject
{
	MHeaderbarCtrl() {}
	~MHeaderbarCtrl() { mir_free(controlsToRedraw); }

	HWND hwnd;

	// UI info
	RECT rc;
	int width, height;
	HICON hIcon;

	// control colors
	RGBQUAD rgbBkgTop, rgbBkgBottom;
	COLORREF clText;

	int nControlsToRedraw;
	HWND *controlsToRedraw;

	// fonts
	HFONT hFont;
};

static void MHeaderbar_SetupColors(MHeaderbarCtrl *dat)
{
	COLORREF cl = GetSysColor(COLOR_WINDOW);
	dat->rgbBkgBottom.rgbRed = (dat->rgbBkgTop.rgbRed = GetRValue(cl)) * .95;
	dat->rgbBkgBottom.rgbBlue = (dat->rgbBkgTop.rgbBlue = GetBValue(cl)) * .95;
	dat->rgbBkgBottom.rgbGreen = (dat->rgbBkgTop.rgbGreen = GetGValue(cl)) * .95;

	dat->clText = GetSysColor(COLOR_WINDOWTEXT);
	if (!dat->hFont)
		dat->hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static void MHeaderbar_FillRect(HDC hdc, int x, int y, int width, int height, COLORREF cl)
{
	int oldMode = SetBkMode(hdc, OPAQUE);
	COLORREF oldColor = SetBkColor(hdc, cl);

	RECT rc; SetRect(&rc, x, y, x+width, y+height);
	ExtTextOutA(hdc, 0, 0, ETO_OPAQUE, &rc, "", 0, nullptr);

	SetBkMode(hdc, oldMode);
	SetBkColor(hdc, oldColor);
}

static void MHeaderbar_DrawGradient(HDC hdc, int x, int y, int width, int height, RGBQUAD *rgb0, RGBQUAD *rgb1)
{
	int oldMode = SetBkMode(hdc, OPAQUE);
	COLORREF oldColor = SetBkColor(hdc, 0);

	RECT rc; SetRect(&rc, x, 0, x + width, 0);
	for (int i = y + height; --i >= y;) {
		COLORREF color = RGB(
			((height - i - 1)*rgb0->rgbRed + i*rgb1->rgbRed) / height,
			((height - i - 1)*rgb0->rgbGreen + i*rgb1->rgbGreen) / height,
			((height - i - 1)*rgb0->rgbBlue + i*rgb1->rgbBlue) / height);
		rc.top = rc.bottom = i;
		++rc.bottom;
		SetBkColor(hdc, color);
		ExtTextOutA(hdc, 0, 0, ETO_OPAQUE, &rc, "", 0, nullptr);
	}

	SetBkMode(hdc, oldMode);
	SetBkColor(hdc, oldColor);
}

static LRESULT MHeaderbar_OnPaint(HWND hwndDlg, MHeaderbarCtrl *mit)
{
	bool bUseAero = IsAeroMode() && !IsWinVer10Plus();
	int iTopSpace = bUseAero ? 0 : 3;
	PAINTSTRUCT ps;

	int titleLength = GetWindowTextLength(hwndDlg) + 1;
	wchar_t *szTitle = (wchar_t *)mir_alloc(sizeof(wchar_t) * titleLength);
	GetWindowText(hwndDlg, szTitle, titleLength);

	wchar_t *szSubTitle = wcschr(szTitle, '\n');
	if (szSubTitle)
		*szSubTitle++ = 0;

	HDC hdc = BeginPaint(hwndDlg, &ps);
	HDC tempDC = CreateCompatibleDC(hdc);

	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = mit->width;
	bmi.bmiHeader.biHeight = -mit->height; // we need this for DrawThemeTextEx
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBmp = CreateDIBSection(tempDC, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);

	HBITMAP hOldBmp = (HBITMAP)SelectObject(tempDC, hBmp);

	if (bUseAero) {
		RECT temprc = { 0, 0, mit->width, mit->width };
		FillRect(tempDC, &temprc, (HBRUSH)GetStockObject(BLACK_BRUSH));

		MARGINS margins = { 0, 0, mit->height, 0 };
		dwmExtendFrameIntoClientArea(GetParent(hwndDlg), &margins);

		WTA_OPTIONS opts;
		opts.dwFlags = opts.dwMask = WTNCA_NOSYSMENU | WTNCA_NODRAWICON;
		setWindowThemeAttribute(GetParent(hwndDlg), WTA_NONCLIENT, &opts, sizeof(opts));
	}
	else {
		if (IsVSMode())
			MHeaderbar_FillRect(tempDC, 0, 0, mit->width, mit->height, GetSysColor(COLOR_WINDOW));
		else
			MHeaderbar_DrawGradient(tempDC, 0, 0, mit->width, mit->height, &mit->rgbBkgTop, &mit->rgbBkgBottom);

		MHeaderbar_FillRect(tempDC, 0, mit->height-2, mit->width, 1, GetSysColor(COLOR_BTNSHADOW));
		MHeaderbar_FillRect(tempDC, 0, mit->height-1, mit->width, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
	}

	HFONT hFont = mit->hFont;
	SetBkMode(tempDC, TRANSPARENT);
	SetTextColor(tempDC, mit->clText);

	LOGFONT lf;
	GetObject(hFont, sizeof(lf), &lf);
	lf.lfWeight = FW_BOLD;
	HFONT hFntBold = CreateFontIndirect(&lf), hOldFont;

	if (mit->hIcon)
		DrawIcon(tempDC, 10, iTopSpace, mit->hIcon);
	else {
		HICON hIcon = (HICON)SendMessage(GetParent(hwndDlg), WM_GETICON, ICON_BIG, 0);
		if (hIcon == nullptr)
			hIcon = (HICON)SendMessage(GetParent(hwndDlg), WM_GETICON, ICON_SMALL, 0);
		DrawIcon(tempDC, 10, iTopSpace, hIcon);
	}

	RECT textRect;
	textRect.left = 50;
	textRect.right = mit->width;
	textRect.top = 2 + iTopSpace;
	textRect.bottom = g_iIconY - 2 + iTopSpace;

	if (bUseAero) {
		DTTOPTS dto = { 0 };
		dto.dwSize = sizeof(dto);
		dto.dwFlags = DTT_COMPOSITED | DTT_GLOWSIZE;
		dto.iGlowSize = 10;

		HANDLE hTheme = OpenThemeData(hwndDlg, L"Window");
		textRect.left = 50;
		hOldFont = (HFONT)SelectObject(tempDC, hFntBold);

		drawThemeTextEx(hTheme, tempDC, WP_CAPTION, CS_ACTIVE, szTitle, -1, DT_TOP | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_END_ELLIPSIS, &textRect, &dto);

		if (szSubTitle) {
			textRect.left = 66;
			SelectObject(tempDC, hFont);

			drawThemeTextEx(hTheme, tempDC, WP_CAPTION, CS_ACTIVE, szSubTitle, -1, DT_BOTTOM | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_END_ELLIPSIS, &textRect, &dto);
		}
		CloseThemeData(hTheme);
	}
	else {
		textRect.left = 50;
		hOldFont = (HFONT)SelectObject(tempDC, hFntBold);
		DrawText(tempDC, szTitle, -1, &textRect, DT_TOP | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_END_ELLIPSIS);

		if (szSubTitle) {
			textRect.left = 66;
			SelectObject(tempDC, hFont);
			DrawText(tempDC, szSubTitle, -1, &textRect, DT_BOTTOM | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_END_ELLIPSIS);
		}
	}

	DeleteObject(hFntBold);

	mir_free(szTitle);

	//Copy to output
	if (mit->nControlsToRedraw) {
		RECT temprc;
		temprc.left = 0;
		temprc.right = mit->width;
		temprc.top = 0;
		temprc.bottom = mit->width;
		HRGN hRgn = CreateRectRgnIndirect(&temprc);

		for (int i = 0; i < mit->nControlsToRedraw; i++) {
			GetWindowRect(mit->controlsToRedraw[i], &temprc);
			MapWindowPoints(nullptr, hwndDlg, (LPPOINT)&temprc, 2);
			HRGN hRgnTmp = CreateRectRgnIndirect(&temprc);
			CombineRgn(hRgn, hRgn, hRgnTmp, RGN_DIFF);
			DeleteObject(hRgnTmp);
		}
		SelectClipRgn(hdc, hRgn);
		DeleteObject(hRgn);
	}

	BitBlt(hdc, mit->rc.left, mit->rc.top, mit->width, mit->height, tempDC, 0, 0, SRCCOPY);

	SelectClipRgn(hdc, nullptr);

	SelectObject(tempDC, hOldBmp);
	DeleteObject(hBmp);
	SelectObject(tempDC, hOldFont);
	DeleteDC(tempDC);

	EndPaint(hwndDlg, &ps);

	return TRUE;
}

static LRESULT CALLBACK MHeaderbarWndProc(HWND hwndDlg, UINT  msg, WPARAM wParam, LPARAM lParam)
{
	MHeaderbarCtrl* itc = (MHeaderbarCtrl *)GetWindowLongPtr(hwndDlg, 0);
	switch (msg) {
	case WM_NCCREATE:
		itc = new MHeaderbarCtrl; //(MHeaderbarCtrl*)mir_alloc(sizeof(MHeaderbarCtrl));

		SetWindowLongPtr(hwndDlg, 0, (LONG_PTR)itc);
		MHeaderbar_SetupColors(itc);

		{
			HWND hParent = GetParent(hwndDlg);
			RECT rcWnd; GetWindowRect(hwndDlg, &rcWnd);
			itc->controlsToRedraw = nullptr;
			itc->nControlsToRedraw = 0;
			for (HWND hChild = FindWindowEx(hParent, nullptr, nullptr, nullptr); hChild; hChild = FindWindowEx(hParent, hChild, nullptr, nullptr)) {
				if (hChild != hwndDlg) {
					RECT rcChild; GetWindowRect(hChild, &rcChild);
					RECT rc;
					IntersectRect(&rc, &rcChild, &rcWnd);
					if (!IsRectEmpty(&rc)) {
						++itc->nControlsToRedraw;
						itc->controlsToRedraw = (HWND *)mir_realloc(itc->controlsToRedraw, sizeof(HWND) * itc->nControlsToRedraw);
						itc->controlsToRedraw[itc->nControlsToRedraw - 1] = hChild;
					}
				}
			}
		}

		break;

	case WM_SETFONT:
		itc->hFont = (HFONT)wParam;
		break;

	case WM_SIZE:
		GetClientRect(hwndDlg, &itc->rc);
		itc->width = itc->rc.right - itc->rc.left;
		itc->height = itc->rc.bottom - itc->rc.top;
		return TRUE;

	case WM_THEMECHANGED:
	case WM_STYLECHANGED:
		MHeaderbar_SetupColors(itc);
		return TRUE;

	case WM_LBUTTONDOWN:
		SendMessage(GetParent(hwndDlg), WM_SYSCOMMAND, 0xF012, 0);
		return 0;

	case WM_SETICON:
		if (wParam < 3) {
			itc->hIcon = (HICON)lParam;
			InvalidateRect(hwndDlg, nullptr, FALSE);
		}
		break;

	case WM_ERASEBKGND:
		return 1;

	case WM_NCPAINT:
		InvalidateRect(hwndDlg, nullptr, FALSE);
		break;

	case WM_PAINT:
		MHeaderbar_OnPaint(hwndDlg, itc);
		break;

	case WM_DESTROY:
		delete itc;
		break;
	}
	return DefWindowProc(hwndDlg, msg, wParam, lParam);
}

int LoadHeaderbarModule()
{
	if (IsWinVerVistaPlus()) {
		HINSTANCE hThemeAPI = LoadLibraryA("uxtheme.dll");
		if (hThemeAPI) {
			drawThemeTextEx = (pfnDrawThemeTextEx)GetProcAddress(hThemeAPI, "DrawThemeTextEx");
			setWindowThemeAttribute = (pfnSetWindowThemeAttribute)GetProcAddress(hThemeAPI, "SetWindowThemeAttribute");
		}

		HINSTANCE hDwmApi = LoadLibrary(L"dwmapi.dll");
		if (hDwmApi) {
			dwmExtendFrameIntoClientArea = (pfnDwmExtendFrameIntoClientArea)GetProcAddress(hDwmApi, "DwmExtendFrameIntoClientArea");
			dwmIsCompositionEnabled = (pfnDwmIsCompositionEnabled)GetProcAddress(hDwmApi, "DwmIsCompositionEnabled");
		}
	}

	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = L"MHeaderbarCtrl";
	wc.lpfnWndProc = MHeaderbarWndProc;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.cbWndExtra = sizeof(MHeaderbarCtrl*);
	wc.style = CS_GLOBALCLASS | CS_SAVEBITS;
	RegisterClassEx(&wc);
	return 0;
}
