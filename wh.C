// wh - searches the path environment variable (and pwd) for the given file.
//      Useful for discovering which directory a file will be executed from.
//
// This also handles INCLUDE paths with an embedded (relative) path.
// For example, this #include line from a .C file:
//
//	#include <sys/types.h>
//
// can be located with this command
//
//	wh /b sys/types.h INCLUDE

#define APP_NAME "wh"
#define VERS_STR "(V3.01)"

#pragma warning (disable:4786)

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <set>
#include <assert.h>
#include "fileio.h"

using namespace std;

//set maintains sorted and uniq'ed list
typedef set<string, less<string>, allocator<string> > StringSet;

#define BUFSZ		(20 * 1024)
#define MAXPATHS	128

char pathVar[BUFSZ], pathExtVar[BUFSZ];
char fileSpec[BUFSZ], tmp[BUFSZ], line[BUFSZ];
char pathArray[MAXPATHS][BUFSZ], extArray[MAXPATHS][BUFSZ];
char *p0;
int debug = 0;


///////////////////////////////////////////////////////////////////////////////
void printUsageAndDie (char *message = 0)
{
	if (message)
		fprintf (stderr, "\n%s: %s; use /h for help\n", APP_NAME, message);
	else {
		fprintf (stderr, "\nUsage: %s [/d] [/t] [/b] [/x] [/sp] <filename> [<environment-variable>]\n", APP_NAME);

		//note following usage needs quotes because PATH might include spaces
		fprintf (stderr, "\n -- for example: %s /sp \"%%PATH%%\"", APP_NAME);
	}

	exit (1);
}

////////////////////////////////////////////////////////////////////////////
char *appendSemi (char *path)
{
	//append semicolon to end, if not already there (buffer must be large enough)
	if (path[strlen (path) - 1] != ';')
		strcat (path, ";");

	return path;
}

////////////////////////////////////////////////////////////////////////////
char *removeTrailingBackslash (char *path)
{
	int len = strlen (path);
	if (len > 0 && path[len - 1] == '\\')
		path[len - 1] = 0;

	return path;
}

//////////////////////////////////////////////////////////////////////////////
int splitPath (char *var, char array[][BUFSZ], int useShortPathNames)
{
	int ii = 0;
	int kk = 0;

	//loop through the path string
	while (kk < MAXPATHS) {
		int jj = 0;
		//path directories are separated by ';'
		while (var[ii] != ';' && var[ii] != 0)
			array[kk][jj++] = var[ii++];

		array[kk][jj] = 0;
		removeTrailingBackslash (array[kk]); //not always necessary, but shouldn't hurt either

		//upcase drive letter
		if (islower (array[kk][0]) && array[kk][1] == ':')
			array[kk][0] = toupper (array[kk][0]);

#if 0 //redundant with processing in main ??
		//if this path directory is the same as any of the others, don't save it
		int found = 0;
		for (int pp = 0; pp < kk; pp++) {
			if (stricmp (array[kk], array[pp]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found)
			kk++;
#else
		kk++;
#endif

		//increment pointer to *pathVar (and skip multiple ';' in a row)
		while (var[ii] == ';')
			ii++;
		if (var[ii] == 0)
			break;
	}

	if (kk == MAXPATHS)
		printf ("overload in splitPath ()\n" );

	if (useShortPathNames) {
		char shortPathName[MAX_PATH];
		for (ii = 0; ii < kk; ii++) {
			//it looks like GetShortPathName() uses the file system to shorten the name, and MVFS doesn't
			if (GetShortPathName (array[ii], shortPathName, MAX_PATH))
				strcpy (array[ii], shortPathName);
//			else
//				printf ("GetShortPathName (\"%s\") failed: %s", array[ii], formatLastError (GetLastError ()));
		}
	}

	//return number of path directories
	return kk;
}

//////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[], char *envp[])
{
	int showTiming = 0;
	int splitArg = 0;

	int operatingOnPATH = 0;
	int useShortPathNames = 0;

	FINDFILEDATA f0;
	FINDFILEDATA* fdFiles = &f0;

//	printf ("%s %s\n", APP_NAME, VERS_STR);

	//fail if we did not get proper number of command line args
	if (argc < 2 || argc > 5)
		printUsageAndDie ();

	//check command line parameters for switches
	int c0;
	int iBrief = 0;
	while (argc > 1 && ((c0 = **++argv) == '-' || c0 == '/')) {
		--argc;
		switch (*++*argv | 040) {
			case 'b':
				iBrief = 1;
				break;

			case 'd':
				debug = 1;
				break;

			case 'h':
				printUsageAndDie ();
				break;

			case 's':
				if (strcasecmp (*argv, "sp") == 0)
					splitArg = 1;
				else {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%s'", c0, *argv);
					printUsageAndDie (buf);
				}
				break;

			case 't':
				showTiming = 1;
				break;

			case 'x':
				useShortPathNames = 1;
				break;

			default:
				{
				char buf[64];
				sprintf (buf, "unrecognized argument, %c%s", c0, *argv);
				printUsageAndDie (buf);
				}
		}
	}

//	if (debug)
//		waitForDebugger ();

	//save file name
	strcpy (fileSpec, *argv++);

	if (splitArg) {
		//this is useful when run as follows: wh /sp "%PATH%"
		debug = 1;

		char header[] = "PATH=";
		char *pathVar = fileSpec;
		if (strncasecmp (pathVar, header, strlen (header)) == 0)
			pathVar += strlen (header); //step over header, if there
		appendSemi (pathVar);

//		operatingOnPATH = 1;

		int numPaths = splitPath (pathVar, pathArray, useShortPathNames);

		printf ("splitPath\n");
		for (int ii = 0; ii < numPaths; ii++)
			printf ("%s;\n", pathArray[ii]); //append semicolon
		printf ("\n");

		return 0;

	} else {
		myAssert (strlen (fileSpec) <= MAX_PATH);
	}

	char envBuf[BUFSZ] = "PATH";
	if (argc == 3)
		strcpy (envBuf, *argv++);

	if (strcasecmp (envBuf, "PATH") == 0)
		operatingOnPATH = 1;

	//get value of environment variable from environment
	char *envTmp;
	if ((envTmp = getenv (envBuf)) == NULL) {
		printf ("error getting \"%s\" from environment\n", envBuf);
		return 1;
	}

	GetCurrentDirectory (BUFSZ - 2, pathVar);
	appendSemi (pathVar);

	myAssert (strlen (envTmp) <= sizeof pathVar);
	strcat (pathVar, envTmp);
	appendSemi (pathVar);
	if (!iBrief)
		printf ("%s=%s\n\n", envBuf, pathVar);

	//if we are searching the path for executable files, we need to get and split PATHEXT
	strcpy (pathExtVar, "");
	if (operatingOnPATH) {
		char *pathExtTmp;
		if ((pathExtTmp = getenv ("PATHEXT")) == NULL) {
			printf ("error getting \"%s\" from environment\n", "PATHEXT");
			return 1;
		}

		//add blank extension to (beginning of) array to allow for wilcards like wh*
		strcpy (pathExtVar, ";");
		strcat (pathExtVar, pathExtTmp);
		appendSemi (pathExtVar);

		if (debug)
			printf ("PATHEXT=%s\n\n", pathExtVar);
	}

	//put each directory in the path into its own string
	int numPaths = splitPath (pathVar, pathArray, useShortPathNames);
	int numExts = splitPath (pathExtVar, extArray, 0/*useShortPathNames*/);

	if (debug) {
		printf ("splitPath\n");
		for (int ii = 0; ii < numPaths; ii++)
			printf ("%s;\n", pathArray[ii]); //append semicolon
		printf ("\n");
	}

	StringSet set1;
	StringSet::const_iterator kk;

	//search each directory in the path for the file
	DWORD dwTick0 = GetTickCount ();
	for (int ii = 0; ii < numPaths; ii++) {
		DWORD dwTick1 = GetTickCount ();

		for (int jj = 0; jj < numExts; jj++) {
			char fileSpec2[MAX_PATH];
			strcpy (fileSpec2, pathArray[ii]);
			appendSlash (fileSpec2);
			strcat (fileSpec2, fileSpec);
			strcat (fileSpec2, extArray[jj]);
			myAssert (strlen (fileSpec2) <= MAX_PATH);
			swapSlashes (fileSpec2);

			char *fileName = strrchr (fileSpec2, '\\');
			if (fileName != NULL) {
				*fileName = 0; //split spec into path and filename
				fileName++; //step over slash
			} //else
//TODO			handle error

			while ((p0 = myFindFile (fileSpec2, fileName, &fdFiles)) != NULL) {
				char *lowName = strlower (p0); //memory leak
				string str = lowName;
				kk = set1.find (str);

				//don't show dups unless we are showing timing or debug
				if (kk == set1.end () || showTiming || debug) { //not found
					set1.insert (str);
					if (!iBrief) {
						//build up output from several places
						char *p2 = formatFileinfo (fdFiles, FFI_SHOWFULLPATH);
						p2[39] = 0;
						printf ("%s", p2);
					}
					printf ("%s\n", p0);
				}
			}
		}

		if (showTiming)
			printf ("%8ld ms to process %s\n", GetTickCount () - dwTick1, pathArray[ii]);
	}

	if (showTiming)
		printf ("%8ld ms total\n", GetTickCount () - dwTick0);

	return 0;
}
