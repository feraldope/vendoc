// mov.c - creates and optionally executes a series of move commands given wildcard filenames
// examples:
//		mov [/noGap] [/f] oldname*.txt newname*.txt
//		mov /rep oldname*.txt old
//		mov /rep oldname*.txt old new
//		mov /renum{1|2}[=<width>][/start=<num>] name*.txt stub
//		mov /pad[=<width>] name*.txt
//
// can also specify "/alpha" to use alpha-numeric sort
//
// also, optionally converts spaces or underscores in names to dashes

#define APP_NAME "mov"
#define VERS_STR "(V6.03)"

#include <windows.h>
#if defined (_MSC_VER)
#include <conio.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <iostream>
#include "fileio.h"

using namespace std;

#define MAX_FILES 30000
char *szUndoCmds[MAX_FILES];

const int IgnoreCase = 0;
const int UseCase = 1;

int numConflicts = 0;
int startIndex = 1;
int increment = 1; //set to -1 for reverse (decrement)
bool useAlphanumSort = 0;
bool renumberWithGaps = 1;
bool forceOverwrite = 0;
bool queryMode = 0;
bool reNumber1 = 0;
bool reNumber2 = 0;
bool pad = 0;
bool replaceString = 0;
bool replaceSpaces = 0;
bool replaceUnderscores = 0;
bool skipAlpha = 0; //set to 1 to only operate on file names that start with a digit
bool saveHistory = 0;
bool saveUndo = 0;
//char *prRoot = 0;

typedef vector<string, allocator<string> > StringVector; //vector has no default sorting

////////////////////////////////////////////////////////////////////////////
struct lessAlphanum : binary_function<string, string, bool>
{
	bool operator() (const string& s1, const string& s2) const
	{
		return alphanumComp (s1.c_str (), s2.c_str ()) < 0; //alpha-numeric sort
	}
};

////////////////////////////////////////////////////////////////////////////
struct lessNoCase : binary_function<string, string, bool>
{
	struct compareNoCase : public binary_function<unsigned char, unsigned char, bool>
	{
		bool operator() (const unsigned char& c1, const unsigned char& c2) const
		{
			return tolower (c1) < tolower (c2);
		}
	};

	bool operator() (const string& s1, const string& s2) const
	{
		return lexicographical_compare (s1.begin (), s1.end (), s2.begin (), s2.end (), compareNoCase ());
	}
};

///////////////////////////////////////////////////////////////////////////////
bool fileExistsWild (string inSpec)
{
	WIN32_FIND_DATA fdFiles;

	HANDLE hFound = FindFirstFileEx (inSpec.c_str (),
									 FindExInfoStandard, //FindExInfoBasic not supported in VC2005
									 &fdFiles,
									 FindExSearchNameMatch,
									 NULL,
									 0); //FIND_FIRST_EX_LARGE_FETCH); //not supported in VC2005

	return hFound != INVALID_HANDLE_VALUE;
}

///////////////////////////////////////////////////////////////////////////////
StringVector getFileList (string inSpec)
{
	StringVector fileList;

	WIN32_FIND_DATA fdFiles;

	HANDLE hFound = FindFirstFileEx (inSpec.c_str (),
									 FindExInfoStandard, //FindExInfoBasic not supported in VC2005
									 &fdFiles,
									 FindExSearchNameMatch,
									 NULL,
									 0); //FIND_FIRST_EX_LARGE_FETCH); //not supported in VC2005

	BOOL bFound = (hFound != INVALID_HANDLE_VALUE);
	while (bFound) {

		//FindFirstFileEx matches on 8.3 names, so we need to confirm file really matches our request
		if (matchtest (fdFiles.cFileName, inSpec.c_str ())) {

			if (!skipAlpha || isdigit (fdFiles.cFileName[0])) {
				string file = getPath (fdFiles.cFileName);
				fileList.push_back (file);
			}
		}

		bFound = FindNextFile (hFound, &fdFiles);
	}

	FindClose (hFound);

	if (useAlphanumSort)
		sort (fileList.begin (), fileList.end (), lessAlphanum ());
	else
		sort (fileList.begin (), fileList.end (), lessNoCase ());

#if 0 //debug
	int numFiles = fileList.size ();
	cout << "numFiles: " << numFiles << endl;
	StringVector::const_iterator jj;
	for (jj = fileList.begin (); jj != fileList.end (); ++jj) {
		string file = *jj;
		cout << file << endl;
	}
#endif

	return fileList;
}

/////////////////////////////////////////////////////////////////////////////
char *getCurrentDirectory ()
{
	static char currentDir[MY_MAX_PATH + 1] = "";
	GetCurrentDirectory (MY_MAX_PATH, currentDir);

	return currentDir;
}

/////////////////////////////////////////////////////////////////////////////
bool setCurrentDirectory (const char *dir)
{
	if (SetCurrentDirectory (dir) == FALSE) {
		fprintf (stderr, "setDirectory: SetCurrentDirectory (%s) failed: %s", dir, formatLastError (GetLastError ()));
		return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
char *setDirectory (const char *inSpec)
{
	if (fileExistsWild (inSpec)) {
		return getCurrentDirectory ();
	}

	char *prRoot = getenv ("PR_ROOT");
	if (prRoot == NULL) {
		return NULL;
	}

	if (SetCurrentDirectory (prRoot) == FALSE) {
		fprintf (stderr, "setDirectory: SetCurrentDirectory (%s) failed: %s", prRoot, formatLastError (GetLastError ()));
		return NULL;
	}

	if (fileExistsWild (inSpec)) {
		return getCurrentDirectory ();
	}

	const int albumSubfolderLength = 2;
	char subdir[albumSubfolderLength + 1];
	strncpy (subdir, inSpec, albumSubfolderLength);
	subdir[albumSubfolderLength] = 0;

	char dir[256];
	strcpy (dir, prRoot);
	strcat (dir, "/jroot/");
	strcat (dir, subdir);

	if (SetCurrentDirectory (dir) == FALSE) {
		fprintf (stderr, "setDirectory: SetCurrentDirectory (%s) failed: %s", dir, formatLastError (GetLastError ()));
		return NULL;
	}

	if (fileExistsWild (inSpec)) {
		return getCurrentDirectory ();
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
bool filesSpreadAcrossMultipleFolders (const char *inSpec)
{
	char *prRoot = getenv ("PR_ROOT");
	if (prRoot == NULL) {
		return FALSE;
	}

	int numDirCount = 0;


//	if (fileExistsWild (inSpec)) {
//		numDirCount++
//	}

	if (SetCurrentDirectory (prRoot) == FALSE) {
		fprintf (stderr, "setDirectory: SetCurrentDirectory (%s) failed: %s", prRoot, formatLastError (GetLastError ()));
		return FALSE;
	}

	if (fileExistsWild (inSpec)) {
		numDirCount++;
	}

	const int albumSubfolderLength = 2;
	char subdir[albumSubfolderLength + 1];
	strncpy (subdir, inSpec, albumSubfolderLength);
	subdir[albumSubfolderLength] = 0;

	char dir[256];
	strcpy (dir, prRoot);
	strcat (dir, "/jroot/");
	strcat (dir, subdir);

	if (SetCurrentDirectory (dir) == FALSE) {
		fprintf (stderr, "setDirectory: SetCurrentDirectory (%s) failed: %s", dir, formatLastError (GetLastError ()));
		return FALSE;
	}

	if (fileExistsWild (inSpec)) {
		numDirCount++;
	}

	return numDirCount > 1;
}

///////////////////////////////////////////////////////////////////////////////
bool checkArg (char *str)
{
	char *p0 = strchr (str, '*'); //search for a '*'
	char *p1 = 0;
	if (p0 && *(p0 + 1)) //if we found one and there is at least one char after it,
		p1 = strchr (++p0, '*'); //search for another '*'

	if (!p0 || p1) { //found zero or more than one '*'
		fprintf (stderr, APP_NAME " error: each arg must have exactly one '*'\n");
		return 0;
	}

	//look for and error on these invalid chars
	p0 = strchr (str, '\\');
	if (!p0)
		p0 = strchr (str, '/');
	if (!p0)
		p0 = strchr (str, ':');
	if (!p0)
		p0 = strchr (str, '?');
	if (p0) {
		fprintf (stderr, APP_NAME " error: no path components are allowed, file names only\n");
		return 0;
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
char *changeChar (char *str, char c1, char c2)
{
	int length = strlen (str);
	for (int ii = 0; ii < length; ii++)
		if (str[ii] == c1)
			str[ii] = c2;

	return str;
}

///////////////////////////////////////////////////////////////////////////////
char *nextNumber1 (char *orig, int width)
{
	//find last block of digits
	int firstDigitToReplace = -1;
	int lastDigitToReplace = -1;
	int inNumber = false;

	for (int ii = strlen (orig) - 1; ii >= 0; --ii) {
		if (isdigit (orig[ii])) {
			if (!inNumber) {
				if (lastDigitToReplace < 0)
					lastDigitToReplace = ii;
				inNumber = true;
			}
		} else {
			if (inNumber) {
				if (firstDigitToReplace < 0)
					firstDigitToReplace = ii + 1;
				else
					break;
			}
		}
	}

	if (lastDigitToReplace < 0) {
		fprintf (stderr, "Warning: no digits found in \"%s\"\n", orig);
		return (orig);
	}

	//if we got to here and firstDigitToReplace is not set, the string starts with a digit
	if (firstDigitToReplace < 0)
		firstDigitToReplace = 0;

	//get next number
	char number[12];
	sprintf (number, "%0*d", width, startIndex);

	startIndex += increment;
	if (renumberWithGaps)
		if ((startIndex % 5) == 0) //leave gaps
			startIndex += increment;

	char *beg1 = strndup (orig, 0, firstDigitToReplace);
	char *end1 = strndup (orig, lastDigitToReplace + 1, strlen (orig) - lastDigitToReplace - 1);

	char buf[64];
	strcpy (buf, beg1);
	strcat (buf, number);
	strcat (buf, end1);

	return (strndup (buf)); //memory leak
}

///////////////////////////////////////////////////////////////////////////////
char *nextNumber2 (int width)
{
	char buf[8];
	sprintf (buf, "%0*d", width, startIndex);

	startIndex += increment;
	if (renumberWithGaps)
		if ((startIndex % 5) == 0) //leave gaps
			startIndex += increment;

	return (strndup (buf)); //memory leak
}

///////////////////////////////////////////////////////////////////////////////
char *padNumber (char *orig, int width)
{
	int length = strlen (orig);
	int padlen = max (width - length, 0);

	for (int ii = 0; ii < length; ii++) {
		if (!isdigit (orig[ii])) {
			fprintf (stderr, "Warning: substring to pad \"%s\" is not a number\n", orig);
			return (orig);
		}
	}

	char buf[32];
	memset (buf, '0', sizeof buf);
	strcpy (&buf[padlen], orig);

	return (strndup (buf)); //memory leak
}

///////////////////////////////////////////////////////////////////////////////
boolean namesAreDifferent (char *from, char *to, int handleCase)
{
	if (handleCase == IgnoreCase)
		return strcasecmp (to, from);
	else //handleCase == UseCase
		return strcmp (to, from);
}

///////////////////////////////////////////////////////////////////////////////
//return true if names (after removing numbers and dashes/underscores) are same
boolean isEffectiveRenumbering (char *fromOrig, char *toOrig, int handleCase)
{
	char *from = strndup (fromOrig); //memory leak
	char *to = strndup (toOrig);

	//truncate string at first digit, dash or underscore
	for (size_t ii = 0; ii < strlen (from); ii++) {
		if (isdigit (from[ii]) || from[ii] == '-' || from[ii] == '_') {
			from[ii] = 0;
			break;
		}
	}

	//truncate string at first digit, dash or underscore
	for (size_t ii = 0; ii < strlen (to); ii++) {
		if (isdigit (to[ii]) || to[ii] == '-' || to[ii] == '_') {
			to[ii] = 0;
			break;
		}
	}

	if (handleCase == IgnoreCase)
		return (strcasecmp (to, from) == 0);
	else //handleCase == UseCase
		return (strcmp (to, from) == 0);
}

///////////////////////////////////////////////////////////////////////////////
int generateCommands (char *to, char *from, int numCommands, char **szCommands)
{
	int numCommandsSave = numCommands;

	if (replaceSpaces)
		changeChar (to, ' ', '-'); //change spaces to dashes

	if (replaceUnderscores)
		changeChar (to, '_', '-'); //change underscores to dashes

//TODO - should this be a case-sensitive compare?
	if (fileExists (to)) {
		numConflicts++;
		fprintf (stderr, "Warning: destination file '%s' already exists (specify /f to force override)\n", to);
	}

	if (namesAreDifferent (from, to, UseCase)) {
		char szCommand[MY_MAX_PATH];
		char szUndoCmd[MY_MAX_PATH];

		if (namesAreDifferent (from, to, IgnoreCase)) {
			sprintf (szCommand, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (from), quoteFile (to));
			sprintf (szUndoCmd, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (to), quoteFile (from));
			myAssert (numCommands < MAX_FILES);
			szCommands[numCommands] = strndup (szCommand);
			szUndoCmds[numCommands] = strndup (szUndoCmd);
			numCommands++;

		} else {
			char tmp[MAX_PATH];
			strcpy (tmp, to);
			strcat (tmp, ".tmp");

			sprintf (szCommand, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (from), quoteFile (tmp));
			sprintf (szUndoCmd, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (to), quoteFile (tmp));
			myAssert (numCommands < MAX_FILES);
			szCommands[numCommands] = strndup (szCommand);
			szUndoCmds[numCommands] = strndup (szUndoCmd);
			numCommands++;

			sprintf (szCommand, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (tmp), quoteFile (to));
			sprintf (szUndoCmd, "move %s %s %s", (forceOverwrite ? "/Y" : "/-Y"), quoteFile (tmp), quoteFile (from));
			myAssert (numCommands < MAX_FILES);
			szCommands[numCommands] = strndup (szCommand);
			szUndoCmds[numCommands] = strndup (szUndoCmd);
			numCommands++;
		}

	} else {
		static int displayedWarning = 0;

		if (!displayedWarning) {
			displayedWarning = 1;
			fprintf (stderr, "Warning: 'from' and 'to' strings are the same\n");
		}
	}

	return numCommands - numCommandsSave;
}

///////////////////////////////////////////////////////////////////////////////
int doReNumber (char *str1, char *str2, int style, int width, char **szCommands)
{
	if (!checkArg (str1))
		return 0; //checkArg prints error

	char *aster1 = strchr (str1, '*');

	// str1
	int str1Len = strlen (str1);
	int beg1Len = aster1 - str1;
	int end1Len = str1Len - beg1Len - 1;
	char *beg1 = strndup (str1, 0, beg1Len);
	char *end1 = strndup (aster1, 1, end1Len);

	StringVector fileList = getFileList (str1);

	char from[MAX_PATH];
	int numCommands = 0;
	StringVector::const_iterator jj;
	for (jj = fileList.begin (); jj != fileList.end (); ++jj) {
		string file = *jj;

		// wild
		strcpy (from, file.c_str ());
		int fromLen = strlen (from);
		int midLen = fromLen - beg1Len - end1Len;
		myAssert (midLen >= 0);
		char *mid = strndup (from, beg1Len, midLen);

		char to[MAX_PATH];
		strcpy (to, beg1);
		if (str2)
			strcat (to, str2);
		if (style == 1)
			strcat (to, nextNumber1 (mid, width));
		else
			strcat (to, nextNumber2 (width));
		strcat (to, end1);

		numCommands += generateCommands (to, from, numCommands, szCommands);
	}

	return numCommands;
}

///////////////////////////////////////////////////////////////////////////////
int doPad (char *str1, size_t width, char **szCommands)
{
	if (!checkArg (str1))
		return 0; //checkArg prints error

	char *aster1 = strchr (str1, '*');

	// str1
	int str1Len = strlen (str1);
	int beg1Len = aster1 - str1;
	int end1Len = str1Len - beg1Len - 1;
	char *beg1 = strndup (str1, 0, beg1Len);
	char *end1 = strndup (aster1, 1, end1Len);

	StringVector fileList = getFileList (str1);

	char from[MAX_PATH];
	int numCommands = 0;
	StringVector::const_iterator jj;
	for (jj = fileList.begin (); jj != fileList.end (); ++jj) {
		string file = *jj;

		// wild
		strcpy (from, file.c_str ());
		int fromLen = strlen (from);
		int midLen = fromLen - beg1Len - end1Len;
		myAssert (midLen >= 0);
		char *mid = strndup (from, beg1Len, midLen);

		char *pad = padNumber (mid, width);

		if (strcmp (mid, pad)) {
			char to[MAX_PATH];
			strcpy (to, beg1);
			strcat (to, pad);
			strcat (to, end1);

			numCommands += generateCommands (to, from, numCommands, szCommands);
		}
	}

	return numCommands;
}

///////////////////////////////////////////////////////////////////////////////
int doMoveFiles (char *str1, char *str2, char **szCommands)
{
	if (!checkArg (str1))
		return 0; //checkArg prints error
	if (!checkArg (str2))
		return 0; //checkArg prints error

	char *aster1 = strchr (str1, '*');
	char *aster2 = strchr (str2, '*');

	// str1
	int str1Len = strlen (str1);
	int beg1Len = aster1 - str1;
	int end1Len = str1Len - beg1Len - 1;
	char *beg1 = strndup (str1, 0, beg1Len);
	char *end1 = strndup (aster1, 1, end1Len);

	// str2
	int str2len = strlen (str2);
	int beg2Len = aster2 - str2;
	int end2Len = str2len - beg2Len - 1;
	char *beg2 = strndup (str2, 0, beg2Len);
	char *end2 = strndup (aster2, 1, end2Len);

	StringVector fileList = getFileList (str1);

	char from[MAX_PATH];
	int numCommands = 0;
	StringVector::const_iterator jj;
	for (jj = fileList.begin (); jj != fileList.end (); ++jj) {
		string file = *jj;

		// wild
		strcpy (from, file.c_str ());
		int fromLen = strlen (from);
		int midLen = fromLen - beg1Len - end1Len;
		myAssert (midLen >= 0);
		char *mid = strndup (from, beg1Len, midLen);

		char to[MAX_PATH];
		strcpy (to, beg2);
		strcat (to, mid);
		strcat (to, end2);

		if (isEffectiveRenumbering (from, to, UseCase) || *str1 == '*')
			saveHistory = 0;

		numCommands += generateCommands (to, from, numCommands, szCommands);
	}

	return numCommands;
}

///////////////////////////////////////////////////////////////////////////////
int doReplaceString (char *str1, char *str2, char *str3, char **szCommands)
{
	if (!checkArg (str1))
		return 0; //checkArg prints error

	StringVector fileList = getFileList (str1);

	char from[MAX_PATH];
	int numCommands = 0;
	StringVector::const_iterator jj;
	for (jj = fileList.begin (); jj != fileList.end (); ++jj) {
		string file = *jj;

		// wild
		strcpy (from, file.c_str ());

		char *p1 = strstri (from, str2);
		if (p1) {
			char to[MAX_PATH];

			int len1 = p1 - from;
			int len2 = strlen (str2);

			strncpy (to, from, len1);
			to[len1] = 0;

			if (str3)
				strcat (to, str3);

			strcat (to, p1 + len2);

			if (isEffectiveRenumbering (from, to, UseCase) || *str1 == '*')
				saveHistory = 0;

			numCommands += generateCommands (to, from, numCommands, szCommands);
		}
	}

	return numCommands;
}

///////////////////////////////////////////////////////////////////////////////
//write the commands necessary to undo these moves to a timestamped file: %PR_ROOT%\tmp\mov.<date>.<time>.bat
int writeUndoCommands (char **szCommands, int numCommands, char *directory)
{
	if (!saveUndo)
		return 0;

	//write undo commands to timestamped file
	char dateBuf[_MAX_PATH] = "";
	time_t timeNow = time ((time_t*) NULL);
	struct tm* tm = localtime (&timeNow);
	strftime (dateBuf, _MAX_PATH, "%Y%m%d.%H%M%S", tm);

	char *prRoot = getenv ("PR_ROOT");

	char logName[MAX_PATH];
	strcpy (logName, prRoot);
	strcat (logName, "/tmp/mov.");
	strcat (logName, dateBuf);
	strcat (logName, ".bat");

	FILE *logFile = fopen (logName, "a+");

	if (logFile) {
		fprintf (logFile, "REM %d undo commands\n", numCommands);
		fprintf (logFile, "setlocal\n");
		fprintf (logFile, "cd /d %s\n", directory);

		//note: written in reverse order, in case it matters
		for (int ii = numCommands - 1; ii >= 0; --ii)
			fprintf (logFile, "%s\n", szUndoCmds[ii]);
	}

	return numCommands;
}

///////////////////////////////////////////////////////////////////////////////
int writeHistory (int argc, char *argv[])
{
	if (!saveHistory)
		return 0;

	printf ("[adding to history]\n");

	char *prRoot = getenv ("PR_ROOT");

	char logName[MAX_PATH];
	strcpy (logName, prRoot);
	strcat (logName, "/");
	strcat (logName, "mov.history.txt");

	FILE *logFile = fopen (logName, "a+");

	if (logFile) {
		//note we hardcode "mov" instead of printing argv[0] (i.e., program name) in case they are different
		fprintf (logFile, "mov ");
		for (int ii = 1; ii < argc; ii++) {
			if (strchr (argv[ii], ' '))
				fprintf (logFile, "\"%s\" ", argv[ii]);
			else
				fprintf (logFile, "%s ", argv[ii]);
		}
		fprintf (logFile, "\n");
		fclose (logFile);
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
	int padWidth = 2;
	int renumWidth = 2;

	char **saveArgv = argv;
	int saveArgc = argc;
	boolean doMoves = false;
	char *prRoot = getenv ("PR_ROOT");
	if (prRoot != NULL) {
		saveHistory = 1;
		saveUndo = 1;
		doMoves = true;
	}

//TODO - this doesn't allow args to be in a different order, and some combinations won't work

	if (argc > 1 && (!strcmp (argv[1], "/noGap") || !strcmp (argv[1], "/nogap"))) {
		--argc;
		argv++;
		renumberWithGaps = 0;
	}

	if (argc > 1 && (!strcmp (argv[1], "/alpha") || !strcmp (argv[1], "/a"))) {
		--argc;
		argv++;
		useAlphanumSort = 1;
	}

	if (argc > 1 && (!strcmp (argv[1], "/skipAlpha") || !strcmp (argv[1], "/sa"))) {
		--argc;
		argv++;
		skipAlpha = 1;
	}

	if (argc > 1 && (!strcmp (argv[1], "/noHistory") || !strcmp (argv[1], "/noh"))) {
		--argc;
		argv++;
		saveHistory = 0;
	}

	if (argc > 1 && !strcmp (argv[1], "/rep")) {
		--argc;
		argv++;
		replaceString = 1;
	}

	if (argc > 1 && !strncmp (argv[1], "/renum1", 7)) { //handle /renum1=<num>
//TODO - handle malformed arg like /renum1=
		if (strlen (argv[1]) > 5 && argv[1][7] == '=') {
			int num = atoi (&argv[1][8]);
			if (num >= 1)
				renumWidth = num;
		}

		--argc;
		argv++;
		reNumber1 = 1;
	}

	if (argc > 1 && !strncmp (argv[1], "/renum2", 7)) { //handle /renum2=<num>
//TODO - handle malformed arg like /renum2=
		if (strlen (argv[1]) > 5 && argv[1][7] == '=') {
			int num = atoi (&argv[1][8]);
			if (num >= 1)
				renumWidth = num;
		}

		--argc;
		argv++;
		reNumber2 = 1;
	}

	if (argc > 1 && !strncmp (argv[1], "/pad", 4)) { //handle /pad=<num>
//TODO - handle malformed arg like /pad=
		if (strlen (argv[1]) > 5 && argv[1][4] == '=') {
			int num = atoi (&argv[1][5]);
			if (num >= 1)
				padWidth = num;
		}

		--argc;
		argv++;
		pad = 1;
	}

	if (argc > 1 && !strncmp (argv[1], "/start", 6)) { //handle /start=<num>
//TODO - handle malformed arg like /start=
		if (strlen (argv[1]) > 7 && argv[1][6] == '=') {
			int num = atoi (&argv[1][7]);
			if (num >= 1)
				startIndex = num;
		}

		--argc;
		argv++;
		pad = 1;
	}

	if (argc > 1 && !strcmp (argv[1], "/rev")) {
		--argc;
		argv++;
		increment = -1;

//TODO - this works because startIndex was already set because the current processing requires the args to be in a specific order
		if (startIndex = 1)
			startIndex = 999;
	}

	if (argc > 1 && !strcmp (argv[1], "/sp")) {
		--argc;
		argv++;
		replaceSpaces = 1;
	}

	if (argc > 1 && !strcmp (argv[1], "/us")) {
		--argc;
		argv++;
		replaceUnderscores = 1;
	}

	if (argc > 1 && !strcmp (argv[1], "/f")) {
		--argc;
		argv++;
		forceOverwrite = 1;
	}
	if (argc > 1 && !strcmp (argv[1], "/q")) {
		--argc;
		argv++;
		queryMode = 1;
	}
	//TODO - make sure we have the correct number of args left

//TODO - fix usage output
	if (reNumber1 || reNumber2) {
		if (argc != 2 && argc != 3 && argc != 4) {
			fprintf (stderr, "Usage: " APP_NAME " [/renum[1|2][=<width>] [/start=<num>]] [/sp] [/us] [/f] <pattern> [<stub>]\n"
					" where pattern can contain wildcards\n");
			return 1;
		}

	} else if (pad) {
		if (argc != 2) {
			fprintf (stderr, "Usage: " APP_NAME " [/pad[=<width>]] [/sp] [/us] [/f] <pattern>\n"
					" where pattern can contain wildcards\n");
			return 1;
		}

	} else if (replaceString) {
		if (argc != 3 && argc != 4) {
			fprintf (stderr, "Usage: " APP_NAME " [/rep] [/sp] [/us] [/f] <pattern> <string1> [<string2>]\n"
					" where pattern can contain wildcards\n");
			return 1;
		}

	} else {
		if (argc != 3) {
			fprintf (stderr, "Usage: " APP_NAME " [/rep] [/sp] [/us] [/f] <pattern1> <pattern2>\n"
					" where each pattern must have exactly one '*'\n");
			return 1;
		}
	}

	char *str1 = *++argv;
	char *str2 = (argc >= 3 ? *++argv : NULL);
	char *str3 = (argc >= 4 ? *++argv : NULL);

//	char *currentDirectory = strdup (getCurrentDirectory ());

	char *directory = setDirectory (str1);

	char *szCommands[MAX_FILES];

	if (filesSpreadAcrossMultipleFolders (str1)) {
		fprintf (stderr, "Error: files exist across multiple folders\n");
		return 1;
	}

	if (directory != null) {
		setCurrentDirectory (directory); //hack reset current directory
	}

	int numCommands = 0;
	if (reNumber1 || reNumber2) {
		int style = (reNumber1 ? 1 : 2);
		numCommands = doReNumber (str1, str2, style, renumWidth, szCommands);
		saveHistory = 0;

	} else if (replaceString) {
		numCommands = doReplaceString (str1, str2, str3, szCommands);
		//saveHistory potentially cleared in doReplaceString ()

	} else if (pad) {
		numCommands = doPad (str1, padWidth, szCommands);
		saveHistory = 0;

	} else {
		numCommands = doMoveFiles (str1, str2, szCommands);
		//saveHistory potentially cleared in doMoveFiles ()
	}

	if (numCommands == 0) {
		printf ("No matching files found\n");

	} else {
		printf ("Move commands (%d):\n", numCommands);

		for (int ii = 0; ii < numCommands; ii++)
			printf ("%s\n", szCommands[ii]);

		if (numConflicts != 0 && !forceOverwrite) {
			char msg1[] = "Error: there is %d conflict\n";
			char msg2[] = "Error: there are %d conflicts\n";
			printf (numConflicts == 1 ? msg1 : msg2, numConflicts);

		} else {
			boolean doMoves = !queryMode;

			if (!doMoves) {
				printf ("--> Perform %d move commands? (Y/N) [N] ", numCommands);
#if defined (_MSC_VER)
				doMoves = ((_getch () | 040) == 'y');
#endif
			}

			if (doMoves) {
				for (int ii = 0; ii < numCommands; ii++)
					system (szCommands[ii]);
//TODO - error checking here?

				writeUndoCommands (szUndoCmds, numCommands, directory);
				writeHistory (saveArgc, saveArgv);
			}
		}
	}

	return 0;
}
