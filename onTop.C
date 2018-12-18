// onTop.cpp
//
// Build command:
//  g++ -o onTop.exe onTop.C FileIO.C -lgdi32 -lboost_regex
//	g++ -o onTop.exe /cygdrive/c/users/bin/onTop.C /cygdrive/c/users/bin/FileIO.C -lgdi32 -lboost_regex
//
// Example command line:
//	set PATH=%PATH%;C:\cygwin64\bin
//  C:\cygwin64\home\feral\onTop /d /t "Administrator: cmd.*" /c "ConsoleWindowClass" 100 100 1000 1000
//
// You can use the following program to learn the class name and window name used by an application
//	C:\Program Files (x86)\Microsoft Visual Studio 8\Common7\Tools\spyxx.exe
//	C:\msdev.6.0\Tools\SPYXX.EXE
// Note you must drag the "Window Finder" onto the application window's title area

//Note the command prompts show up like this:
//lpClass = "ConsoleWindowClass"
//lpWindowName = "1", "2", ...

//Hack for Windows 2008 (names have two leading spaces):
//lpWindowName = "  1", "  2", ...
//see http://serverfault.com/questions/35561/how-to-remove-administrator-from-the-command-promt-title

//"Boost Regular Expression Syntax"
//http://www.boost.org/doc/libs/1_55_0/libs/regex/doc/html/boost_regex/syntax.html
//http://www.boost.org/doc/libs/1_55_0/libs/regex/doc/html/boost_regex/syntax/perl_syntax.html

#include <windows.h>
#include <stdio.h>
#include <iostream> //for cout
#include <string>
#include <vector>
#include <boost/regex.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "fileio.h"

using namespace std;

typedef struct tagWINDATA {
	LPSTR winClass;
	LPSTR winName;
	HWND hWnd;
} WINDATA, *PWINDATA;

enum BaseCoord {UpperLeft, LowerRight};
const size_t BUFSIZE = 256;

//globals
int _debug = 0;
int _listMode = 0;
vector<string> _visibleWindows; //vector of visible windows, used with _listMode


/////////////////////////////////////////////////////////////////////////////
class lessNoCase {
public:
	bool operator () (const string& s1, const string& s2) {
		return strcasecmp (s1.c_str (), s2.c_str ()) < 0;
	}
};

////////////////////////////////////////////////////////////////////////////////
void MessageBox1 (char *unused1, char *err, char *header, UINT unused2)
{
	fprintf (stderr, err);
}

////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK myEnumProc (HWND hWnd1, LPARAM lParam)
{
	PWINDATA pData = (PWINDATA) lParam;

	if (IsWindowVisible (hWnd1)) {
		char windowText[BUFSIZE];
		GetWindowText (hWnd1, windowText, BUFSIZE);
		//TODO - check for errors

		char className[BUFSIZE];
		GetClassName (hWnd1, className, BUFSIZE);
		//TODO - check for errors

		if (_listMode) { //create list of every window examined
			RECT rect;
			if (!GetWindowRect (hWnd1, &rect)) {
				char err[BUFSIZE];
				sprintf (err, "GetWindowRect(\"%s\") failed: %s", windowText, formatLastError (GetLastError ()));
				MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONWARNING); //not fatal
			}

			boolean minimized = IsIconic (hWnd1);

			char buffer[BUFSIZE];
			sprintf (buffer, "WindowText: \"%s\", Class: \"%s\", L:%d, T:%d, R:%d, B:%d, W:%d, H:%d, Min:%s\n",
					 windowText, className,
					 rect.left, rect.top, rect.right, rect.bottom,
					 rect.right - rect.left, rect.bottom - rect.top,
					 minimized ? "Y" : "N");

			_visibleWindows.push_back (buffer);

			return TRUE; //tell caller to continue searching
		}

		boost::regex windowTextPattern;
		boost::regex classNamePattern;
		try {
			windowTextPattern = pData->winName;
			classNamePattern = pData->winClass;
		} catch (boost::exception const& ex) {
			printf ("boost::exception: \"%s\"\n", boost::diagnostic_information (ex)); //this doesn't always work; sometimes ex is invalid?

			char err[BUFSIZE];
			sprintf (err, "Error: invalid regular expression: \"%s\" or \"%s\"\n", pData->winName, pData->winClass);
			MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
			return FALSE; //tell caller to discontinue searching
		}

		if (boost::regex_match (windowText, windowTextPattern) && boost::regex_match (className, classNamePattern)) {
			HWND hWnd2 = FindWindow (className, windowText);

			if (hWnd2) {
				pData->hWnd = hWnd2;
				if (_debug)
					printf ("found window matching name \"%s\" (\"%s\", \"%s\")\n", pData->winName, windowText, pData->winClass);
				return FALSE; //tell caller to discontinue searching
			}
		}
	}

	//no matching window found
	SetLastError (ERROR_FILE_NOT_FOUND);
	pData->hWnd = NULL;
	return TRUE; //tell caller to continue searching
}

////////////////////////////////////////////////////////////////////////////////
//returns non-zero on success, 0 on failure
HWND myFindWindowWild (LPSTR lpClass, LPSTR lpWindowName)
{
	WINDATA data;

	data.winClass = lpClass;
	data.winName = lpWindowName;
	data.hWnd = NULL;

	if (_debug) {
		printf ("\n");
	}

	EnumWindows (myEnumProc, (LPARAM) &data);

/*
	//workaround Win2008 Command Prompt title issue
	if (data.hWnd || _listMode)
		return data.hWnd;

	//handle Windows 2008 title
	char newWindowName[BUFSIZE];
//	sprintf (newWindowName, "Administrator* %s", lpWindowName);
	sprintf (newWindowName, "  %s", lpWindowName); //two spaces before title

	data.winClass = lpClass;
	data.winName = (LPSTR) newWindowName;
	data.hWnd = NULL;

//	if (_debug)
//		printf ("myFindWindowWild: looking for ConsoleWindowClass with Windows 2008 title = \"%s\"\n", data.winName);

	EnumWindows (myEnumProc, (LPARAM) &data);
*/

	return data.hWnd;
}

/* unused
////////////////////////////////////////////////////////////////////////////////
void setOnTop (LPSTR lpWindowName, LPSTR lpClass, HWND hWndInsertAfter)
{
	HWND hWnd = FindWindow (lpClass, lpWindowName);

	if (hWnd == NULL) {
		char err[BUFSIZE];
		sprintf (err, "FindWindow(\"%s\", \"%s\") failed: %s", lpClass, lpWindowName, formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
		return;
	}

	if (!SetWindowPos (hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE)) {
		char err[BUFSIZE];
		sprintf (err, "SetWindowPos(\"%s\") failed: %s", lpWindowName, formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
		return;
	}
}
*/

/* unused for now
////////////////////////////////////////////////////////////////////////////////
//"Using SetForegroundWindow on Windows Owned by Other Processes"
//original from http://www.devx.com/tips/Tip/42563
void NewSetForegroundWindow (HWND hWnd)
{
	if (GetForegroundWindow () != hWnd) {
		DWORD dwMyThreadID = GetWindowThreadProcessId (GetForegroundWindow (), NULL);
		DWORD dwOtherThreadID = GetWindowThreadProcessId (hWnd, NULL);
		if (dwMyThreadID != dwOtherThreadID) {
			AttachThreadInput (dwMyThreadID, dwOtherThreadID, TRUE);
			SetForegroundWindow (hWnd);
			SetFocus (hWnd);
			AttachThreadInput (dwMyThreadID, dwOtherThreadID, FALSE);
		}
		else
			SetForegroundWindow (hWnd);

		if (IsIconic (hWnd))
			ShowWindow (hWnd, SW_RESTORE);
		else
			ShowWindow (hWnd, SW_SHOW);
	}
}
*/

////////////////////////////////////////////////////////////////////////////////
//moves first window that match (name, class) to the specified location, maintains minimized state
boolean moveWindow (LPSTR lpWindowName, LPSTR lpClass, BaseCoord baseCoord, int x, int y, int w, int h)
{
	HWND hWnd = myFindWindowWild (lpClass, lpWindowName);

	if (_debug) {
		printf ("moveWindow: hWnd=0x%08x, lpWindowName=\"%s\"\n", hWnd, lpWindowName);
	}

	if (hWnd == NULL) {
		char err[BUFSIZE];
		//NOTE: I changed the order of the lpWindowName/lpClass parameters in this error message to make it easier to read
		sprintf (err, "FindWindow(\"%s\", \"%s\") failed: %s", lpWindowName, lpClass, formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
		return false;
	}

	boolean minimized = IsIconic (hWnd);

	if (minimized) {
		if (!ShowWindow (hWnd, SW_SHOWNORMAL)) {
			char err[BUFSIZE];
			sprintf (err, "ShowWindow(\"%s\", SW_SHOWNORMAL) failed: %s", lpWindowName, formatLastError (GetLastError ()));
			MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
			return false;
		}
	}

	RECT rect;
	if (!GetWindowRect (hWnd, &rect)) {
		char err[BUFSIZE];
		sprintf (err, "GetWindowRect(\"%s\") failed: %s", lpWindowName, formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
		return false;
	}

	if (_debug) {
		printf ("moveWindow: (bef) L:%d, T:%d, R:%d, B:%d, W:%d, H:%d, Min:%s\n",
				rect.left, rect.top, rect.right, rect.bottom, rect.right - rect.left, rect.bottom - rect.top, minimized ? "Y" : "N");
	}

	if (w < 0) {
		w = rect.right - rect.left; //use current width
	}
	if (h < 0) {
		h = rect.bottom - rect.top; //use current height
	}
	if (baseCoord == LowerRight) {
		x -= w;
		y -= h;
	}

	int oldX = rect.left;
	int oldY = rect.top;
	int oldW = rect.right - rect.left;
	int oldH = rect.bottom - rect.top;

	boolean windowNeedsMoved = !(oldX == x && oldY == y && oldW == w && oldH == h);

	if (_debug) {
		if (windowNeedsMoved) {
			printf ("moveWindow: windowNeedsMoved:true\n");
		}
	}

	if (windowNeedsMoved) {
		UINT uFlags = SWP_NOACTIVATE | SWP_NOZORDER;
		if (!SetWindowPos (hWnd, NULL, x, y, w, h, uFlags)) {
			char err[BUFSIZE];
			sprintf (err, "SetWindowPos(\"%s\") failed: %s", lpWindowName, formatLastError (GetLastError ()));
			MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONERROR);
			return false;
		}
	}

	if (_debug) {
		if (!GetWindowRect (hWnd, &rect)) {
			char err[BUFSIZE];
			sprintf (err, "GetWindowRect(\"%s\") failed: %s", lpWindowName, formatLastError (GetLastError ()));
			MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONWARNING); //not fatal
		}

		printf ("moveWindow: (aft) L:%d, T:%d, R:%d, B:%d, W:%d, H:%d, Min:%s\n",
				rect.left, rect.top, rect.right, rect.bottom, rect.right - rect.left, rect.bottom - rect.top, minimized ? "Y" : "N");
	}

	if (minimized) { //restore minimized state
		if (!ShowWindow (hWnd, SW_MINIMIZE)) {
			char err[BUFSIZE];
			sprintf (err, "ShowWindow(\"%s\", SW_MINIMIZE) failed: %s", lpWindowName, formatLastError (GetLastError ()));
			MessageBox1 (NULL, err, "onTop", MB_OK | MB_ICONWARNING); //not fatal
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
//TODO improve arg handling (e.g., error on bad args, etc.)

	if (argc > 1 && (!strcasecmp (argv[1], "/d") || !strcasecmp (argv[1], "/debug"))) {
		--argc;
		argv++;
		_debug = 1;
	}

	if (argc > 1 && (!strcasecmp (argv[1], "/l") || !strcasecmp (argv[1], "/list"))) {
		--argc;
		argv++;
		_listMode = 1;
	}

	char szWindowText[BUFSIZE] = "*";
	if (argc > 1 && (!strcasecmp (argv[1], "/t") || !strcasecmp (argv[1], "/text"))) {
		--argc;
		argv++;
		strcpy (szWindowText, *++argv);
	}

	char szWindowClass[BUFSIZE] = "*";
	if (argc > 1 && (!strcasecmp (argv[1], "/c") || !strcasecmp (argv[1], "/class"))) {
		--argc;
		argv++;
		strcpy (szWindowClass, *++argv);
	}

	if (_listMode) {
		myFindWindowWild (".*", ".*");

		sort (_visibleWindows.begin (), _visibleWindows.end (), lessNoCase ());

		cout << "Sorted list of visible windows:" << endl;
		for (vector<string>::iterator it = _visibleWindows.begin (); it != _visibleWindows.end (); ++it)
			cout << *it;

		return 0;
	}

	char szBuffer[BUFSIZE] = "*";
	int left = 100;
	int top = 100;
	int width = 1000;
	int height = 1000;
	if (argc > 0) {
		--argc;
		sscanf (*++argv, "%d", &left);
	}
	if (argc > 0) {
		--argc;
		sscanf (*++argv, "%d", &top);
	}
	if (argc > 0) {
		--argc;
		sscanf (*++argv, "%d", &width);
	}
	if (argc > 0) {
		--argc;
		sscanf (*++argv, "%d", &height);
	}

	if (_debug) {
		printf ("WindowText: \"%s\", Class: \"%s\", L:%d, T:%d, W:%d, H:%d\n",
				 szWindowText, szWindowClass, left, top, width, height);
	}

	moveWindow (szWindowText, szWindowClass, UpperLeft, left, top, width, height);

	return 0;
}
