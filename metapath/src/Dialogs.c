/******************************************************************************
*
*
* metapath - The universal Explorer-like Plugin
*
* Dialogs.c
*   metapath dialog boxes implementation
*
* See Readme.txt for more information about this source code.
* Please send me your comments to this work.
*
* See License.txt for details about distribution and modification.
*
*                                              (c) Florian Balmer 1996-2011
*                                                  florian.balmer@gmail.com
*                                               http://www.flos-freeware.ch
*
*
******************************************************************************/

#define OEMRESOURCE  // use OBM_ resource constants
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include "Helpers.h"
#include "metapath.h"
#include "Dlapi.h"
#include "Dialogs.h"
#include "resource.h"
#include "version.h"

//=============================================================================
//
//  ErrorMessage()
//
//  L"Title\nMessage Text"
//
extern HWND hwndMain;

int ErrorMessage(int iLevel, UINT uIdMsg, ...) {
	WCHAR szText[256 * 2];
	WCHAR szTitle[256 * 2];

	GetString(uIdMsg, szText, COUNTOF(szText));

	va_list va;
	va_start(va, uIdMsg);
	wvsprintf(szTitle, szText, va);
	va_end(va);

	WCHAR *c = StrChr(szTitle, L'\n');
	if (c) {
		lstrcpy(szText, (c + 1));
		*c = L'\0';
	} else {
		lstrcpy(szText, szTitle);
		lstrcpy(szTitle, L"");
	}

	HWND hwnd;
	if ((hwnd = GetActiveWindow()) == NULL) {
		hwnd = hwndMain;
	}

	const int iIcon = (iLevel > 1) ? MB_ICONEXCLAMATION : MB_ICONINFORMATION;
	PostMessage(hwndMain, APPM_CENTER_MESSAGE_BOX, (WPARAM)hwnd, 0);
	return MessageBoxEx(hwnd, szText, szTitle, MB_SETFOREGROUND | iIcon, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT));
}

//=============================================================================
//
// BFFCallBack()
//
static int CALLBACK BFFCallBack(HWND hwnd, UINT umsg, LPARAM lParam, LPARAM lpData) {
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(lpData);

	if (umsg == BFFM_INITIALIZED) {
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}

//=============================================================================
//
// GetDirectory()
//
BOOL GetDirectory(HWND hwndParent, int iTitle, LPWSTR pszFolder, LPCWSTR pszBase) {
	WCHAR szTitle[256];
	lstrcpy(szTitle, L"");
	GetString(iTitle, szTitle, COUNTOF(szTitle));

	WCHAR szBase[MAX_PATH];
	if (StrIsEmpty(pszBase)) {
		GetCurrentDirectory(MAX_PATH, szBase);
	} else {
		lstrcpy(szBase, pszBase);
	}

	BROWSEINFO bi;
	bi.hwndOwner = hwndParent;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = pszFolder;
	bi.lpszTitle = szTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BFFCallBack;
	bi.lParam = (LPARAM)szBase;
	bi.iImage = 0;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl) {
		SHGetPathFromIDList(pidl, pszFolder);
		CoTaskMemFree((LPVOID)pidl);
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// GetDirectory2()
//
BOOL GetDirectory2(HWND hwndParent, int iTitle, LPWSTR pszFolder, int iBase) {
	WCHAR szTitle[256];
	lstrcpy(szTitle, L"");
	GetString(iTitle, szTitle, COUNTOF(szTitle));

	LPITEMIDLIST pidlRoot;
	if (NOERROR != SHGetSpecialFolderLocation(hwndParent, iBase, &pidlRoot)) {
		CoTaskMemFree((LPVOID)pidlRoot);
		return FALSE;
	}

	BROWSEINFO bi;
	bi.hwndOwner = hwndParent;
	bi.pidlRoot = pidlRoot;
	bi.pszDisplayName = pszFolder;
	bi.lpszTitle = szTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = NULL;
	bi.lParam = 0;
	bi.iImage = 0;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	const BOOL fOk = pidl != NULL;
	if (fOk) {
		SHGetPathFromIDList(pidl, pszFolder);
		CoTaskMemFree((LPVOID)pidl);
	}

	CoTaskMemFree((LPVOID)pidlRoot);
	return fOk;
}

extern WCHAR szCurDir[MAX_PATH + 40];

//=============================================================================
//
//  RunDlgProc()
//
//
extern HWND hwndDirList;
extern int cxRunDlg;

INT_PTR CALLBACK RunDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		ResizeDlg_InitX(hwnd, cxRunDlg, IDC_RESIZEGRIP3);
		MakeBitmapButton(hwnd, IDC_SEARCHEXE, g_hInstance, IDB_OPEN);

		DLITEM dli;
		dli.mask = DLI_FILENAME;
		if (DirList_GetItem(hwndDirList, -1, &dli) != -1) {
			LPWSTR psz = GetFilenameStr(dli.szFileName);
			QuotateFilenameStr(psz);
			SetDlgItemText(hwnd, IDC_COMMANDLINE, psz);
		}

		SendDlgItemMessage(hwnd, IDC_COMMANDLINE, EM_LIMITTEXT, MAX_PATH - 1, 0);
		SHAutoComplete(GetDlgItem(hwnd, IDC_COMMANDLINE), SHACF_FILESYSTEM);
		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxRunDlg, NULL);
		DeleteBitmapButton(hwnd, IDC_SEARCHEXE);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RUNDESC, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_SEARCHEXE, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_COMMANDLINE, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
		InvalidateRect(GetDlgItem(hwnd, IDC_RUNDESC), NULL, TRUE);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SEARCHEXE: {
			WCHAR szArgs[MAX_PATH];
			WCHAR szArg2[MAX_PATH];
			WCHAR szFile[MAX_PATH * 2];

			GetDlgItemText(hwnd, IDC_COMMANDLINE, szArgs, COUNTOF(szArgs));
			ExpandEnvironmentStringsEx(szArgs, COUNTOF(szArgs));
			ExtractFirstArgument(szArgs, szFile, szArg2);

			WCHAR szTitle[32];
			WCHAR szFilter[256];
			GetString(IDS_SEARCHEXE, szTitle, COUNTOF(szTitle));
			GetString(IDS_FILTER_EXE, szFilter, COUNTOF(szFilter));
			PrepareFilterStr(szFilter);

			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = szFilter;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = COUNTOF(szFile);
			ofn.lpstrTitle = szTitle;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
						| OFN_PATHMUSTEXIST | OFN_SHAREAWARE | OFN_NODEREFERENCELINKS | OFN_NOVALIDATE;

			if (GetOpenFileName(&ofn)) {
				QuotateFilenameStr(szFile);

				if (StrNotEmpty(szArg2)) {
					lstrcat(szFile, L" ");
					lstrcat(szFile, szArg2);
				}
				SetDlgItemText(hwnd, IDC_COMMANDLINE, szFile);
			}
			PostMessage(hwnd, WM_NEXTDLGCTL, 1, 0);
		}
		break;

		case IDC_COMMANDLINE: {
			BOOL bEnableOK = FALSE;
			WCHAR args[MAX_PATH];

			if (GetDlgItemText(hwnd, IDC_COMMANDLINE, args, MAX_PATH)) {
				if (ExtractFirstArgument(args, args, NULL)) {
					if (StrNotEmpty(args)) {
						bEnableOK = TRUE;
					}
				}
			}
			EnableWindow(GetDlgItem(hwnd, IDOK), bEnableOK);
		}
		break;

		case IDOK: {
			WCHAR arg1[MAX_PATH];

			if (GetDlgItemText(hwnd, IDC_COMMANDLINE, arg1, MAX_PATH)) {
				WCHAR arg2[MAX_PATH];
				if (*arg1 == L'/') {
					EndDialog(hwnd, IDOK);
					// Call DisplayPath() when dialog ended
					ExtractFirstArgument(arg1 + 1, arg1, arg2);
					DisplayPath(arg1, IDS_ERR_CMDLINE);
				} else {
					ExpandEnvironmentStringsEx(arg1, COUNTOF(arg1));
					ExtractFirstArgument(arg1, arg1, arg2);

					SHELLEXECUTEINFO sei;
					ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
					sei.cbSize = sizeof(SHELLEXECUTEINFO);
					sei.fMask = 0;
					sei.hwnd = hwnd;
					sei.lpVerb = NULL;
					sei.lpFile = arg1;
					sei.lpParameters = arg2;
					sei.lpDirectory = szCurDir;
					sei.nShow = SW_SHOWNORMAL;

					if (ShellExecuteEx(&sei)) {
						EndDialog(hwnd, IDOK);
					} else {
						PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwnd, IDC_COMMANDLINE), 1);
					}
				}
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  RunDlg()
//
//
void RunDlg(HWND hwnd) {
	ThemedDialogBox(g_hInstance, MAKEINTRESOURCE(IDD_RUN), hwnd, RunDlgProc);
}

//=============================================================================
//
//  GotoDlgProc()
//
//
extern HISTORY mHistory;
extern int cxGotoDlg;

INT_PTR CALLBACK GotoDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		ResizeDlg_InitX(hwnd, cxGotoDlg, IDC_RESIZEGRIP);

		HWND hwndGoto = GetDlgItem(hwnd, IDC_GOTO);
		SendMessage(hwndGoto, CB_LIMITTEXT, MAX_PATH - 1, 0);
		SendMessage(hwndGoto, CB_SETEXTENDEDUI, TRUE, 0);

		for (int i = 0; i < HISTORY_ITEMS; i++) {
			if (mHistory.psz[i]) {
				const int iItem = (int)SendMessage(hwndGoto, CB_FINDSTRINGEXACT, (WPARAM)(-1), (LPARAM)mHistory.psz[i]);
				if (iItem != LB_ERR) {
					SendMessage(hwndGoto, CB_DELETESTRING, iItem, 0);
				}
				SendMessage(hwndGoto, CB_INSERTSTRING, 0, (LPARAM)mHistory.psz[i]);
			}
		}

		// from WinUser.h: GetComboBoxInfo() since Windows Vista, but CB_GETCOMBOBOXINFO since Windows XP.
		COMBOBOXINFO cbi;
		ZeroMemory(&cbi, sizeof(COMBOBOXINFO));
		cbi.cbSize = sizeof(COMBOBOXINFO);
		if (SendMessage(hwndGoto, CB_GETCOMBOBOXINFO, 0, (LPARAM)(&cbi))) {
			SHAutoComplete(cbi.hwndItem, SHACF_FILESYSTEM);
		}
		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxGotoDlg, NULL);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);

		HDWP hdwp = BeginDeferWindowPos(5);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_GOTO, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_GOTODESC, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);

		InvalidateRect(GetDlgItem(hwnd, IDC_GOTODESC), NULL, TRUE);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_GOTO:
			EnableWindow(GetDlgItem(hwnd, IDOK),
						 (GetWindowTextLength(GetDlgItem(hwnd, IDC_GOTO)) ||
						  CB_ERR != SendDlgItemMessage(hwnd, IDC_GOTO, CB_GETCURSEL, 0, 0)));

			if (HIWORD(wParam) == CBN_CLOSEUP) {
				LONG lSelEnd = 0;
				SendDlgItemMessage(hwnd, IDC_GOTO, CB_GETEDITSEL, 0, (LPARAM)&lSelEnd);
				SendDlgItemMessage(hwnd, IDC_GOTO, CB_SETEDITSEL, 0, MAKELPARAM(lSelEnd, lSelEnd));
			}
			break;

		case IDOK: {
			WCHAR tch[MAX_PATH];

			if (GetDlgItemText(hwnd, IDC_GOTO, tch, MAX_PATH)) {
				EndDialog(hwnd, IDOK);
				PathUnquoteSpaces(tch);
				DisplayPath(tch, IDS_ERR_CMDLINE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDOK),
							 (GetWindowTextLength(GetDlgItem(hwnd, IDC_GOTO)) ||
							  CB_ERR != SendDlgItemMessage(hwnd, IDC_GOTO, CB_GETCURSEL, 0, 0)));
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  GotoDlg()
//
//
void GotoDlg(HWND hwnd) {
	ThemedDialogBox(g_hInstance, MAKEINTRESOURCE(IDD_GOTO), hwnd, GotoDlgProc);
}

void OpenHelpLink(HWND hwnd, int cmd) {
	LPCWSTR link = NULL;
	switch (cmd) {
	case IDC_WEBPAGE_LINK:
		link = L"http://www.flos-freeware.ch";
		break;
	case IDC_EMAIL_LINK:
		link = L"mailto:florian.balmer@gmail.com";
		break;
	case IDC_NEW_PAGE_LINK:
		link = VERSION_NEWPAGE_DISPLAY;
		break;
	}

	if (StrNotEmpty(link)) {
		ShellExecute(hwnd, L"open", link, NULL, NULL, SW_SHOWNORMAL);
	}
}

//=============================================================================
//
//  AboutDlgProc()
//
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		WCHAR wch[256];
#if defined(VERSION_BUILD_TOOL_BUILD)
		wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
			VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH, VERSION_BUILD_TOOL_BUILD);
#else
		wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
			VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH);
#endif

		SetDlgItemText(hwnd, IDC_VERSION, VERSION_FILEVERSION_LONG);
		SetDlgItemText(hwnd, IDC_BUILD_INFO, wch);
		SetDlgItemText(hwnd, IDC_COPYRIGHT, VERSION_LEGALCOPYRIGHT_SHORT);
		SetDlgItemText(hwnd, IDC_AUTHORNAME, VERSION_AUTHORNAME);

		HFONT hFontTitle = (HFONT)SendDlgItemMessage(hwnd, IDC_VERSION, WM_GETFONT, 0, 0);
		if (hFontTitle == NULL) {
			hFontTitle = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		}

		LOGFONT lf;
		GetObject(hFontTitle, sizeof(LOGFONT), &lf);
		lf.lfWeight = FW_BOLD;
		hFontTitle = CreateFontIndirect(&lf);
		SendDlgItemMessage(hwnd, IDC_VERSION, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
		SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)(hFontTitle));

		if (GetDlgItem(hwnd, IDC_WEBPAGE_LINK) == NULL) {
			SetDlgItemText(hwnd, IDC_WEBPAGE_TEXT, VERSION_WEBPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_WEBPAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_WEBPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_WEBPAGE_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_EMAIL_LINK) == NULL) {
			SetDlgItemText(hwnd, IDC_EMAIL_TEXT, VERSION_EMAIL_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_EMAIL_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_EMAIL_DISPLAY);
			SetDlgItemText(hwnd, IDC_EMAIL_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_NEW_PAGE_LINK) == NULL) {
			SetDlgItemText(hwnd, IDC_NEW_PAGE_TEXT, VERSION_NEWPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_NEW_PAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_NEWPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_NEW_PAGE_LINK, wch);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmhdr = (LPNMHDR)lParam;
		switch (pnmhdr->code) {
		case NM_CLICK:
		case NM_RETURN:
			OpenHelpLink(hwnd, (int)(pnmhdr->idFrom));
			break;
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, IDOK);
			break;
		}
		return TRUE;

	case WM_DESTROY: {
		HFONT hFontTitle = (HFONT)GetWindowLongPtr(hwnd, DWLP_USER);
		DeleteObject(hFontTitle);
	}
	return FALSE;
	}
	return FALSE;
}

//=============================================================================
//
//  GeneralPageProc
//
//
extern BOOL bSaveSettings;
extern WCHAR szQuickview[MAX_PATH];
extern WCHAR szQuickviewParams[MAX_PATH];
extern WCHAR tchFavoritesDir[MAX_PATH];
extern BOOL bClearReadOnly;
extern BOOL bRenameOnCollision;
extern BOOL bSingleClick;
extern BOOL bOpenFileInSameWindow;
extern BOOL bTrackSelect;
extern BOOL bFullRowSelect;
extern BOOL bFocusEdit;
extern BOOL bAlwaysOnTop;
extern BOOL bMinimizeToTray;
extern BOOL fUseRecycleBin;
extern BOOL fNoConfirmDelete;
extern int  iStartupDir;
extern int  iEscFunction;
extern BOOL bReuseWindow;

static INT_PTR CALLBACK GeneralPageProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG:
		if (StrNotEmpty(szIniFile)) {
			if (bSaveSettings) {
				CheckDlgButton(hwnd, IDC_SAVESETTINGS, BST_CHECKED);
			}
		} else {
			EnableWindow(GetDlgItem(hwnd, IDC_SAVESETTINGS), FALSE);
		}

		if (bOpenFileInSameWindow) {
			CheckDlgButton(hwnd, IDC_OPENFILE_SAME_WINDOW, BST_CHECKED);
		}

		if (bSingleClick) {
			CheckDlgButton(hwnd, IDC_SINGLECLICK, BST_CHECKED);
		}

		if (bTrackSelect) {
			CheckDlgButton(hwnd, IDC_TRACKSELECT, BST_CHECKED);
		}

		if (bFullRowSelect) {
			CheckDlgButton(hwnd, IDC_FULLROWSELECT, BST_CHECKED);
		}

		if (bFocusEdit) {
			CheckDlgButton(hwnd, IDC_FOCUSEDIT, BST_CHECKED);
		}

		if (bAlwaysOnTop) {
			CheckDlgButton(hwnd, IDC_ALWAYSONTOP, BST_CHECKED);
		}

		if (bMinimizeToTray) {
			CheckDlgButton(hwnd, IDC_MINIMIZETOTRAY, BST_CHECKED);
		}

		if (bReuseWindow) {
			CheckDlgButton(hwnd, IDC_REUSEWINDOW, BST_CHECKED);
		}
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case NM_CLICK:
		case NM_RETURN:
			switch (((LPNMHDR)lParam)->idFrom) {
			case IDC_CLEARWINPOS:
				ClearWindowPositionHistory();
				break;

			case IDC_ABOUT:
				ThemedDialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
				break;
			}
			break;

		case PSN_APPLY:
			if (IsWindowEnabled(GetDlgItem(hwnd, IDC_SAVESETTINGS))) {
				bSaveSettings = IsButtonChecked(hwnd, IDC_SAVESETTINGS);
			}

			bOpenFileInSameWindow = IsButtonChecked(hwnd, IDC_OPENFILE_SAME_WINDOW);
			bSingleClick = IsButtonChecked(hwnd, IDC_SINGLECLICK);
			bTrackSelect = IsButtonChecked(hwnd, IDC_TRACKSELECT);
			bFullRowSelect = IsButtonChecked(hwnd, IDC_FULLROWSELECT);
			bFocusEdit = IsButtonChecked(hwnd, IDC_FOCUSEDIT);
			bAlwaysOnTop = IsButtonChecked(hwnd, IDC_ALWAYSONTOP);
			bMinimizeToTray = IsButtonChecked(hwnd, IDC_MINIMIZETOTRAY);

			IniSetBool(INI_SECTION_NAME_FLAGS, L"ReuseWindow", IsButtonChecked(hwnd, IDC_REUSEWINDOW));
			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
			return TRUE;
		}
	}

	return FALSE;
}

//=============================================================================
//
//  AdvancedPageProc
//
//
static INT_PTR CALLBACK AdvancedPageProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG:
		if (bWindowLayoutRTL) {
			CheckDlgButton(hwnd, IDC_RTL_LAYOUT, BST_CHECKED);
		}

		if (bClearReadOnly) {
			CheckDlgButton(hwnd, IDC_CLEARREADONLY, BST_CHECKED);
		}

		if (bRenameOnCollision) {
			CheckDlgButton(hwnd, IDC_RENAMEONCOLLISION, BST_CHECKED);
		}

		if (fUseRecycleBin) {
			CheckDlgButton(hwnd, IDC_USERECYCLEBIN, BST_CHECKED);
		}

		if (fNoConfirmDelete) {
			CheckDlgButton(hwnd, IDC_NOCONFIRMDELETE, BST_CHECKED);
		}

		if (iStartupDir) {
			CheckDlgButton(hwnd, IDC_STARTUPDIR, BST_CHECKED);
			if (iStartupDir == 1) {
				CheckRadioButton(hwnd, IDC_GOTOMRU, IDC_GOTOFAV, IDC_GOTOMRU);
			} else {
				CheckRadioButton(hwnd, IDC_GOTOMRU, IDC_GOTOFAV, IDC_GOTOFAV);
			}
		} else {
			CheckRadioButton(hwnd, IDC_GOTOMRU, IDC_GOTOFAV, IDC_GOTOMRU);
			EnableWindow(GetDlgItem(hwnd, IDC_GOTOMRU), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_GOTOFAV), FALSE);
		}

		if (iEscFunction) {
			CheckDlgButton(hwnd, IDC_ESCFUNCTION, BST_CHECKED);
			if (iEscFunction == 1) {
				CheckRadioButton(hwnd, IDC_ESCMIN, IDC_ESCEXIT, IDC_ESCMIN);
			} else {
				CheckRadioButton(hwnd, IDC_ESCMIN, IDC_ESCEXIT, IDC_ESCEXIT);
			}
		} else {
			CheckRadioButton(hwnd, IDC_ESCMIN, IDC_ESCEXIT, IDC_ESCMIN);
			EnableWindow(GetDlgItem(hwnd, IDC_ESCMIN), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_ESCEXIT), FALSE);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_STARTUPDIR) {
			if (IsButtonChecked(hwnd, IDC_STARTUPDIR)) {
				EnableWindow(GetDlgItem(hwnd, IDC_GOTOMRU), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_GOTOFAV), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_GOTOMRU), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_GOTOFAV), FALSE);
			}
		} else if (LOWORD(wParam) == IDC_ESCFUNCTION) {
			if (IsButtonChecked(hwnd, IDC_ESCFUNCTION)) {
				EnableWindow(GetDlgItem(hwnd, IDC_ESCMIN), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_ESCEXIT), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_ESCMIN), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_ESCEXIT), FALSE);
			}
		}
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case PSN_APPLY:
			bWindowLayoutRTL = IsButtonChecked(hwnd, IDC_RTL_LAYOUT);
			bClearReadOnly = IsButtonChecked(hwnd, IDC_CLEARREADONLY);
			bRenameOnCollision = IsButtonChecked(hwnd, IDC_RENAMEONCOLLISION);
			fUseRecycleBin = IsButtonChecked(hwnd, IDC_USERECYCLEBIN);
			fNoConfirmDelete = IsButtonChecked(hwnd, IDC_NOCONFIRMDELETE);

			if (IsButtonChecked(hwnd, IDC_STARTUPDIR)) {
				iStartupDir = IsButtonChecked(hwnd, IDC_GOTOMRU) ? 1 : 2;
			} else {
				iStartupDir = 0;
			}

			if (IsButtonChecked(hwnd, IDC_ESCFUNCTION)) {
				iEscFunction = IsButtonChecked(hwnd, IDC_ESCMIN) ? 1 : 2;
			} else {
				iEscFunction = 0;
			}

			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
			return TRUE;
		}
	}

	return FALSE;
}

//=============================================================================
//
//  ItemsPageProc
//
//
extern BOOL     bDefColorNoFilter;
extern BOOL     bDefColorFilter;
extern COLORREF colorNoFilter;
extern COLORREF colorFilter;
extern COLORREF colorCustom[16];

static INT_PTR CALLBACK ItemsPageProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	static BOOL m_bDefColorNoFilter;
	static BOOL m_bDefColorFilter;

	static COLORREF m_colorNoFilter;
	static COLORREF m_colorFilter;

	static HBRUSH m_hbrNoFilter;
	static HBRUSH m_hbrFilter;

	CHOOSECOLOR cc;

	switch (umsg) {
	case WM_INITDIALOG:
		m_bDefColorNoFilter = bDefColorNoFilter;
		m_bDefColorFilter = bDefColorFilter;

		m_colorNoFilter = colorNoFilter;
		m_colorFilter = colorFilter;

		m_hbrNoFilter = CreateSolidBrush(m_colorNoFilter);
		m_hbrFilter = CreateSolidBrush(m_colorFilter);

		if (m_bDefColorNoFilter) {
			CheckRadioButton(hwnd, IDC_COLOR_DEF1, IDC_COLOR_CUST1, IDC_COLOR_DEF1);
			EnableWindow(GetDlgItem(hwnd, IDC_COLOR_PICK1), FALSE);
		} else {
			CheckRadioButton(hwnd, IDC_COLOR_DEF1, IDC_COLOR_CUST1, IDC_COLOR_CUST1);
		}

		if (m_bDefColorFilter) {
			CheckRadioButton(hwnd, IDC_COLOR_DEF2, IDC_COLOR_CUST2, IDC_COLOR_DEF2);
			EnableWindow(GetDlgItem(hwnd, IDC_COLOR_PICK2), FALSE);
		} else {
			CheckRadioButton(hwnd, IDC_COLOR_DEF2, IDC_COLOR_CUST2, IDC_COLOR_CUST2);
		}
		return TRUE;

	case WM_DESTROY:
		DeleteObject(m_hbrNoFilter);
		DeleteObject(m_hbrFilter);
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_COLOR_DEF1:
		case IDC_COLOR_CUST1:
			m_bDefColorNoFilter = !IsButtonChecked(hwnd, IDC_COLOR_CUST1);
			EnableWindow(GetDlgItem(hwnd, IDC_COLOR_PICK1), !m_bDefColorNoFilter);
			InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SAMP1), NULL, TRUE);
			break;

		case IDC_COLOR_DEF2:
		case IDC_COLOR_CUST2:
			m_bDefColorFilter = !IsButtonChecked(hwnd, IDC_COLOR_CUST2);
			EnableWindow(GetDlgItem(hwnd, IDC_COLOR_PICK2), !m_bDefColorFilter);
			InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SAMP2), NULL, TRUE);
			break;

		case IDC_COLOR_PICK1:
			ZeroMemory(&cc, sizeof(CHOOSECOLOR));

			cc.lStructSize = sizeof(CHOOSECOLOR);
			cc.hwndOwner = hwnd;
			cc.rgbResult = m_colorNoFilter;
			cc.lpCustColors = colorCustom;
			cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;

			if (ChooseColor(&cc)) {
				DeleteObject(m_hbrNoFilter);
				m_colorNoFilter = cc.rgbResult;
				m_hbrNoFilter = CreateSolidBrush(m_colorNoFilter);
			}

			InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SAMP1), NULL, TRUE);
			break;

		case IDC_COLOR_PICK2:
			ZeroMemory(&cc, sizeof(CHOOSECOLOR));

			cc.lStructSize = sizeof(CHOOSECOLOR);
			cc.hwndOwner = hwnd;
			cc.rgbResult = m_colorFilter;
			cc.lpCustColors = colorCustom;
			cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;

			if (ChooseColor(&cc)) {
				DeleteObject(m_hbrFilter);
				m_colorFilter = cc.rgbResult;
				m_hbrFilter = CreateSolidBrush(m_colorFilter);
			}

			InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SAMP2), NULL, TRUE);
			break;
		}
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case PSN_APPLY:
			bDefColorNoFilter = m_bDefColorNoFilter;
			bDefColorFilter = m_bDefColorFilter;

			colorNoFilter = m_colorNoFilter;
			colorFilter = m_colorFilter;

			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
			return TRUE;
		}
		return FALSE;

	case WM_CTLCOLORSTATIC:
		if (!m_bDefColorNoFilter && GetDlgCtrlID((HWND)lParam) == IDC_COLOR_SAMP1) {
			return (LRESULT)m_hbrNoFilter;
		}
		if (!m_bDefColorFilter && GetDlgCtrlID((HWND)lParam) == IDC_COLOR_SAMP2) {
			return (LRESULT)m_hbrFilter;
		}
		return FALSE;
	}

	return FALSE;
}

//=============================================================================
//
//  ProgPageProc
//
//
static INT_PTR CALLBACK ProgPageProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		MakeBitmapButton(hwnd, IDC_BROWSE_Q, g_hInstance, IDB_OPEN);
		MakeBitmapButton(hwnd, IDC_BROWSE_F, g_hInstance, IDB_OPEN);

		WCHAR tch[MAX_PATH];
		lstrcpy(tch, szQuickview);
		PathQuoteSpaces(tch);
		if (StrNotEmpty(szQuickviewParams)) {
			StrCatBuff(tch, L" ", COUNTOF(tch));
			StrCatBuff(tch, szQuickviewParams, COUNTOF(tch));
		}
		SendDlgItemMessage(hwnd, IDC_QUICKVIEW, EM_LIMITTEXT, MAX_PATH - 2, 0);
		SetDlgItemText(hwnd, IDC_QUICKVIEW, tch);
		SHAutoComplete(GetDlgItem(hwnd, IDC_QUICKVIEW), SHACF_FILESYSTEM);

		SendDlgItemMessage(hwnd, IDC_FAVORITES, EM_LIMITTEXT, MAX_PATH - 2, 0);
		SetDlgItemText(hwnd, IDC_FAVORITES, tchFavoritesDir);
		SHAutoComplete(GetDlgItem(hwnd, IDC_FAVORITES), SHACF_FILESYSTEM);
	}
	return TRUE;

	case WM_DESTROY:
		DeleteBitmapButton(hwnd, IDC_BROWSE_Q);
		DeleteBitmapButton(hwnd, IDC_BROWSE_F);
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BROWSE_Q: {
			WCHAR tchBuf[MAX_PATH];
			WCHAR szFile[MAX_PATH];
			WCHAR szParams[MAX_PATH];

			GetDlgItemText(hwnd, IDC_QUICKVIEW, tchBuf, COUNTOF(tchBuf));
			ExtractFirstArgument(tchBuf, szFile, szParams);

			WCHAR szTitle[32];
			WCHAR szFilter[256];
			GetString(IDS_GETQUICKVIEWER, szTitle, COUNTOF(szTitle));
			GetString(IDS_FILTER_EXE, szFilter, COUNTOF(szFilter));
			PrepareFilterStr(szFilter);

			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = szFilter;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = COUNTOF(szFile);
			ofn.lpstrTitle = szTitle;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
						| OFN_PATHMUSTEXIST | OFN_SHAREAWARE | OFN_NODEREFERENCELINKS | OFN_NOVALIDATE;

			if (GetOpenFileName(&ofn)) {
				lstrcpyn(tchBuf, szFile, COUNTOF(tchBuf));
				PathQuoteSpaces(tchBuf);
				if (StrNotEmpty(szParams)) {
					StrCatBuff(tchBuf, L" ", COUNTOF(tchBuf));
					StrCatBuff(tchBuf, szParams, COUNTOF(tchBuf));
				}
				SetDlgItemText(hwnd, IDC_QUICKVIEW, tchBuf);
			}

			PostMessage(hwnd, WM_NEXTDLGCTL, 1, 0);
		}
		break;

		case IDC_BROWSE_F: {
			WCHAR tch[MAX_PATH];

			GetDlgItemText(hwnd, IDC_FAVORITES, tch, COUNTOF(tch));
			StrTrim(tch, L" \"");

			if (GetDirectory(hwnd, IDS_FAVORITES, tch, tch)) {
				SetDlgItemText(hwnd, IDC_FAVORITES, tch);
			}

			PostMessage(hwnd, WM_NEXTDLGCTL, 1, 0);
		}
		break;
		}
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case PSN_APPLY: {
			WCHAR tch[MAX_PATH];

			if (!GetDlgItemText(hwnd, IDC_QUICKVIEW, tch, MAX_PATH)) {
				GetSystemDirectory(szQuickview, MAX_PATH);
				PathAddBackslash(szQuickview);
				lstrcat(szQuickview, L"Viewers\\Quikview.exe");
				PathQuoteSpaces(szQuickview);
				lstrcpy(szQuickviewParams, L"");
			} else {
				ExtractFirstArgument(tch, szQuickview, szQuickviewParams);
			}

			if (!GetDlgItemText(hwnd, IDC_FAVORITES, tchFavoritesDir, MAX_PATH)) {
				GetDefaultFavoritesDir(tchFavoritesDir, COUNTOF(tchFavoritesDir));
			} else {
				StrTrim(tchFavoritesDir, L" \"");
			}

			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
		}
		return TRUE;
		}
	}

	return FALSE;
}

//=============================================================================
//
//  OptionsPropSheet
//
//
extern HWND hwndStatus;
extern int nIdFocus;

extern WCHAR tchFilter[128];
extern BOOL bNegFilter;
extern int cxFileFilterDlg;

INT_PTR OptionsPropSheet(HWND hwnd, HINSTANCE hInstance) {
	PROPSHEETHEADER psh;
	PROPSHEETPAGE psp[4];
	INT_PTR nResult;

	ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
	ZeroMemory(psp, sizeof(psp));

	psp[0].dwSize      = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags     = PSP_DLGINDIRECT;
	psp[0].hInstance   = hInstance;
	psp[0].pResource   = LoadThemedDialogTemplate(MAKEINTRESOURCE(IDPP_GENERAL), hInstance);
	psp[0].pfnDlgProc  = GeneralPageProc;

	psp[1].dwSize      = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags     = PSP_DLGINDIRECT;
	psp[1].hInstance   = hInstance;
	psp[1].pResource   = LoadThemedDialogTemplate(MAKEINTRESOURCE(IDPP_ADVANCED), hInstance);
	psp[1].pfnDlgProc  = AdvancedPageProc;

	psp[2].dwSize      = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags     = PSP_DLGINDIRECT;
	psp[2].hInstance   = hInstance;
	psp[2].pResource   = LoadThemedDialogTemplate(MAKEINTRESOURCE(IDPP_ITEMS), hInstance);
	psp[2].pfnDlgProc  = ItemsPageProc;

	psp[3].dwSize      = sizeof(PROPSHEETPAGE);
	psp[3].dwFlags     = PSP_DLGINDIRECT;
	psp[3].hInstance   = hInstance;
	psp[3].pResource   = LoadThemedDialogTemplate(MAKEINTRESOURCE(IDPP_PROG), hInstance);
	psp[3].pfnDlgProc  = ProgPageProc;

	psh.dwSize      = sizeof(PROPSHEETHEADER);
	psh.dwFlags     = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW | PSH_PROPTITLE;
	psh.hwndParent  = hwnd;
	psh.hInstance   = hInstance;
	psh.pszCaption  = L"metapath";
	psh.nPages      = 4;
	psh.nStartPage  = 0;
	psh.ppsp        = psp;

	nResult = PropertySheet(&psh);

	if (psp[0].pResource) {
		NP2HeapFree((LPVOID)psp[0].pResource);
	}
	if (psp[1].pResource) {
		NP2HeapFree((LPVOID)psp[1].pResource);
	}
	if (psp[2].pResource) {
		NP2HeapFree((LPVOID)psp[2].pResource);
	}
	if (psp[3].pResource) {
		NP2HeapFree((LPVOID)psp[3].pResource);
	}

	// Apply the results
	if (nResult) {
		if (bAlwaysOnTop) {
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		} else {
			SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if (bTrackSelect) {
			ListView_SetExtendedListViewStyleEx(hwndDirList, LVS_EX_TRACKSELECT | LVS_EX_ONECLICKACTIVATE, LVS_EX_TRACKSELECT | LVS_EX_ONECLICKACTIVATE);
		} else {
			ListView_SetExtendedListViewStyleEx(hwndDirList, LVS_EX_TRACKSELECT | LVS_EX_ONECLICKACTIVATE, 0);
		}

		if (bFullRowSelect) {
			ListView_SetExtendedListViewStyleEx(hwndDirList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
			SetExplorerTheme(hwndDirList);
		} else {
			ListView_SetExtendedListViewStyleEx(hwndDirList, LVS_EX_FULLROWSELECT, 0);
			SetListViewTheme(hwndDirList);
		}

		if (!StrEqual(tchFilter, L"*.*") || bNegFilter) {
			ListView_SetTextColor(hwndDirList, bDefColorFilter ? GetSysColor(COLOR_WINDOWTEXT) : colorFilter);
			ListView_RedrawItems(hwndDirList, 0, ListView_GetItemCount(hwndDirList) - 1);
		} else {
			ListView_SetTextColor(hwndDirList, bDefColorNoFilter ? GetSysColor(COLOR_WINDOWTEXT) : colorNoFilter);
			ListView_RedrawItems(hwndDirList, 0, ListView_GetItemCount(hwndDirList) - 1);
		}
	}

	return nResult;
}

//=============================================================================
//
//  GetFilterDlgProc()
//
//
INT_PTR CALLBACK GetFilterDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		ResizeDlg_InitX(hwnd, cxFileFilterDlg, IDC_RESIZEGRIP3);
		MakeBitmapButton(hwnd, IDC_BROWSEFILTER, NULL, OBM_COMBO);

		SendDlgItemMessage(hwnd, IDC_FILTER, EM_LIMITTEXT, COUNTOF(tchFilter) - 1, 0);
		SetDlgItemText(hwnd, IDC_FILTER, tchFilter);

		CheckDlgButton(hwnd, IDC_NEGFILTER, bNegFilter);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxFileFilterDlg, NULL);
		DeleteBitmapButton(hwnd, IDC_BROWSEFILTER);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(5);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_BROWSEFILTER, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FILTER, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BROWSEFILTER: {
			WCHAR szTypedFilter[512];

			GetDlgItemText(hwnd, IDC_FILTER, szTypedFilter, COUNTOF(szTypedFilter));

			HMENU hMenu = CreatePopupMenu();

			IniSection section;
			WCHAR *pIniSectionBuf = (WCHAR *)NP2HeapAlloc(sizeof(WCHAR) * MAX_INI_SECTION_SIZE_FILTERS);
			const int cchIniSection = (int)(NP2HeapSize(pIniSectionBuf) / sizeof(WCHAR));
			IniSection *pIniSection = &section;
			IniSectionInit(pIniSection, 128);

			LoadIniSection(INI_SECTION_NAME_FILTERS, pIniSectionBuf, cchIniSection);
			IniSectionParseArray(pIniSection, pIniSectionBuf);

			DWORD dwIndex = 0;
			DWORD dwCheck = 0xFFFF; // index of current filter
			for (int i = 0; i < pIniSection->count; i++) {
				const IniKeyValueNode *node = &pIniSection->nodeList[i];
				LPCWSTR pszFilterValue = node->value;
				if (*pszFilterValue) {
					AppendMenu(hMenu, MF_ENABLED | MF_STRING, 1234 + dwIndex, node->key);
					// Find description for current filter
					const BOOL negFilter = IsButtonChecked(hwnd, IDC_NEGFILTER);
					if ((StrCaseEqual(pszFilterValue, szTypedFilter) && !negFilter) ||
							(StrCaseEqual(CharNext(pszFilterValue), szTypedFilter) &&
							 negFilter && *pszFilterValue == L'-')) {
						dwCheck = dwIndex;
					}
				}

				dwIndex++;
			}
			IniSectionFree(pIniSection);
			NP2HeapFree(pIniSectionBuf);

			if (dwCheck != 0xFFFF) { // check description for current filter
				CheckMenuRadioItem(hMenu, 0, dwIndex, dwCheck, MF_BYPOSITION);
			}

			if (dwIndex) { // at least 1 item exists
				RECT rc;

				GetWindowRect(GetDlgItem(hwnd, IDC_BROWSEFILTER), &rc);
				//MapWindowPoints(hwnd, NULL, (POINT*)&rc, 2);
				// Seems that TrackPopupMenuEx() works with client coords...?
				const DWORD dwCmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, rc.left + 1, rc.bottom + 1, hwnd, NULL);

				if (dwCmd) {
					WCHAR tchName[256];
					WCHAR tchValue[256];
					GetMenuString(hMenu, dwCmd, tchName, COUNTOF(tchName), MF_BYCOMMAND);

					if (IniGetString(INI_SECTION_NAME_FILTERS, tchName, L"", tchValue, COUNTOF(tchValue))) {
						if (tchValue[0] == L'-') { // Negative Filter
							if (tchValue[1]) {
								SetDlgItemText(hwnd, IDC_FILTER, tchValue + 1);
								CheckDlgButton(hwnd, IDC_NEGFILTER, TRUE);
							} else {
								MessageBeep(MB_OK);
							}
						} else {
							SetDlgItemText(hwnd, IDC_FILTER, tchValue);
							CheckDlgButton(hwnd, IDC_NEGFILTER, FALSE);
						}
					} else {
						MessageBeep(MB_OK);
					}
				}
			} else {
				ErrorMessage(0, IDS_ERR_FILTER);
			}

			DestroyMenu(hMenu);
			PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)(GetDlgItem(hwnd, IDC_FILTER)), 1);
		}
		break;

		case IDOK:
			if (GetDlgItemText(hwnd, IDC_FILTER, tchFilter, COUNTOF(tchFilter) - 1)) {
				bNegFilter = IsButtonChecked(hwnd, IDC_NEGFILTER);
			} else {
				lstrcpy(tchFilter, L"*.*");
				bNegFilter = FALSE;
			}
			EndDialog(hwnd, IDOK);
			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  GetFilterDlg()
//
//
BOOL GetFilterDlg(HWND hwnd) {
	WCHAR tchOldFilter[DL_FILTER_BUFSIZE];

	lstrcpy(tchOldFilter, tchFilter);
	const BOOL bOldNegFilter = bNegFilter;

	if (IDOK == ThemedDialogBox(g_hInstance, MAKEINTRESOURCE(IDD_FILTER), hwnd, GetFilterDlgProc)) {
		if (StrCaseEqual(tchFilter, tchOldFilter) && (bOldNegFilter == bNegFilter)) {
			return FALSE;    // Old and new filters are identical
		}
		return TRUE;
	}

	return FALSE;
}

// Data structure used in file operation dialogs
typedef struct tagFILEOPDLGDATA {
	WCHAR szSource[MAX_PATH];
	WCHAR szDestination[MAX_PATH];
	UINT wFunc;
} FILEOPDLGDATA, *LPFILEOPDLGDATA;

typedef const FILEOPDLGDATA * LPCFILEOPDLGDATA;

extern int cxRenameFileDlg;
//=============================================================================
//
//  RenameFileDlgProc()
//
//
INT_PTR CALLBACK RenameFileDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_InitX(hwnd, cxRenameFileDlg, IDC_RESIZEGRIP2);
		const LPCFILEOPDLGDATA lpfod = (LPCFILEOPDLGDATA)lParam;

		SetDlgItemText(hwnd, IDC_OLDNAME, lpfod->szSource);
		SetDlgItemText(hwnd, IDC_NEWNAME, lpfod->szSource);
		SendDlgItemMessage(hwnd, IDC_NEWNAME, EM_LIMITTEXT, MAX_PATH - 1, 0);
		SendDlgItemMessage(hwnd, IDC_NEWNAME, EM_SETMODIFY, 0, 0);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxRenameFileDlg, NULL);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(5);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP2, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_OLDNAME, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_NEWNAME, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_NEWNAME:
			EnableWindow(GetDlgItem(hwnd, IDOK), GetWindowTextLength(GetDlgItem(hwnd, IDC_NEWNAME)));
			break;

		case IDOK:
			if (!SendDlgItemMessage(hwnd, IDC_NEWNAME, EM_GETMODIFY, 0, 0)) {
				EndDialog(hwnd, IDCANCEL);
			} else {
				LPFILEOPDLGDATA lpfod = (LPFILEOPDLGDATA)GetWindowLongPtr(hwnd, DWLP_USER);
				GetDlgItemText(hwnd, IDC_NEWNAME, lpfod->szDestination, COUNTOF(lpfod->szDestination) - 1);
				EndDialog(hwnd, IDOK);
			}
			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  RenameFileDlg()
//
//
BOOL RenameFileDlg(HWND hwnd) {
	DLITEM dli;

	dli.mask = DLI_FILENAME;
	if (DirList_GetItem(hwndDirList, -1, &dli) == -1) {
		return FALSE;
	}

	FILEOPDLGDATA fod;
	lstrcpy(fod.szSource, GetFilenameStr(dli.szFileName));

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_RENAME), hwnd, RenameFileDlgProc, (LPARAM)&fod)) {
		WCHAR tchSource[MAX_PATH + 4];
		WCHAR szFullDestination[MAX_PATH];
		WCHAR tchDestination[MAX_PATH + 4];

		SHFILEOPSTRUCT shfos;
		ZeroMemory(&shfos, sizeof(SHFILEOPSTRUCT));
		shfos.hwnd = hwnd;
		shfos.wFunc = FO_RENAME;
		shfos.pFrom = tchSource;
		shfos.pTo = tchDestination;
		shfos.fFlags = FOF_ALLOWUNDO;

		// Generate fully qualified destination
		lstrcpy(szFullDestination, dli.szFileName);
		*GetFilenameStr(szFullDestination) = 0;
		lstrcat(szFullDestination, fod.szDestination);

		// Double null terminated strings are essential!!!
		ZeroMemory(tchSource, sizeof(tchSource));
		ZeroMemory(tchDestination, sizeof(tchDestination));
		lstrcpy(tchSource, dli.szFileName);
		lstrcpy(tchDestination, szFullDestination);

		if (SHFileOperation(&shfos) == 0) { // success, select renamed item
			SHFILEINFO shfi;
			// refresh directory view
			SendWMCommand(hwnd, IDM_VIEW_UPDATE);
			// get new display name
			SHGetFileInfo(tchDestination, 0, &shfi, sizeof(SHFILEINFO), SHGFI_DISPLAYNAME);
			DirList_SelectItem(hwndDirList, shfi.szDisplayName, tchDestination);
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  CopyMoveDlgProc()
//
//
extern int cxCopyMoveDlg;

INT_PTR CALLBACK CopyMoveDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);

		ResizeDlg_InitX(hwnd, cxCopyMoveDlg, IDC_RESIZEGRIP5);
		MakeBitmapButton(hwnd, IDC_BROWSEDESTINATION, g_hInstance, IDB_OPEN);

		const LPCFILEOPDLGDATA lpfod = (LPCFILEOPDLGDATA)lParam;
		HWND hwndDest = GetDlgItem(hwnd, IDC_DESTINATION);
		MRU_LoadToCombobox(hwndDest, MRU_KEY_COPY_MOVE_HISTORY);
		SendMessage(hwndDest, CB_SETCURSEL, 0, 0);

		SetDlgItemText(hwnd, IDC_SOURCE, lpfod->szSource);
		SendMessage(hwndDest, CB_LIMITTEXT, MAX_PATH - 1, 0);

		SendMessage(hwndDest, CB_SETEXTENDEDUI, TRUE, 0);

		if (lpfod->wFunc == FO_COPY) {
			CheckRadioButton(hwnd, IDC_FUNCCOPY, IDC_FUNCMOVE, IDC_FUNCCOPY);
		} else {
			CheckRadioButton(hwnd, IDC_FUNCCOPY, IDC_FUNCMOVE, IDC_FUNCMOVE);
		}

		COMBOBOXINFO cbi;
		cbi.cbSize = sizeof(COMBOBOXINFO);
		if (GetComboBoxInfo(hwndDest, &cbi)) {
			SHAutoComplete(cbi.hwndItem, SHACF_FILESYSTEM);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxCopyMoveDlg, NULL);
		DeleteBitmapButton(hwnd, IDC_BROWSEDESTINATION);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(7);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP5, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_EMPTY_MRU, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_SOURCE, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_DESTINATION, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_BROWSEDESTINATION, dx, 0, SWP_NOSIZE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmhdr = (LPNMHDR)lParam;
		if (pnmhdr->idFrom == IDC_EMPTY_MRU && (pnmhdr->code == NM_CLICK || pnmhdr->code == NM_RETURN)) {
			WCHAR tch[MAX_PATH];
			GetDlgItemText(hwnd, IDC_DESTINATION, tch, COUNTOF(tch));
			MRU_ClearCombobox(GetDlgItem(hwnd, IDC_DESTINATION), MRU_KEY_COPY_MOVE_HISTORY);
			SetDlgItemText(hwnd, IDC_DESTINATION, tch);
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_DESTINATION:
			EnableWindow(GetDlgItem(hwnd, IDOK),
						 (GetWindowTextLength(GetDlgItem(hwnd, IDC_DESTINATION)) ||
						  CB_ERR != SendDlgItemMessage(hwnd, IDC_DESTINATION, CB_GETCURSEL, 0, 0)));
			break;

		case IDC_BROWSEDESTINATION: {
			WCHAR tch[MAX_PATH];
			GetDlgItemText(hwnd, IDC_DESTINATION, tch, COUNTOF(tch));
			ExpandEnvironmentStringsEx(tch, COUNTOF(tch));
			if (GetDirectory(hwnd, IDS_COPYMOVE, tch, tch)) {
				SetDlgItemText(hwnd, IDC_DESTINATION, tch);
			}
			PostMessage(hwnd, WM_NEXTDLGCTL, 1, 0);
		}
		break;

		case IDOK: {
			/*text*/
			LPFILEOPDLGDATA lpfod = (LPFILEOPDLGDATA)GetWindowLongPtr(hwnd, DWLP_USER);
			if (GetDlgItemText(hwnd, IDC_DESTINATION, lpfod->szDestination, COUNTOF(lpfod->szDestination) - 1)) {
				lpfod->wFunc = IsButtonChecked(hwnd, IDC_FUNCCOPY) ? FO_COPY : FO_MOVE;
				EndDialog(hwnd, IDOK);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDOK),
							 (GetWindowTextLength(GetDlgItem(hwnd, IDC_DESTINATION)) ||
							  CB_ERR != SendDlgItemMessage(hwnd, IDC_DESTINATION, CB_GETCURSEL, 0, 0)));
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  CopyMoveDlg()
//
//
BOOL CopyMoveDlg(HWND hwnd, UINT *wFunc) {
	DLITEM dli;

	dli.mask = DLI_FILENAME;
	if (DirList_GetItem(hwndDirList, -1, &dli) == -1) {
		return FALSE;
	}

	FILEOPDLGDATA fod;
	fod.wFunc = *wFunc;
	lstrcpy(fod.szSource, GetFilenameStr(dli.szFileName));

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COPYMOVE), hwnd, CopyMoveDlgProc, (LPARAM)&fod)) {
		WCHAR tchSource[MAX_PATH + 4];
		WCHAR tchDestination[MAX_PATH + 4];

		SHFILEOPSTRUCT shfos;
		ZeroMemory(&shfos, sizeof(SHFILEOPSTRUCT));
		shfos.hwnd = hwnd;
		shfos.wFunc = fod.wFunc;
		shfos.pFrom = tchSource;
		shfos.pTo = tchDestination;
		shfos.fFlags = FOF_NO_CONNECTED_ELEMENTS | FOF_ALLOWUNDO;
		if (shfos.wFunc == FO_COPY && bRenameOnCollision) {
			shfos.fFlags |= FOF_RENAMEONCOLLISION;
		}

		// Save item
		MRU_AddOneItem(MRU_KEY_COPY_MOVE_HISTORY, fod.szDestination);
		ExpandEnvironmentStringsEx(fod.szDestination, COUNTOF(fod.szDestination));

		// Double null terminated strings are essential!!!
		ZeroMemory(tchSource, sizeof(tchSource));
		ZeroMemory(tchDestination, sizeof(tchDestination));
		lstrcpy(tchSource, dli.szFileName);
		lstrcpy(tchDestination, fod.szDestination);

		// tchDestination is always assumed to be a directory
		// if it doesn't exist, the file name of tchSource is added
		if (PathIsRelative(tchDestination)) {
			WCHAR wszDir[MAX_PATH];
			GetCurrentDirectory(COUNTOF(wszDir), wszDir);
			PathAppend(wszDir, tchDestination);
			lstrcpy(tchDestination, wszDir);
		}

		if (!PathIsDirectory(tchDestination)) {
			PathAppend(tchDestination, PathFindFileName(dli.szFileName));
		}

		if (SHFileOperation(&shfos) == 0) { // success
			if (bClearReadOnly) {
				DWORD dwFileAttributes = GetFileAttributes(tchDestination);
				if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
					dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
					SetFileAttributes(tchDestination, dwFileAttributes);
					// this should work after the successful file operation...
				}
			}
		}

		*wFunc = fod.wFunc; // save state for next call
		return TRUE;
	}
	return FALSE;
}

extern WCHAR tchOpenWithDir[MAX_PATH];
extern int flagNoFadeHidden;

extern int cxOpenWithDlg;
extern int cyOpenWithDlg;
extern int cxNewDirectoryDlg;

INT_PTR CALLBACK OpenWithDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_Init(hwnd, cxOpenWithDlg, cyOpenWithDlg, IDC_RESIZEGRIP3);

		HWND hwndLV = GetDlgItem(hwnd, IDC_OPENWITHDIR);
		InitWindowCommon(hwndLV);
		//SetExplorerTheme(hwndLV);
		ListView_SetExtendedListViewStyle(hwndLV, /*LVS_EX_FULLROWSELECT | */LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
		LVCOLUMN lvc = { LVCF_FMT | LVCF_TEXT, LVCFMT_LEFT, 0, NULL, -1, 0, 0, 0
#if (NTDDI_VERSION >= NTDDI_VISTA)
			, 0, 0, 0
#endif
		};
		ListView_InsertColumn(hwndLV, 0, &lvc);
		DirList_Init(hwndLV, NULL);
		DirList_Fill(hwndLV, tchOpenWithDir, DL_ALLOBJECTS, NULL, FALSE, flagNoFadeHidden, DS_NAME, FALSE);
		DirList_StartIconThread(hwndLV);
		ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);

		MakeBitmapButton(hwnd, IDC_GETOPENWITHDIR, g_hInstance, IDB_OPEN);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		DirList_Destroy(GetDlgItem(hwnd, IDC_OPENWITHDIR));
		DeleteBitmapButton(hwnd, IDC_GETOPENWITHDIR);
		ResizeDlg_Destroy(hwnd, &cxOpenWithDlg, &cyOpenWithDlg);
		return FALSE;

	case WM_SIZE: {
		int dx;
		int dy;

		ResizeDlg_Size(hwnd, lParam, &dx, &dy);

		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_OPENWITHDIR, dx, dy, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_GETOPENWITHDIR, 0, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_OPENWITHDESCR, 0, dy, SWP_NOSIZE);
		EndDeferWindowPos(hdwp);

		ResizeDlgCtl(hwnd, IDC_OPENWITHDESCR, dx, 0);
		ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_OPENWITHDIR), 0, LVSCW_AUTOSIZE_USEHEADER);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmh = (LPNMHDR)lParam;
		if (pnmh->idFrom == IDC_OPENWITHDIR) {
			HWND hwndLV = GetDlgItem(hwnd, IDC_OPENWITHDIR);
			switch (pnmh->code) {
			case LVN_GETDISPINFO:
				DirList_GetDispInfo(hwndLV, lParam, flagNoFadeHidden);
				break;

			case LVN_DELETEITEM:
				DirList_DeleteItem(hwndLV, lParam);
				break;

			case LVN_ITEMCHANGED: {
				const NM_LISTVIEW *pnmlv = (NM_LISTVIEW *)lParam;
				EnableWindow(GetDlgItem(hwnd, IDOK), (pnmlv->uNewState & LVIS_SELECTED));
			}
			break;

			case NM_DBLCLK:
				if (ListView_GetSelectedCount(hwndLV)) {
					SendWMCommand(hwnd, IDOK);
				}
				break;
			}
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_GETOPENWITHDIR: {
			HWND hwndLV = GetDlgItem(hwnd, IDC_OPENWITHDIR);
			if (GetDirectory(hwnd, IDS_OPENWITH, tchOpenWithDir, tchOpenWithDir)) {
				DirList_Fill(hwndLV, tchOpenWithDir, DL_ALLOBJECTS, NULL, FALSE, flagNoFadeHidden, DS_NAME, FALSE);
				DirList_StartIconThread(hwndLV);
				ListView_EnsureVisible(hwndLV, 0, FALSE);
				ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);
			}
			PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)(hwndLV), 1);
		}
		break;

		case IDOK: {
			LPDLITEM lpdli = (LPDLITEM)GetWindowLongPtr(hwnd, DWLP_USER);
			lpdli->mask = DLI_FILENAME | DLI_TYPE;
			lpdli->ntype = DLE_NONE;
			DirList_GetItem(GetDlgItem(hwnd, IDC_OPENWITHDIR), (-1), lpdli);

			if (lpdli->ntype != DLE_NONE) {
				EndDialog(hwnd, IDOK);
			} else {
				MessageBeep(MB_OK);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  OpenWithDlg()
//
//
BOOL OpenWithDlg(HWND hwnd, LPCDLITEM lpdliParam) {
	DLITEM dliOpenWith;
	dliOpenWith.mask = DLI_FILENAME;

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OPENWITH), hwnd, OpenWithDlgProc, (LPARAM)&dliOpenWith)) {
		WCHAR szDestination[MAX_PATH + 4];
		ZeroMemory(szDestination, sizeof(szDestination));

		if (PathIsLnkToDirectory(dliOpenWith.szFileName, szDestination, COUNTOF(szDestination))) {
			WCHAR szSource[MAX_PATH + 4];
			ZeroMemory(szSource, sizeof(szSource));
			lstrcpy(szSource, lpdliParam->szFileName);

			PathAppend(szDestination, PathFindFileName(szSource));

			SHFILEOPSTRUCT shfos;
			ZeroMemory(&shfos, sizeof(SHFILEOPSTRUCT));
			shfos.hwnd = hwnd;
			shfos.wFunc = FO_COPY;
			shfos.pFrom = szSource;
			shfos.pTo = szDestination;
			shfos.fFlags = FOF_ALLOWUNDO;

			if (SHFileOperation(&shfos) == 0) { // success
				if (bClearReadOnly) {
					DWORD dwFileAttributes = GetFileAttributes(szDestination);
					if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
						dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
						SetFileAttributes(szDestination, dwFileAttributes);
						// this should work after the successful file operation...
					}
				}
			}
			return TRUE;
		}
		{
			SHELLEXECUTEINFO sei;
			WCHAR szParam[MAX_PATH];

			ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = 0;
			sei.hwnd = hwnd;
			sei.lpVerb = NULL;
			sei.lpFile = dliOpenWith.szFileName;
			sei.lpParameters = szParam;
			sei.lpDirectory = szCurDir;
			sei.nShow = SW_SHOWNORMAL;

			// resolve links and get short path name
			if (!(PathIsLnkFile(lpdliParam->szFileName) && PathGetLnkPath(lpdliParam->szFileName, szParam, COUNTOF(szParam)))) {
				lstrcpy(szParam, lpdliParam->szFileName);
			}

			GetShortPathName(szParam, szParam, COUNTOF(szParam));
			ShellExecuteEx(&sei);
			return TRUE;
		}
	}

	return FALSE;
}

//=============================================================================
//
//  NewDirDlgProc()
//
//
INT_PTR CALLBACK NewDirDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_InitX(hwnd, cxNewDirectoryDlg, IDC_RESIZEGRIP);

		SendDlgItemMessage(hwnd, IDC_NEWDIR, EM_LIMITTEXT, MAX_PATH - 1, 0);
		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxNewDirectoryDlg, NULL);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(4);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_NEWDIR, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_NEWDIR:
			EnableWindow(GetDlgItem(hwnd, IDOK), GetWindowTextLength(GetDlgItem(hwnd, IDC_NEWDIR)));
			break;

		case IDOK: {
			LPFILEOPDLGDATA lpfod = (LPFILEOPDLGDATA)GetWindowLongPtr(hwnd, DWLP_USER);
			GetDlgItemText(hwnd, IDC_NEWDIR, lpfod->szDestination, COUNTOF(lpfod->szDestination) - 1);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
//  NewDirDlg()
//
//
BOOL NewDirDlg(HWND hwnd, LPWSTR pszNewDir) {
	FILEOPDLGDATA fod;

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NEWDIR), hwnd, NewDirDlgProc, (LPARAM)&fod)) {
		lstrcpy(pszNewDir, fod.szDestination);
		return TRUE;
	}
	return FALSE;
}

//=============================================================================
//
//  FindWinDlgProc()
//
//  Find target window helper dialog
//
extern int flagPortableMyDocs;
extern int cxFindWindowDlg;

static INT_PTR CALLBACK FindWinDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	static HICON hIconCross1;
	static HICON hIconCross2;
	static HCURSOR hCursorCross;
	static BOOL bHasCapture;

	switch (umsg) {
	case WM_INITDIALOG:
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_InitX(hwnd, cxFindWindowDlg, IDC_RESIZEGRIP5);

		hIconCross1 = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_CROSS1));
		hIconCross2 = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_CROSS2));
		hCursorCross = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_CROSSHAIR));
		CenterDlgInParent(hwnd);
		bHasCapture = FALSE;
		return TRUE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(5);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP5, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FINDWINDESC, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_WINTITLE, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
		InvalidateRect(GetDlgItem(hwnd, IDC_FINDWINDESC), NULL, TRUE);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_CANCELMODE:
		ReleaseCapture();
		bHasCapture = FALSE;
		break;

	case WM_LBUTTONDOWN: {
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		if (GetDlgCtrlID((ChildWindowFromPoint(hwnd, pt))) == IDC_CROSSCURSOR) {
			SetCapture(hwnd);
			bHasCapture = TRUE;
			SetCursor(hCursorCross);
			SendDlgItemMessage(hwnd, IDC_CROSSCURSOR, STM_SETICON, (WPARAM)hIconCross2, 0);
		}
	}
	break;

	case WM_LBUTTONUP: {
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		SendDlgItemMessage(hwnd, IDC_CROSSCURSOR, STM_SETICON, (WPARAM)hIconCross1, 0);
		ReleaseCapture();
		bHasCapture = FALSE;

		HWND hwndOK = GetDlgItem(hwnd, IDOK);
		WCHAR tch[256];
		EnableWindow(hwndOK, GetDlgItemText(hwnd, IDC_WINCLASS, tch, COUNTOF(tch)));
		if (IsWindowEnabled(hwndOK)) {
			PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)(hwndOK), 1);
		}

		//if (GetDlgItemText(hwnd, IDC_WINMODULE, tch, COUNTOF(tch))) {
		//	SetDlgItemText(GetParent(hwnd), IDC_TARGETPATH, tch);
		//}
		//
		//if (GetDlgItemText(hwnd, IDC_WINCLASS, tch, COUNTOF(tch))) {
		//	LPWSTR pTargetWndClassBuf = (LPWSTR)GetWindowLongPtr(hwnd, DWLP_USER);
		//	lstrcpyn(pTargetWndClassBuf, tch, 256);
		//	PostMessage(hwnd, WM_CLOSE, 0, 0);
		//}
	}
	break;

	case WM_MOUSEMOVE: {
		if (bHasCapture) {
			POINT pt;
			GetCursorPos(&pt);

			HWND hwndFind = WindowFromPoint(pt);
			while (GetWindowLongPtr(hwndFind, GWL_STYLE) & WS_CHILD) {
				hwndFind = GetParent(hwndFind);
			}

			if (hwndFind != hwnd) {
				WCHAR tch[256];
				GetWindowText(hwndFind, tch, COUNTOF(tch));
				SetDlgItemText(hwnd, IDC_WINTITLE, tch);
				GetClassName(hwndFind, tch, COUNTOF(tch));
				SetDlgItemText(hwnd, IDC_WINCLASS, tch);

				if (ExeNameFromWnd(hwndFind, tch, COUNTOF(tch))) {
					SetDlgItemText(hwnd, IDC_WINMODULE, tch);
				} else {
					SetDlgItemText(hwnd, IDC_WINMODULE, L"");
				}
			} else {
				SetDlgItemText(hwnd, IDC_WINTITLE, L"");
				SetDlgItemText(hwnd, IDC_WINCLASS, L"");
				SetDlgItemText(hwnd, IDC_WINMODULE, L"");
			}
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			WCHAR tch[MAX_PATH] = L"";
			if (GetDlgItemText(hwnd, IDC_WINMODULE, tch, COUNTOF(tch))) {
				PathRelativeToApp(tch, tch, COUNTOF(tch), TRUE, TRUE, flagPortableMyDocs);
				PathQuoteSpaces(tch);
				SetDlgItemText(GetParent(hwnd), IDC_TARGETPATH, tch);
			}

			if (GetDlgItemText(hwnd, IDC_WINCLASS, tch, COUNTOF(tch))) {
				LPWSTR pTargetWndClassBuf = (LPWSTR)GetWindowLongPtr(hwnd, DWLP_USER);
				lstrcpyn(pTargetWndClassBuf, tch, COUNTOF(tch));
			}
			EndDialog(hwnd, IDOK);
		} break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxFindWindowDlg, NULL);
		if (bHasCapture) {
			ReleaseCapture();
			SendMessage(hwnd, WM_LBUTTONUP, 0, 0);
		}
		DestroyIcon(hIconCross1);
		DestroyIcon(hIconCross2);
		DestroyCursor(hCursorCross);
		return FALSE;
	}

	return FALSE;
}

//=============================================================================
//
//  FindTargetDlgProc()
//
//  Select metapath target application
//
extern int iUseTargetApplication;
extern int iTargetApplicationMode;
extern int cxTargetApplicationDlg;
extern BOOL bLoadLaunchSetingsLoaded;
extern WCHAR szTargetApplication[MAX_PATH];
extern WCHAR szTargetApplicationParams[MAX_PATH];
extern WCHAR szTargetApplicationWndClass[MAX_PATH];
extern WCHAR szDDEMsg[256];
extern WCHAR szDDEApp[256];
extern WCHAR szDDETopic[256];

INT_PTR CALLBACK FindTargetDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);

	static WCHAR szTargetWndClass[256] = L"";

	switch (umsg) {
	case WM_INITDIALOG: {
		ResizeDlg_InitX(hwnd, cxTargetApplicationDlg, IDC_RESIZEGRIP4);
		// ToolTip for browse button
		HWND hwndToolTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, 0, 0, 0, 0, 0, hwnd, NULL, g_hInstance, NULL);

		TOOLINFO ti;
		ZeroMemory(&ti, sizeof(TOOLINFO));
		ti.cbSize   = sizeof(TOOLINFO);
		ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
		ti.hwnd     = hwnd;
		ti.uId      = (UINT_PTR)GetDlgItem(hwnd, IDC_BROWSE);
		ti.hinst    = g_hInstance;
		ti.lpszText = (LPWSTR)IDS_SEARCHEXE;

		if (!SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti)) {
			DestroyWindow(hwndToolTip);
		}

		// ToolTip for find window button
		//hwndToolTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, 0, 0, 0, 0, 0, hwnd, NULL, g_hInstance, NULL);
		//ZeroMemory(&ti, sizeof(TOOLINFO));
		//ti.cbSize   = sizeof(TOOLINFO);
		//ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
		//ti.hwnd     = hwnd;
		//ti.uId      = (UINT)GetDlgItem(hwnd, IDC_FINDWIN);
		//ti.hinst    = g_hInstance;
		//ti.lpszText = (LPWSTR)IDS_FINDWINTITLE;
		//
		//if (!SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti)) {
		//	DestroyWindow(hwndToolTip);
		//}

		// Bitmap for the Browse button
		MakeBitmapButton(hwnd, IDC_BROWSE, g_hInstance, IDB_OPEN);
		//MakeBitmapButton(hwnd, IDC_FINDWIN, g_hInstance, IDB_BROWSE);

		// initialize edit controls
		SendDlgItemMessage(hwnd, IDC_TARGETPATH, EM_LIMITTEXT, MAX_PATH - 1, 0);
		SHAutoComplete(GetDlgItem(hwnd, IDC_TARGETPATH), SHACF_FILESYSTEM | SHACF_URLMRU);

		SendDlgItemMessage(hwnd, IDC_DDEMSG, EM_LIMITTEXT, 128, 0);
		SendDlgItemMessage(hwnd, IDC_DDEAPP, EM_LIMITTEXT, 128, 0);
		SendDlgItemMessage(hwnd, IDC_DDETOPIC, EM_LIMITTEXT, 128, 0);

		if (!bLoadLaunchSetingsLoaded) {
			LoadLaunchSetings();
		}
		if (iUseTargetApplication) {
			CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
		} else {
			CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_LAUNCH);
		}

		WCHAR wch[MAX_PATH];
		lstrcpy(wch, szTargetApplication);
		PathQuoteSpaces(wch);
		if (StrNotEmpty(szTargetApplicationParams)) {
			StrCatBuff(wch, L" ", COUNTOF(wch));
			StrCatBuff(wch, szTargetApplicationParams, COUNTOF(wch));
		}
		SetDlgItemText(hwnd, IDC_TARGETPATH, wch);

		if (iUseTargetApplication) {
			const int i = clamp_i(iTargetApplicationMode, 0, 2);
			CheckRadioButton(hwnd, IDC_ALWAYSRUN, IDC_USEDDE, IDC_ALWAYSRUN + i);
		}

		lstrcpy(szTargetWndClass, szTargetApplicationWndClass);

		SetDlgItemText(hwnd, IDC_DDEMSG, szDDEMsg);
		SetDlgItemText(hwnd, IDC_DDEAPP, szDDEApp);
		SetDlgItemText(hwnd, IDC_DDETOPIC, szDDETopic);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxTargetApplicationDlg, NULL);
		DeleteBitmapButton(hwnd, IDC_BROWSE);
		//DeleteBitmapButton(hwnd, IDC_FINDWIN);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, NULL);
		HDWP hdwp = BeginDeferWindowPos(8);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP4, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_TARGETPATH, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_BROWSE, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_DDEMSG, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_DDEAPP, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_DDETOPIC, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BROWSE: {
			WCHAR tchBuf[MAX_PATH];
			WCHAR szFile[MAX_PATH];
			WCHAR szParams[MAX_PATH];

			GetDlgItemText(hwnd, IDC_TARGETPATH, tchBuf, COUNTOF(tchBuf));
			ExtractFirstArgument(tchBuf, szFile, szParams);
			PathAbsoluteFromApp(szFile, szFile, COUNTOF(szFile), TRUE);

			WCHAR szTitle[32];
			WCHAR szFilter[256];
			GetString(IDS_SEARCHEXE, szTitle, COUNTOF(szTitle));
			GetString(IDS_FILTER_EXE, szFilter, COUNTOF(szFilter));
			PrepareFilterStr(szFilter);

			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner   = hwnd;
			ofn.lpstrFilter = szFilter;
			ofn.lpstrFile   = szFile;
			ofn.nMaxFile    = COUNTOF(szFile);
			ofn.lpstrTitle  = szTitle;
			ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
							  OFN_PATHMUSTEXIST | OFN_SHAREAWARE | OFN_NODEREFERENCELINKS | OFN_NOVALIDATE;

			// execute file open dlg
			if (GetOpenFileName(&ofn)) {
				lstrcpyn(tchBuf, szFile, COUNTOF(tchBuf));
				PathRelativeToApp(tchBuf, tchBuf, COUNTOF(tchBuf), TRUE, TRUE, flagPortableMyDocs);
				PathQuoteSpaces(tchBuf);
				if (StrNotEmpty(szParams)) {
					StrCatBuff(tchBuf, L" ", COUNTOF(tchBuf));
					StrCatBuff(tchBuf, szParams, COUNTOF(tchBuf));
				}
				SetDlgItemText(hwnd, IDC_TARGETPATH, tchBuf);
			}

			// set focus to edit control
			PostMessage(hwnd, WM_NEXTDLGCTL, 1, 0);

			CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
		}
		break;

		//case IDC_COMMANDLINE: {
		//	BOOL bEnableOK = FALSE;
		//	WCHAR tchArgs[MAX_PATH * 2];
		//
		//	if (GetDlgItemText(hwnd, IDC_COMMANDLINE, tchArgs, COUNTOF(tchArgs))) {
		//		if (ExtractFirstArgument(tchArgs, tchArgs, NULL)) {
		//			if (StrNotEmpty(tchArgs)) {
		//				bEnableOK = TRUE;
		//			}
		//		}
		//	}
		//
		//	// OK Button enable and disable
		//	EnableWindow(GetDlgItem(hwnd, IDOK), bEnableOK);
		//}
		//break;

		case IDC_LAUNCH:
			CheckRadioButton(hwnd, IDC_ALWAYSRUN, IDC_USEDDE, 0);
			break;

		case IDC_TARGET:
			CheckRadioButton(hwnd, IDC_ALWAYSRUN, IDC_USEDDE, IDC_ALWAYSRUN);
			break;

		case IDC_TARGETPATH:
			if (HIWORD(wParam) == EN_SETFOCUS) {
				CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
			}
			break;

		case IDC_ALWAYSRUN:
		case IDC_SENDDROPMSG:
		case IDC_USEDDE:
			CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
			break;

		case IDC_DDEMSG:
		case IDC_DDEAPP:
		case IDC_DDETOPIC:
			if (HIWORD(wParam) == EN_SETFOCUS) {
				CheckRadioButton(hwnd, IDC_ALWAYSRUN, IDC_USEDDE, IDC_USEDDE);
				CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
			}
			break;

		case IDC_FINDWIN: {
			ShowWindow(hwnd, SW_HIDE);
			ShowWindow(hwndMain, SW_HIDE);

			ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_FINDWIN), hwnd, FindWinDlgProc, (LPARAM)szTargetWndClass);

			ShowWindow(hwndMain, SW_SHOWNORMAL);
			ShowWindow(hwnd, SW_SHOWNORMAL);

			CheckRadioButton(hwnd, IDC_ALWAYSRUN, IDC_USEDDE, IDC_SENDDROPMSG);
			CheckRadioButton(hwnd, IDC_LAUNCH, IDC_TARGET, IDC_TARGET);
		}
		return FALSE;

		case IDOK: {
			WCHAR tch[MAX_PATH];

			// input validation
			if ((IsButtonChecked(hwnd, IDC_TARGET) && GetDlgItemText(hwnd, IDC_TARGETPATH, tch, COUNTOF(tch)) == 0) ||
					(IsButtonChecked(hwnd, IDC_SENDDROPMSG) && StrIsEmpty(szTargetWndClass)) ||
					(IsButtonChecked(hwnd, IDC_USEDDE) &&
					 (GetDlgItemText(hwnd, IDC_DDEMSG, tch, COUNTOF(tch)) == 0 ||
					  GetDlgItemText(hwnd, IDC_DDEAPP, tch, COUNTOF(tch)) == 0 ||
					  GetDlgItemText(hwnd, IDC_DDETOPIC, tch, COUNTOF(tch)) == 0))) {
				ErrorMessage(1, IDS_ERR_INVALIDTARGET);
			} else {
				IniSectionOnSave section;
				WCHAR *pIniSectionBuf = (WCHAR *)NP2HeapAlloc(sizeof(WCHAR) * MAX_INI_SECTION_SIZE_TARGET_APPLICATION);
				IniSectionOnSave *pIniSection = &section;
				pIniSection->next = pIniSectionBuf;

				int i = IsButtonChecked(hwnd, IDC_LAUNCH);
				iUseTargetApplication = !i;
				IniSectionSetBool(pIniSection, L"UseTargetApplication", iUseTargetApplication);

				if (iUseTargetApplication) {
					GetDlgItemText(hwnd, IDC_TARGETPATH, tch, COUNTOF(tch));
					ExtractFirstArgument(tch, szTargetApplication, szTargetApplicationParams);
				} else {
					lstrcpy(szTargetApplication, L"");
					lstrcpy(szTargetApplicationParams, L"");
				}
				IniSectionSetString(pIniSection, L"TargetApplicationPath", szTargetApplication);
				IniSectionSetString(pIniSection, L"TargetApplicationParams", szTargetApplicationParams);

				if (!iUseTargetApplication) {
					iTargetApplicationMode = 0;
				} else {
					if (IsButtonChecked(hwnd, IDC_ALWAYSRUN)) {
						iTargetApplicationMode = 0;
					} else if (IsButtonChecked(hwnd, IDC_SENDDROPMSG)) {
						iTargetApplicationMode = 1;
					} else {
						iTargetApplicationMode = 2;
					}
				}
				IniSectionSetInt(pIniSection, L"TargetApplicationMode", iTargetApplicationMode);

				if (IsButtonChecked(hwnd, IDC_SENDDROPMSG) && !i) {
					lstrcpy(szTargetApplicationWndClass, szTargetWndClass);
				} else {
					lstrcpy(szTargetApplicationWndClass, L"");
				}
				IniSectionSetString(pIniSection, L"TargetApplicationWndClass", szTargetApplicationWndClass);

				i = IsButtonChecked(hwnd, IDC_USEDDE);
				if (i) {
					GetDlgItemText(hwnd, IDC_DDEMSG, szDDEMsg, COUNTOF(szDDEMsg));
				} else {
					lstrcpy(szDDEMsg, L"");
				}
				IniSectionSetString(pIniSection, L"DDEMessage", szDDEMsg);

				if (i) {
					GetDlgItemText(hwnd, IDC_DDEAPP, szDDEApp, COUNTOF(szDDEApp));
				} else {
					lstrcpy(szDDEApp, L"");
				}
				IniSectionSetString(pIniSection, L"DDEApplication", szDDEApp);

				if (i) {
					GetDlgItemText(hwnd, IDC_DDETOPIC, szDDETopic, COUNTOF(szDDETopic));
				} else {
					lstrcpy(szDDETopic, L"");
				}
				IniSectionSetString(pIniSection, L"DDETopic", szDDETopic);

				SaveIniSection(INI_SECTION_NAME_TARGET_APPLICATION, pIniSectionBuf);
				NP2HeapFree(pIniSectionBuf);

				EndDialog(hwnd, IDOK);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;

		}
		return TRUE;
	}

	return FALSE;
}
