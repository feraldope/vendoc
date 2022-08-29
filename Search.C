//Search.C - searches files for strings

#define APP_NAME "search"
#define VERS_STR "(V5.02)"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "fileio.h"

#define BUFSZ  (64 * 1024) //maximum length of line that can be read without splitting
#define MAXSTRINGS	   24  //maximum number of input strings accepted
#define MATCH_AND		1
#define MATCH_OR		0
#define TABSIZE			4

typedef struct tagMATCHDATA {
	int ptr;
	int len;
	} MATCHDATA;

int debug;
HANDLE hStdout;
WORD wBaseColor, wHighlightColor;

const int iTimeOrderedOutputSize = (20 * 1024);
char *pTimeOrderedOutput[iTimeOrderedOutputSize];
int iTimeOrderedIndex = 0;
int bShowFileTimes;


////////////////////////////////////////////////////////////////////////////
void printUsageAndDie (char *message = 0)
{
#define NL "\n"

	if (message) {
		fprintf (stderr, APP_NAME);
		fprintf (stderr, ": ");
		fprintf (stderr, message);
		fprintf (stderr, "; use /h for help" NL);

	} else {
		char *usageString =
		NL
		"Usage: " APP_NAME " [<options>] <filespec>[,<filespec>] <string> [<string>] [...]" NL
		NL
		"where <options> are:" NL
		NL
		"/a  - (and) only print lines that contain all strings (opposite of /o)" NL
		"/ab - (atBeginning) only print lines that start with first string" NL
		"/ae - (atEnd) only print lines that end with last string" NL
		"/c  - (caseSensitive) only print lines that match case exactly" NL
		"/d  - (debug) print debug info" NL
		"/i  - (orderIsImportant) print lines if order of strings matches order on command line" NL
		"/e= - (excludeFiles) specifies comma-separated list of files to exclude from search" NL
		"/f= - (files) specifies comma-separated list of files to search" NL
		"/h  - (help) print this usage" NL
		"/l  - (log) print every filename (to stdout) as it is searched" NL
		"/ll - (logLog) print every filename (to stderr) as it is searched" NL
		"/m= - (maxMatches) specifies maximum matching lines to print for each file" NL
		"/n  - (lineNumbers) print line numbers of matching lines" NL
		"/o  - (or) print lines that contain any string (opposite of /a)" NL
		"/q  - (quiet) do not print filenames when matching lines found" NL
		"/r  - (redir) redirect search output to /dev/nul" NL
		"/s  - (subdirs) recurse into subdirectories" NL
		"/sl - (skipLabels) when searching vobs, skip version that are labels" NL
		"/st - (showTimes) prepend file name with time_t value representing file time" NL
		"/t= - (tabSize) specifies number of spaces in tabs" NL
		"/v= - (excludeStrings) specifies comma-separated list of strings to exclude from output" NL
//TODO	"/vc= - (excludeStrings) same as /v, except case sensitive" NL
		"/w= - (window) specifies how many lines after every match are printed" NL
		"/x  - (eXcludeNonSource) skip non-source (binary) files" NL
		NL
		" and files may be comma-separated lists, with wildcards" NL;

		fprintf (stderr, usageString);
	}

	exit (1);
}

///////////////////////////////////////////////////////////////////////////////
int compare1 (const void *ptr1, const void *ptr2)
{
	MATCHDATA *data1 = (MATCHDATA*) ptr1;
	MATCHDATA *data2 = (MATCHDATA*) ptr2;

	return data1->ptr - data2->ptr;
}

//////////////////////////////////////////////////////////////////////////////
int compare2 (const void *ptr1, const void *ptr2)
{
	return strcmp (*(char**) (ptr1), *(char**) (ptr2));
}

////////////////////////////////////////////////////////////////////////////
void writeBytes (const char *str, int idx, int len, WORD attrs)
{
	DWORD cWritten;

	SetConsoleTextAttribute (hStdout, attrs);
	WriteFile (hStdout, &str[idx], len, &cWritten, NULL);
}

////////////////////////////////////////////////////////////////////////////
int __cdecl myPrintf (const char *fmt, ...)
{
	va_list args;
	char str[BUFSZ];

	va_start (args, fmt);
	int ii = vsprintf (str, fmt, args);
	va_end (args);

	if (bShowFileTimes) {
		char *ptr = pTimeOrderedOutput[iTimeOrderedIndex];

		if (ptr) {
			int len = strlen (ptr) + strlen (str) + 1;
			char *temp = new char [len];
			myAssert (temp);

			strcpy (temp, ptr);
			strcat (temp, str);
			pTimeOrderedOutput[iTimeOrderedIndex] = strndup (temp);

			delete [] ptr;

		} else {
			pTimeOrderedOutput[iTimeOrderedIndex] = strndup (str);
		}

	} else {
		printf ("%s", str);
	}

	return ii;
}

////////////////////////////////////////////////////////////////////////////
void printLine (const char *line, MATCHDATA *matchData, int cnt)
{
	qsort (matchData, cnt, sizeof (MATCHDATA), compare1);

	int idx = 0;
	for (int ii = 0; ii < cnt; ii++) {
		int nextHighlighted = matchData[ii].ptr;

		if (idx < nextHighlighted) {
			writeBytes (line, idx, nextHighlighted - idx, wBaseColor);
			idx = nextHighlighted;

			writeBytes (line, idx, matchData[ii].len, wHighlightColor);
			idx += matchData[ii].len;

		} else {
			int len = matchData[ii].len;
			if (idx > nextHighlighted)
				len -= (idx - nextHighlighted);

			if (len > 0) {
				writeBytes (line, idx, len, wHighlightColor);
				idx += len;
			}
		}
	}

	int bytes = strlen (line);
	if (idx < bytes)
		writeBytes (line, idx, bytes - idx, wBaseColor);

	writeBytes ("\n", 0, 1, wBaseColor);
}

#if defined (_MSC_VER)
////////////////////////////////////////////////////////////////////////////
//try to restore original color before exiting
void signalHandler (int signal)
{
//consider handling CTRL_C_EVENT and CTRL_BREAK_EVENT, see:
//http://msdn.microsoft.com/en-us/library/ms683242(VS.85).aspx

#if 0 //from C:\msvc.6.0\include\SIGNAL.H
#define SIGINT          2       /* interrupt */
#define SIGILL          4       /* illegal instruction - invalid function image */
#define SIGFPE          8       /* floating point exception */
#define SIGSEGV         11      /* segment violation */
#define SIGTERM         15      /* Software termination signal from kill */
#define SIGBREAK        21      /* Ctrl-Break sequence */
#define SIGABRT         22      /* abnormal termination triggered by abort call */
#endif

/*
	fflush (stdout);
	fflush (stderr);

	Sleep (1000);

	fflush (stdout);
	fflush (stderr);
*/

//todo - this does not always work
	printf ("\n");
	static char message[] = "interrupt\n";
	writeBytes (message, 0, sizeof (message), wBaseColor);

	printf ("\n");
//	printf ("wBaseColor=0x%04X\n", wBaseColor);
//	printf ("wHighlightColor=0x%04X\n", wHighlightColor);

	for (int ii = 0; ii < 3; ii++)
		printf ("signal handler: %d\n", signal);

//	Sleep (250);
	for (int ii = 0; ii < 3; ii++)
		writeBytes (message, 0, sizeof (message), wBaseColor);

/*
	Sleep (1000);
	fflush (stdout);
	fflush (stderr);
*/

	exit (0);
}
#endif

///////////////////////////////////////////////////////////////////////////////
void checkArgAndDieOnError (char arg[], size_t size, char leading)
{
	if (strlen (arg) != size) {
		fprintf (stderr, "%s: unrecognized argument, %c%s\n", APP_NAME, leading, arg);
		printUsageAndDie ();
	}
}

////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
#if defined (_MSC_VER)
	signal (SIGINT, signalHandler);
//	signal (SIGABRT, signalHandler);
	signal (SIGTERM, signalHandler);
	signal (SIGBREAK, signalHandler);
#endif

	FINDFILEDATA f0;
	FINDFILEDATA* fdFiles = &f0;

	char *p0;
	char szInSpec[MY_MAX_PATH];
	char szFileOnly[MY_MAX_PATH];
	char szPathOnly[MY_MAX_PATH];
	char line[BUFSZ];
	char lowline[BUFSZ];
	char *pStrings[MAXSTRINGS];
	char *pExcludeFileList;
	char *pExcludeStringList;
	int c0, c1, c2, i0, j0, iNumSearchStrings, iRecordsSearched, bHeader;
	int iNumMatchesInFile, iNumFilesSearched, iNumFilesWithMatches;
	int iMaxMatches;
	int iFlags, iWindowCount, eMatchType, iWindowLines, iTabSize, bUnicodeFile;
	int bRecurseSubdirs, iAddlSubdirLevels, bShowLineNumbers, bExcludeNonSource, bCaseSensitive;
	int bOrderInStringIsImportant, bQuietMode, bLogFiles, bLogToStderr, bSkipLabels;
	int bMatchAtBeg, bMatchAtEnd, bExcludeStringCaseSensitive, bNoSearchOutput;
	int iRemapFlags = RMC_TABS | RMC_FORMFEEDS | RMC_CARRIAGECONTROL;
	int bUseConsoleOutput = 0;

	for (int ii = 0; ii < iTimeOrderedOutputSize; ii++)
		pTimeOrderedOutput[ii] = 0;

	char **argvSave = argv;
	int argcSave = argc;

	//check command line parameters for switches
	debug = 0;
	eMatchType = MATCH_AND;
	iTabSize = TABSIZE;
	iWindowLines = 1;
	iMaxMatches = 0; //0 means ignore, and print all matches
	szInSpec[0] = 0;
	pExcludeFileList = pExcludeStringList = NULL;
	bRecurseSubdirs = bShowLineNumbers = bExcludeNonSource = bCaseSensitive = FALSE;
	bOrderInStringIsImportant = bQuietMode = bLogFiles = bLogToStderr = bSkipLabels = FALSE;
	bShowFileTimes = bMatchAtBeg = bMatchAtEnd = bExcludeStringCaseSensitive = bNoSearchOutput = FALSE;
	iAddlSubdirLevels = 999;
	while (argc > 1 && ((c0 = **++argv) == '-' || c0 == '/')) {
		--argc;
		switch (c1 = (*++*argv | 040)) { //assign!
			case 'a':
				c2 = *((*argv)+1);
				if (c2 == 'b') //user specified /ab
					bMatchAtBeg = TRUE;
				else if (c2 == 'e') //user specified /ae
					bMatchAtEnd = TRUE;
				else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else
					eMatchType = MATCH_AND; //user specified /a
				break;

			case 'c':
				checkArgAndDieOnError (*argv, 1, c0);
				bCaseSensitive = TRUE;
				break;

			case 'd':
				checkArgAndDieOnError (*argv, 1, c0);
				debug = 1;
				break;

			case 'e':
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv)
					pExcludeFileList = *argv;
				else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'f':
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv)
					strcpy (szInSpec, *argv);
				else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'h':
				checkArgAndDieOnError (*argv, 1, c0);
				printUsageAndDie ();

			case 'i':
				checkArgAndDieOnError (*argv, 1, c0);
				bOrderInStringIsImportant = TRUE;
				break;

			case 'l':
				c2 = *((*argv)+1);
				if (c2 == 'l') {
					bLogToStderr = TRUE; //user specified /ll
				} else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else
					bLogFiles = TRUE; //user specified /l
				break;

			case 'm':
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv) {
					if ((iMaxMatches = abs (atoi (*argv))) < 0)
						iMaxMatches = 0;
				} else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'n':
				checkArgAndDieOnError (*argv, 1, c0);
				bShowLineNumbers = TRUE;
				break;

			case 'o':
				checkArgAndDieOnError (*argv, 1, c0);
				eMatchType = MATCH_OR;
				break;

			case 'q':
				checkArgAndDieOnError (*argv, 1, c0);
				bQuietMode = TRUE;
				iRemapFlags |= RMC_QUIETMODE;
				break;

			case 'r':
				checkArgAndDieOnError (*argv, 1, c0);
				bNoSearchOutput = TRUE;
				break;

			case 's':
				c2 = *((*argv)+1);
				if (c2 == 'l') { //user specified /sl
					//special handling for ClearCase Version Extended Pathnames
					bSkipLabels = TRUE;
//TODO: add check for malformed args that have trailing cruft, e.g., /slq
				} else if (isdigit (c2)) { //user specified /s<N>
					iAddlSubdirLevels = atol ((*argv)+1);
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS;
				} else if (c2 == 't') { //user specified /st
					//special handling for ClearCase Version Extended Pathnames
					bShowFileTimes = TRUE;
				} else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS; //user specified /s
				break;

			case 't':
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv) {
					if ((iTabSize = abs (atoi (*argv))) == 0)
						iTabSize = TABSIZE;
				} else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				iRemapFlags |= iTabSize;
				break;

			case 'v':
//TODO: handle /vc and set bExcludeStringCaseSensitive
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv) {
					pExcludeStringList = *argv;
				} else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'w':
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;
				if (**argv) {
					if ((iWindowLines = abs (atoi (*argv))) == 0)
						iWindowLines = 1;

				} else {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'x':
				checkArgAndDieOnError (*argv, 1, c0);
 				bExcludeNonSource = MFF_EXCLUDE_NONSOURCE | MFF_ISS_TEXT;
				break;

			default:
			{
				char buf[64];
				sprintf (buf, "unrecognized argument '%c%c'", c0, c1);
				printUsageAndDie (buf);
			}
		}
	}

#if 0
	if (debug) {
		waitForDebugger ();

		for (int ii = 0; ii < argcSave; ii++)
			fprintf (stderr, "argv[%d]=\"%s\"\n", ii, argvSave[ii]);
	}
#endif

	//check for options that don't work together
	if (bOrderInStringIsImportant && eMatchType == MATCH_OR) {
		fprintf (stderr, "\noptions mutually exclusive: /o and /i");
		return 1;
	}
	if (iMaxMatches > 0 && (bLogFiles || bLogToStderr)) {
		fprintf (stderr, "\noptions mutually exclusive: /m and /l");
		return 1;
	}

	if (bNoSearchOutput)
		bLogFiles = TRUE;

	//use next parameter as filespec, if not already specified
	if (szInSpec[0] == 0 && --argc > 0)
		strcpy (szInSpec, *argv++);
	swapSlashes (szInSpec);

	//count and save the strings, and lowercase them if case insensitive
	iNumSearchStrings = 0;
	while (--argc > 0 && iNumSearchStrings < MAXSTRINGS) {
		pStrings[iNumSearchStrings] = strndup (*argv++);
		if (!bCaseSensitive)
			strlwr (pStrings[iNumSearchStrings]);
		iNumSearchStrings++;
	}

	//error if not at least one search string
	if (iNumSearchStrings == 0)
		printUsageAndDie ("incorrect usage");

	//signal the user if the data has overflowed the array
	if (argc > 0 && iNumSearchStrings == MAXSTRINGS)
		report_data_overflow_error();

	if (!bShowFileTimes) {
		CONSOLE_SCREEN_BUFFER_INFO sbInfo;
		hStdout = GetStdHandle (STD_OUTPUT_HANDLE);
		if (GetConsoleScreenBufferInfo (hStdout, &sbInfo)) {
			bUseConsoleOutput = 1;
			wBaseColor = sbInfo.wAttributes;

			if ((wBaseColor & FOREGROUND_INTENSITY) == FOREGROUND_INTENSITY)
				wHighlightColor = wBaseColor & ~FOREGROUND_INTENSITY;
			else
				wHighlightColor = wBaseColor | FOREGROUND_INTENSITY;

			if (debug) {
				printf ("wBaseColor = 0x%02X\n", wBaseColor);
				printf ("wHighlightColor = 0x%02X\n", wHighlightColor);
			}

			//allow override of highlight color
			if (char *tmp = getenv ("SEARCH_HIGHLIGHT_COLOR")) { //assign!
				int color = 0;
				int fields = sscanf (tmp, "%x", &color);
				if (fields > 0 && color > 0)
					wHighlightColor = color;

				if (debug)
					printf ("--> got env var SEARCH_HIGHLIGHT_COLOR=0x%02X\n", color);
			}

			if (debug) {
				printf ("wBaseColor = 0x%02X\n", wBaseColor);
				printf ("wHighlightColor = 0x%02X\n", wHighlightColor);
			}


/* for reference, attributes for Command Prompt are stored in Registry here:
cmsSetEnv /print HKCU "Console\Command Prompt"

cmsSetenv /sys SEARCH_HIGHLIGHT_COLOR b
cmsSetenv /sys SEARCH_HIGHLIGHT_COLOR 3
*/

/* for reference:
use the "color" command to reset the Command Prompt colors
*/

/* for reference:
"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\PlatformSDK\Include\WinCon.h"
//
// Attributes flags:
//

#define FOREGROUND_BLUE      0x0001 // text color contains blue.
#define FOREGROUND_GREEN     0x0002 // text color contains green.
#define FOREGROUND_RED       0x0004 // text color contains red.
#define FOREGROUND_INTENSITY 0x0008 // text color is intensified.
#define BACKGROUND_BLUE      0x0010 // background color contains blue.
#define BACKGROUND_GREEN     0x0020 // background color contains green.
#define BACKGROUND_RED       0x0040 // background color contains red.
#define BACKGROUND_INTENSITY 0x0080 // background color is intensified.
*/

			if (debug) {
				char str[32];
				const int MAX_COLORS = 16;
				for (int background = 0; background < MAX_COLORS; background++) {
					for (int foreground = 0; foreground < MAX_COLORS; foreground++) {
						int color = foreground + background * MAX_COLORS;
						sprintf (str, " 0x%02X", color);
						writeBytes (str, 0, strlen (str), color);
					}
					writeBytes ("\n", 0, 1, wBaseColor); //reset color
				}
			}

#if 0
			if (debug) { //try to fix terminal that has had colors corrupted
/*
				// note: when this is working, here is the output:
				wBaseColor = 0x07
				wHighlightColor = 0x03
				// but after window colors have been corrupted:
				wBaseColor = 0x03
				wHighlightColor = 0x03
*/
				wBaseColor = 7;
				wHighlightColor = 3;

				writeBytes ("highlight\n", 0, 10, wHighlightColor);
				writeBytes ("base\n", 0, 5, wBaseColor);

				printf ("wBaseColor = 0x%02X\n", wBaseColor);
				printf ("wHighlightColor = 0x%02X\n", wHighlightColor);

/*
				//override command prompt colors (turn off FOREGROUND_INTENSITY)
				if ((wBaseColor & FOREGROUND_INTENSITY) == FOREGROUND_INTENSITY)
					wBaseColor &= ~FOREGROUND_INTENSITY;
				wHighlightColor = FOREGROUND_RED | FOREGROUND_INTENSITY;
				writeBytes ("0\n", 0, 2, wHighlightColor);
				writeBytes ("1\n", 0, 2, wBaseColor);

				fprintf (stderr, "wBaseColor=0x%04X\n", wBaseColor);
				fprintf (stderr, "wHighlightColor=0x%04X\n", wHighlightColor);
*/
			}
#endif
		}
	}

	p0 = myFileParse (szInSpec, szPathOnly, szFileOnly);
	if (!p0)
		return 1; //myFileParse() prints error message

	if (!bQuietMode) {
		//now tell the user what we are doing
		printf ("%s %s ", APP_NAME, VERS_STR);
		printf (" /window=%d /match=%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s %s",
			iWindowLines,
			(eMatchType == MATCH_AND ? "and" : "or"),
			(bMatchAtBeg ? " /matchAtBeg" : "" ),
			(bMatchAtEnd ? " /matchAtEnd" : "" ),
			(bLogFiles ? " /log" : ""),
			(bOrderInStringIsImportant ? " /iOrder" : ""),
			(bRecurseSubdirs ? " /subDirs" : ""),
			(bShowLineNumbers ? " /numbers" : ""),
			(bSkipLabels ? " /skipLabels" : ""),
			(bShowFileTimes ? " /showFileTimes" : ""),
			(bCaseSensitive ? " /case" : " /noCase"),
			(bExcludeNonSource ? " /sourceOnly" : ""),
			(pExcludeFileList ? " /excludeFiles=" : ""),
			(pExcludeFileList ? pExcludeFileList : ""),
			(pExcludeStringList ? " /excludeStrings=" : ""),
			(pExcludeStringList ? pExcludeStringList : ""),
			p0);
		for (i0 = 0; i0 < iNumSearchStrings; i0++) {
			if (strchr (pStrings[i0], 0x20)) //space
				printf (" \"%s\"", pStrings[i0]);
			else
				printf (" %s", pStrings[i0]);
		}
		printf ("\n");
	}

	//count number of files searched and number with matches
	iNumFilesWithMatches = iNumFilesSearched = 0;
	iFlags = bRecurseSubdirs | bExcludeNonSource;
	while ((p0 = myFindFile (szPathOnly, szFileOnly, &fdFiles, iFlags, iAddlSubdirLevels)) != NULL) {
		iNumMatchesInFile = 0;
		iRecordsSearched = 0;
		bHeader = FALSE;
		bUnicodeFile = FALSE;

		//aaa - this looks like a bug waiting to happen, see [bug] below
		iWindowCount = (-1);

		//skip the exclude file list, if requested
		if (pExcludeFileList && matchWildcardList (p0, pExcludeFileList))
			continue;

		if (bSkipLabels && !stringIsOnlyDigits (FILENAMEP (fdFiles)))
			continue;

		if (!usingStdin (p0)) {
			//open file in binary mode (to read binary files like PDF with images)
			if (myfreopen (p0, "rb", stdin) == NULL) {
				if (!bQuietMode) {
					fprintf (stderr, "fopen error (%d): ", errno);
					perror (p0);
				}
				continue;
			}

			//check for unicode file signature
			char buf[5];
			size_t iBytesRead = fread (buf, sizeof (char), 2, stdin);
			if (iBytesRead == 2 && ((buf[0] & 0xFF) == 0xFF && (buf[1] & 0xFF) == 0xFE))
				bUnicodeFile = TRUE;

			//seek back to beginning of file
			int status = fseek (stdin, 0, 0);
		}

		iNumFilesSearched++;

		if (bLogToStderr)
			fprintf (stderr, "(%d) %s\n", iNumFilesSearched, p0);

		//search it
#if defined (_MSC_VER)
		while (myfgets (line, BUFSZ - 2, stdin, bUnicodeFile)) {
#else
		while (fgets (line, BUFSZ - 2, stdin)) {
#endif
			iRecordsSearched++;

			char *pStr;
			int offsetFromOriginalBase = 0; //for use with bOrderInStringIsImportant

			remapChars (line, BUFSZ, iRemapFlags);

			//match strings in lower case if case insensitive
			if (bCaseSensitive)
				pStr = line;
			else {
				strcpy (lowline, line);
				strlwr (lowline);
				pStr = lowline;
			}

			//skip the exclude string list, if requested
			if (pExcludeStringList) {
				if (matchStringInList (pStr, pExcludeStringList, !bExcludeStringCaseSensitive))
					continue;
			}

			int matchIndex = 0;
			const int iMatchDataSize = 2048;
			MATCHDATA matchData[iMatchDataSize];

			//count number of pStrings[i]'s found in line
			for (i0 = 0, j0 = 0; i0 < iNumSearchStrings; i0++) {
				int bFound = 0;
				char *pMatch = NULL;

				if (bMatchAtBeg && i0 == 0) { //first string must be at beginning
					bFound = (strncmp (pStr, pStrings[i0], strlen (pStrings[i0])) == 0);
					if (bFound) {
						pMatch = pStr;
						matchData[matchIndex].ptr = 0;
						matchData[matchIndex].len = strlen (pStrings[i0]);
						myAssert (++matchIndex <= iMatchDataSize);
					}

				} else if (bMatchAtEnd && i0 == (iNumSearchStrings - 1)) { //last string must be at end
					int len0 = strlen (pStr);
					int len1 = strlen (pStrings[i0]);
					bFound = (strncmp (&pStr[len0 - len1], pStrings[i0], len1) == 0);
					if (bFound) {
						pMatch = pStr + len0 - len1;
						matchData[matchIndex].ptr = (len0 - len1) + offsetFromOriginalBase;
						matchData[matchIndex].len = strlen (pStrings[i0]);
						myAssert (++matchIndex <= iMatchDataSize);
					}

				} else { //match anywhere in string
					pMatch = strstr (pStr, pStrings[i0]);
					bFound = (pMatch != NULL);
					if (bFound) { //save first occurrence
						matchData[matchIndex].ptr = (pMatch - pStr) + offsetFromOriginalBase;
						matchData[matchIndex].len = strlen (pStrings[i0]);
						myAssert (++matchIndex <= iMatchDataSize);

						//find and save all other occurrences
						char *pTemp = pMatch;
						while ((pTemp = strstr (++pTemp, pStrings[i0])) != NULL) {
							matchData[matchIndex].ptr = (pTemp - pStr) + offsetFromOriginalBase;
							matchData[matchIndex].len = strlen (pStrings[i0]);
//							myAssert (++matchIndex <= iMatchDataSize);
							if (++matchIndex >= iMatchDataSize)
								break; //silent error
						}
					}
				}

				if (bFound) {
					j0++;
//					if (eMatchType == MATCH_OR && !bUseConsoleOutput)
//						break; //one match is all we need for MATCH_OR
				}

				if (bOrderInStringIsImportant && eMatchType == MATCH_AND) {
					//if order is important, start searching
					//for next string after last matched string
					if (pMatch != NULL) {
						pMatch += strlen (pStrings[i0]); //step over matched string
						offsetFromOriginalBase += (pMatch - pStr);
						pStr = pMatch;
					}
				}
			}

			//does this line meet the requirements?
			if ((eMatchType == MATCH_AND && j0 == iNumSearchStrings)
			 || (eMatchType == MATCH_OR  && j0 >= 1)) {
				iWindowCount = iWindowLines;
				iNumMatchesInFile++;
			}

			//print the line if we are within the window
//aaa - [bug] this test is not very clean
			if (--iWindowCount >= 0) {
				//only print the file name header once
				if (!bNoSearchOutput && !bHeader && !bQuietMode && !usingStdin (p0)) {
					if (bShowFileTimes) {
						char buf[32];
						time_t fileTime_t = fileTime (p0);
						struct tm* tm = localtime (&fileTime_t);
						strftime (buf, sizeof buf, "%Y/%m/%d %H:%M:%S ", tm);
						myPrintf ("%s", buf); //prepend file creation time for sorting
					}

					myPrintf ("%s\n", p0);
					bHeader = TRUE;
				}

				//print the line number, if requested
				if (bShowLineNumbers && !bQuietMode)
					myPrintf ("%d: ", iRecordsSearched);

//				if (debug) {
				if (0) {
					for (int ii = 0; ii < matchIndex; ii++)
						fprintf (stderr, "matchData[%d].ptr=%d, matchData[%d].len=%d\n",
										  ii, matchData[ii].ptr, ii, matchData[ii].len);
				}

				//print matching line
				if (!bNoSearchOutput) {
					if (bUseConsoleOutput)
						printLine (line, matchData, matchIndex);
					else
						myPrintf ("%s\n", line);
				}
			}

			//stop after printing maximun number of matches requested
			if (iMaxMatches > 0 && iMaxMatches <= iNumMatchesInFile)
				break;
		}

		//print log if match was found or switch was specified
		if (iNumMatchesInFile > 0 || bLogFiles) {
			if (!bQuietMode) {
				myPrintf ("%s: %d record%s, %d match%s%s\n", p0,
					iRecordsSearched, (iRecordsSearched == 1 ? "" : "s"),
					iNumMatchesInFile, (iNumMatchesInFile == 1 ? "" : "es"),
					(bUnicodeFile ? " (Unicode)" : ""));
			}

			if (iNumMatchesInFile > 0)
				iNumFilesWithMatches++;
		}

//TODO?	fclose (stdin);

		if (iNumMatchesInFile > 0 && !bNoSearchOutput && !bQuietMode)
			myPrintf ("*********************************************\n");

//		if (bLogToStderr)
			fflush (stdout);

		if (bShowFileTimes && pTimeOrderedOutput[iTimeOrderedIndex]) {
			myAssert (++iTimeOrderedIndex <= iTimeOrderedOutputSize); //skip to next string
		}

		//break out of myFindFile loop since there is only one stdin
		if (usingStdin (p0))
			break;
	}

	if (iNumFilesSearched == 0)
		myPrintf ("No files found\n");
	else if (!bQuietMode)
		myPrintf ("%d of %d files searched had matches\n", iNumFilesWithMatches, iNumFilesSearched);

	if (bShowFileTimes) {
		qsort (pTimeOrderedOutput, iTimeOrderedIndex, sizeof (char*), compare2);

		for (int ii = 0; ii < iTimeOrderedIndex; ii++)
			printf ("%s", pTimeOrderedOutput[ii]);
	}

	return 0;
}
