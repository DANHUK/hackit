// TrayMan.cpp : Defines the initialization routines for the DLL.
//

#include <windows.h>
#include <tchar.h>
#include "TrayMan.h"
#include "../resource.h"
#include "../defines.h"
#include "../LoggingSystem.h"

#pragma data_seg(".SHELLHOOKDATA")
static HWND HackItWnd;
static HHOOK cbt = NULL;
static HHOOK sysmsg = NULL;
static HHOOK shell = NULL;
#pragma data_seg()
#ifdef DEBUG
DWORD dwDebuggerProcId = 0;
#endif // _DEBUG
#pragma data_seg()
#define PATHSIZE 256
#define DEBUGGER _TEXT("MSDEV.EXE") // Change this to the name of the debugger, all caps (i.e. MSDEV.EXE)
HINSTANCE hInstance;
BOOL g_SysMenuCapture = FALSE;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID reserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		::hInstance = hInstance;
	}

	return TRUE;
}

LRESULT CALLBACK CBTHook(int nCode, WPARAM wParam, LPARAM lParam) {
	static HWND HackItWnd = NULL;
	if (HackItWnd == NULL) {
		HackItWnd = ::FindWindow("HackItListManager", NULL);
	}

	if (HackItWnd == NULL || !IsWindow(HackItWnd)) {
		// Hack-It is no longer running, call next hook and unhook ourselves
		int result = CallNextHookEx(cbt, nCode, wParam, lParam);
		UnhookWindowsHookEx(cbt);
		cbt = NULL;
		return result;
	}

	switch (nCode) {
	case HCBT_MINMAX:

		// Check if window being minimized is a tray window
		if (::SendMessage(HackItWnd, WM_USER+2, wParam, lParam)) {
			if (LOWORD(lParam) == SW_MINIMIZE) {
				::ShowWindow((HWND)wParam, SW_HIDE);
				return 1;
			}
		}

		break;
	case HCBT_SYSCOMMAND:
		switch (wParam & 0xFFF0) {
		case IDM_VISIBLEHIDDEN:
		case IDM_TOPPEDUNTOPPED:
		case IDM_TRAYYEDUNTRAYYED:
		case IDM_HIGHESTPRIORITY:
		case IDM_HIGHPRIORITY:
		case IDM_NORMALPRIORITY:
		case IDM_LOWPRIORITY:
		case IDM_TRANSPARENCY_10:
		case IDM_TRANSPARENCY_20:
		case IDM_TRANSPARENCY_30:
		case IDM_TRANSPARENCY_40:
		case IDM_TRANSPARENCY_50:
		case IDM_TRANSPARENCY_60:
		case IDM_TRANSPARENCY_70:
		case IDM_TRANSPARENCY_80:
		case IDM_TRANSPARENCY_90:
		case IDM_TRANSPARENCY_100:
		case IDM_TRANSPARENCY_NONE:
			::PostMessage(HackItWnd, WM_HACKITFUNCTIONDETECTED, wParam, lParam);
			break;
		}
		break;
	case HCBT_CREATEWND:
		::PostMessage(HackItWnd, WM_CREATEDETECTED, (WPARAM)((CBT_CREATEWND*)lParam)->lpcs, wParam);
		GetSystemMenu((HWND)wParam, FALSE);
		break;
	case HCBT_DESTROYWND:
		::PostMessage(HackItWnd, WM_DESTROYDETECTED, lParam, wParam);
		break;
	} 
	return CallNextHookEx(cbt, nCode, wParam, lParam);
}

#ifdef DEBUG
// *** DO NOT PUT ANY BREAKPOINTS IN THIS CODE!!!
BOOL IsCurProcDebugger()
{
  // Do this first for speed.
  // If there is only one debugger present
  // and you have already found it.
  if ( dwDebuggerProcId )
  {
     return ( GetCurrentProcessId() == dwDebuggerProcId );
  }

  // In Visual C++, declare these three variables after the
  // following if-block to speed things up.
  _TCHAR szPathName[PATHSIZE];
  _TCHAR *szFileName = szPathName;
  _TCHAR *tcp;

  // If only one debugger is running, then the rest of this code
  // should be entered only until the debugger is first hooked.
  // After that, the preceding code should catch it every time.

  GetModuleFileName( NULL, szPathName, PATHSIZE );

  // Only check the file name, not the full path.
  // A co-worker's path may be different.

  for ( tcp = szPathName; *tcp; tcp++ )
  {
     if ( *tcp == '/' || *tcp == '\\' )
        szFileName = tcp + 1;
  }

  // Use _TEXT("MSDEV.EXE") for the Visual C++ debugger, or
  // else use YOUR debugger's name.

  if ( !_tcscmp( _tcsupr(szFileName), DEBUGGER) )
  {
     // It's the debugger!
     dwDebuggerProcId = GetCurrentProcessId();
     return TRUE;
  }

  return FALSE;
}
#endif  // DEBUG 

LRESULT CALLBACK CallWndProcRetHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	static HWND HackItWnd = NULL;
#ifdef DEBUG
  if ( !IsCurProcDebugger() ) // If you are not in the debugger...
  {
#endif // DEBUG
	if (nCode >= 0) {
		if (nCode == HC_ACTION) {
			if (lParam != NULL) {
				if (HackItWnd == NULL) {
					HackItWnd = ::FindWindow("HackItListManager", NULL);
				}

				if (HackItWnd == NULL || !IsWindow(HackItWnd)) {
					// Hack-It is no longer running, call next hook and unhook ourselves
					int result = CallNextHookEx(sysmsg, nCode, wParam, lParam);
					UnhookWindowsHookEx(sysmsg);
					sysmsg = NULL;
					return result;
				}

				LPCWPRETSTRUCT cwp = (LPCWPRETSTRUCT)lParam;
				switch (cwp->message) {
				case WM_WINDOWPOSCHANGING:
					if (!(GetWindowLong(cwp->hwnd, GWL_STYLE) & WS_CHILD)) {
						::PostMessage(HackItWnd, WM_POSCHANGEDDETECTED, cwp->lParam, (UINT)cwp->hwnd);
					}
					break;
				case WM_SETTEXT:
					::PostMessage(HackItWnd, WM_SETTEXTDETECTED, cwp->lParam, (UINT)cwp->hwnd);
					break;
				case WM_EXITMENULOOP:
					if (!cwp->wParam) {
						::SendMessageTimeout(HackItWnd, WM_SYSMENUENDED, cwp->lParam, (UINT)cwp->hwnd, SMTO_ABORTIFHUNG, 100, NULL);
					}
					break;
				case WM_INITMENUPOPUP:
					if (HIWORD(cwp->lParam) && !LOWORD(cwp->lParam)) {
						::SendMessageTimeout(HackItWnd, WM_SYSMENUDETECTED, (WPARAM)cwp->wParam, (LPARAM)cwp->hwnd, SMTO_ABORTIFHUNG, 100, NULL);
						//::PostMessage(HackItWnd, WM_SYSMENUDETECTED, (WPARAM)cwp->wParam, (LPARAM)cwp->hwnd);
					}
					break;
				}
			}
		}
	}
#ifdef DEBUG
  }
#endif // DEBUG

	return CallNextHookEx(sysmsg, nCode, wParam, lParam);
}

BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam) {
	::PostMessage((HWND)lParam, WM_CREATEDETECTED, NULL, (LPARAM)hwnd);
	
	return TRUE;
}

BOOL CALLBACK EnumParentWindowsProc(HWND hwnd, LPARAM lParam) {
	::PostMessage((HWND)lParam, WM_CREATEDETECTED, NULL, (LPARAM)hwnd);
	EnumChildWindows(hwnd, EnumChildWindowsProc, lParam);

	return TRUE;
}

VOID CALLBACK MySendAsyncProc(/*HWND hwnd, UINT msg, ULONG_PTR dwData, LRESULT lresult*/) {
	// Hack-It has finished processing the initial list populator messages, load the hook
	
	if (cbt == NULL) {
		cbt = SetWindowsHookEx(WH_CBT, CBTHook, hInstance, 0);
	}
}

extern "C" void StartCBTHook(HWND wnd)
{
	HackItWnd = wnd;

	// enumerate windows for initial population
	EnumWindows(EnumParentWindowsProc, (LPARAM)wnd);

	// wait for hack-it to process posted message before starting hook
	//SendMessageCallback(wnd, WM_NULL, 0, 0, SendAsyncProc, 0);
	MySendAsyncProc(/*NULL, 0, NULL, 0*/);
}

extern "C" void StopCBTHook()
{
	if (cbt != NULL) {
		UnhookWindowsHookEx(cbt);
	}

	cbt = NULL;
}

extern "C" void StartCallWndProcRetHook(HWND wnd)
{
	HackItWnd = wnd;
	if (sysmsg == NULL) {
		sysmsg = SetWindowsHookEx(WH_CALLWNDPROCRET, CallWndProcRetHook, hInstance, 0);
	}
}

extern "C" void StopCallWndProcRetHook()
{
	if (sysmsg != NULL) {
		UnhookWindowsHookEx(sysmsg);
	}

	sysmsg = NULL;
}
