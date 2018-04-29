// drTitle.C - change another process's window title
//
// You can use the following program to learn the class name and window name used by an application
//	C:\Program Files (x86)\Microsoft Visual Studio 8\Common7\Tools\spyxx.exe
//	C:\msdev.6.0\Tools\SPYXX.EXE
// Note you must drag the "Window Finder" onto the application window's title area

//"Boost Regular Expression Syntax"
//http://www.boost.org/doc/libs/1_55_0/libs/regex/doc/html/boost_regex/syntax.html
//http://www.boost.org/doc/libs/1_55_0/libs/regex/doc/html/boost_regex/syntax/perl_syntax.html

#define APP_NAME "drTitle"
#define VERS_STR " (V1.02)"

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

const size_t BUFSIZE = 256;

//globals
int _debug = 0;
int _listMode = 0;
vector<string> _visibleWindows; //vector of visible windows, used with _listMode


///////////////////////////////////////////////////////////////////////////////
void printUsageAndDie (char *message = 0)
{
	if (message) {
		fprintf (stderr, "\n%s: %s; use /h for help\n", APP_NAME, message);
	} else {
		fprintf (stderr, "\nUsage: %s [/h] [/d] [/c[=]<className>] <oldWindowName> <newWindowName>\n", APP_NAME);
	}

	exit (1);
}

///////////////////////////////////////////////////////////////////////////////
void checkArgAndDieOnError (char arg[], size_t size, char leading)
{
	if (strlen (arg) != size) {
		fprintf (stderr, "%s: unrecognized argument, %c%s\n", APP_NAME, leading, arg);
		printUsageAndDie ();
	}
}

////////////////////////////////////////////////////////////////////////////////
string wrapRegex (const string &strIn) //convert to regex with 'match anywhere in string' and ignore case
{
	string str = strIn;

	//TODO - if (matchAsWild)
	str = "(.*)" + str + "(.*)";

	//TODO - if (ignoreCase)
	str = "(?i)" + str;

	return str;
}

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

		char *commandPromptClass = "ConsoleWindowClass";
		if (strcmp (commandPromptClass, className) == 0) {
			if (_debug) {
				printf ("skipping Command Prompt\n");
			}
			return TRUE; //tell caller to continue searching
		}

		if (_listMode) { //create list of every window examined (except Command Prompts)
			char buffer[BUFSIZE];
			sprintf (buffer, "WindowText: \"%s\", Class: \"%s\"\n", windowText, className);
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
			MessageBox1 (NULL, err, APP_NAME, MB_OK | MB_ICONERROR);
			return FALSE; //tell caller to discontinue searching
		}

		if (boost::regex_match (windowText, windowTextPattern) && boost::regex_match (className, classNamePattern)) {
			HWND hWnd2 = FindWindow (className, windowText);

			if (hWnd2) {
				pData->hWnd = hWnd2;
				if (_debug) {
					printf ("found window matching name \"%s\" (\"%s\", \"%s\")\n", pData->winName, windowText, pData->winClass);
				}
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

	return data.hWnd;
}

////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
//TODO - remove hardcoded default ???
	string className;
	string oldWindowName;
	string newWindowName;

	//set debug flag from environment
	_debug = (getenv ("DEBUG") == NULL ? 0 : 1);

	int ignoreCase = 1;
	int matchAsWild = 1;
	int c0, c1;
	while (argc > 1 && ((c0 = **++argv) == '-' || c0 == '/')) {
		--argc;
		switch (c1 = (*++*argv | 040)) { //assign!
			case 'd':
				checkArgAndDieOnError (*argv, 1, c0);
				_debug = 1;
				FileIoDebug = 1;
				break;

			case 'c': //specify classname
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':') {
					++*argv;
				}
				if (**argv) {
					className = *argv;
				} else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'h':
				printUsageAndDie ();
				break;

			case 'l':
				checkArgAndDieOnError (*argv, 1, c0);
				_listMode = 1;
				break;

			default:
				{
				char buf[64];
				sprintf (buf, "unrecognized argument, %c%s", c0, *argv);
				printUsageAndDie (buf);
				}
		}
	}

	if (_listMode) {
		myFindWindowWild (".*", ".*");

		cout << "Z-order list of visible windows (except Command Prompts):" << endl;
		for (vector<string>::iterator it = _visibleWindows.begin (); it != _visibleWindows.end (); ++it)
			cout << *it;

		return 0;
	}

	//next parameter is old title
	if (--argc > 0) {
		oldWindowName = *argv++;
	} else {
		printUsageAndDie ("incorrect usage");
	}

	//next parameter is new title
	if (--argc > 0) {
		newWindowName = *argv++;
	} else {
		printUsageAndDie ("incorrect usage");
	}

	//next parameters is syntax error!
	if (--argc > 0) {
		printUsageAndDie ("incorrect usage");
	}

	className = wrapRegex (className);
	oldWindowName = wrapRegex (oldWindowName);

	if (_debug) {
		printf ("className = \"%s\"\n", className.c_str ());
		printf ("oldWindowName = \"%s\"\n", oldWindowName.c_str ());
		printf ("newWindowName = \"%s\"\n", newWindowName.c_str ());
	}

	HWND hWnd = myFindWindowWild ((LPSTR) className.c_str (), (LPSTR) oldWindowName.c_str ());

	if (hWnd == NULL) {
		char err[BUFSIZE];
		//NOTE: I changed the order of the lpWindowName/lpClass parameters in this error message to make it easier to read
		sprintf (err, "FindWindow(\"%s\", \"%s\") failed: %s", oldWindowName.c_str (), className.c_str (), formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, APP_NAME, MB_OK | MB_ICONERROR);
		return false;
	}

	if (!SendMessage (hWnd, WM_SETTEXT, 0, (LPARAM) newWindowName.c_str ())) {
		char err[BUFSIZE];
		sprintf (err, "SendMessage(\"%s\", \"%s\") failed: %s", WM_SETTEXT, newWindowName.c_str (), formatLastError (GetLastError ()));
		MessageBox1 (NULL, err, APP_NAME, MB_OK | MB_ICONERROR);
		return false;
	}

	return 0;
}
