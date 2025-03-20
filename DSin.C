//Dsin does a 'dir /since /modified' of the current directory

#define APP_NAME "Dsin"
#define VERS_STR " (V8.01)"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include "fileio.h"

typedef struct tagFILESLIST {
	char *lpLine;		//complete formatted output line
	char *lpFileName;	//file name only
	char *lpFileSpec;	//file name plus path
	double dBytes;
	FILETIME ftLastWrite;
	} FILESLIST;
const int iMaxFiles = 60000;
FILESLIST fileList[iMaxFiles];

enum SortBy {None, Date, Exten, Name, Size};
SortBy sortBy;
int sortOrder = 1; //1 for normal sort, -1 for reverse sort

const int Space = 0x20; // ASCII space char

int bShowAttrs, bRecurseSubdirs, iAddlSubdirLevels, bIncludeDirs, bExcludeNonSource, bBrief;
int bForceFileFlush, bShowOnlySubdirs, bDoNotShowSubdirs;


///////////////////////////////////////////////////////////////////////////////
void printUsageAndDie (char *message = 0)
{
	if (message)
		fprintf (stderr, "\n%s: %s; use /h for help\n", APP_NAME, message);
	else
		fprintf (stderr, "\nUsage: %s [/a] [/e[=]<file>[,<file>]] [/f] [/h] [/nod] [/o[-]{d|e|n|s}] [/s] [/x]\n", APP_NAME);

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

///////////////////////////////////////////////////////////////////////////////
int compare1 (const void *ptr1, const void *ptr2)
{
	FILESLIST *file1 = (FILESLIST*) ptr1;
	FILESLIST *file2 = (FILESLIST*) ptr2;

	//if recursing subdirs, base name sort on full path
	char *str1 = bRecurseSubdirs ? file1->lpFileSpec : file1->lpFileName;
	char *str2 = bRecurseSubdirs ? file2->lpFileSpec : file2->lpFileName;

	int comp = 0;
	switch (sortBy) {
		default:
			printf("compare1: unknown sortBy value: %d\n", sortBy);
			break;

		case Date:
			comp = CompareFileTime (&file1->ftLastWrite, &file2->ftLastWrite);
			break;

		case Exten:
			comp = compareFileExten (str1, str2);
			break;

		case Size:
			comp = compareFileSize (file1->dBytes, file2->dBytes);
			break;

		case Name:
			; //fall through to return comparison of names
	}

	if (comp == 0) //fall back to sort by name
		comp = alphanumComp (str1, str2); //use alpha-numeric comparison

	return sortOrder * comp;
}

///////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
	char *p0;
	char line[MAX_PATH];
	char szInSpec[MAX_PATH];
	char szPathOnly[MAX_PATH];
	char szFileOnly[MAX_PATH];
	char *pExcludeFileList = NULL;
	int c0, c1, c2, debug;
	double dLargeFiles = 0;

	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;

	//set debug flag from environment
	debug = (getenv ("DEBUG") == NULL ? 0 : 1);

	sortBy = None;
	bShowAttrs = bRecurseSubdirs = bIncludeDirs = bExcludeNonSource = bBrief = 0;
	bForceFileFlush = bShowOnlySubdirs = bDoNotShowSubdirs = 0;
	bIncludeDirs = MFF_RETURN_SUBDIRS;
	iAddlSubdirLevels = 999;
	while (argc > 1 && ((c0 = **++argv) == '-' || c0 == '/')) {
		--argc;
		switch (c1 = (*++*argv | 040)) { //assign!
			case 'a':
//				checkArgAndDieOnError (*argv, 2, c0);
				if (strcasecmp (*argv, "a") == 0) {
					bShowAttrs = 1;
				} else if (strcasecmp (*argv, "ad") == 0) {
					bShowOnlySubdirs = 1;
				} else if (strcasecmp (*argv, "a-d") == 0) {
					bDoNotShowSubdirs = 1;
				} else {
					char buf[64];
					sprintf (buf, "unrecognized argument, %c%s", c0, *argv);
					printUsageAndDie (buf);
				}
				break;

			case 'b':
				checkArgAndDieOnError (*argv, 1, c0);
				bBrief = 1;
				break;

			case 'd':
				checkArgAndDieOnError (*argv, 1, c0);
				debug = 1;
				FileIoDebug = 1;
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
				checkArgAndDieOnError (*argv, 1, c0);
				bForceFileFlush = 1;
				break;

			case 'h':
				printUsageAndDie ();
				break;

			case 'l':
				//only print files larger than given arg
				//step over equals sign (or colon), if there is one
				if (*++*argv == '=' || **argv == ':')
					++*argv;

				dLargeFiles = unitSuffixScale (*argv);
				if (dLargeFiles == 0) {
					char buf[64];
					sprintf (buf, "incomplete argument '%c%c'", c0, c1);
					printUsageAndDie (buf);
				}
				break;

			case 'n':
				checkArgAndDieOnError (*argv, 3, c0);
				if (strcasecmp (*argv, "nod") == 0)
					bIncludeDirs = 0;
				else {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%s'", c0, *argv);
					printUsageAndDie (buf);
				}
				break;

			case 'o':
//				checkArgAndDieOnError (*argv, 2, c0);
				if (strcasecmp (*argv, "od") == 0) {
					sortBy = Date;
					sortOrder = 1;
				} else if (strcasecmp (*argv, "o-d") == 0) {
					sortBy = Date;
					sortOrder = -1;
				} else if (strcasecmp (*argv, "oe") == 0) {
					sortBy = Exten;
					sortOrder = 1;
				} else if (strcasecmp (*argv, "o-e") == 0) {
					sortBy = Exten;
					sortOrder = -1;
				} else if (strcasecmp (*argv, "on") == 0) {
					sortBy = Name;
					sortOrder = 1;
				} else if (strcasecmp (*argv, "o-n") == 0) {
					sortBy = Name;
					sortOrder = -1;
				} else if (strcasecmp (*argv, "os") == 0) {
					sortBy = Size;
					sortOrder = 1;
				} else if (strcasecmp (*argv, "o-s") == 0) {
					sortBy = Size;
					sortOrder = -1;
				} else {
					char buf[64];
					sprintf (buf, "unrecognized argument, %c%s", c0, *argv);
					printUsageAndDie (buf);
				}
				break;

			case 's':
//				checkArgAndDieOnError (*argv, 1, c0);
				c2 = *((*argv)+1);
				if (isdigit (c2)) {
					iAddlSubdirLevels = atol ((*argv)+1);
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS; //user specified /s<N>
				} else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS; //user specified /s
				break;

			case 'x':
				checkArgAndDieOnError (*argv, 1, c0);
				bExcludeNonSource = MFF_EXCLUDE_NONSOURCE;
				break;

			default:
				{
				char buf[64];
				sprintf (buf, "unrecognized argument, %c%s", c0, *argv);
				printUsageAndDie (buf);
				}
		}
	}

	//use next parameter, if there is one, as filename to search for
	if (--argc > 0) {
		strcpy (szInSpec, *argv);
	} else {
		strcpy (szInSpec, ALLFILES);
	}

	//convert path to windows format
	swapSlashes (szInSpec);

	if (debug) {
		printf ("%s\n", szInSpec);
	}

	p0 = myFileParse (szInSpec, szPathOnly, szFileOnly);
	if (!p0) {
		return 1; //myFileParse() prints error message
	}

	//next param, if there is one, is number of days to go back (default to 0)
	double dNumDays = (--argc > 0 ? atof (*++argv) : 0);

	//get current time and truncate to (last night at) midnight
	time_t t_now;
	time (&t_now);
	struct tm *tm_midnight = localtime (&t_now);
	tm_midnight->tm_sec = 0;
	tm_midnight->tm_min = 0;
	tm_midnight->tm_hour = 0;
	time_t t_midnight = mktime (tm_midnight);

	time_t t_cutoff = t_midnight - (time_t) (dNumDays * 86400);

	if (!bBrief) {
		printf ("cutoff time: %s", asctime (localtime (&t_cutoff))); //asctime appends newline

		printf (formatDirHeader (szPathOnly));
	}

	int iNumFiles = 0;
	double dBytes = 0;

	int ffFlags = bIncludeDirs | bRecurseSubdirs | bExcludeNonSource;
	while ((p0 = myFindFile (szPathOnly, szFileOnly, &fdFiles, ffFlags, iAddlSubdirLevels)) != NULL) {
		//skip files in the exclude file list, if requested
		if (pExcludeFileList && matchWildcardList (p0, pExcludeFileList)) {
			continue;
		}

		if ((bShowOnlySubdirs && !ISSUBDIRP (fdFiles)) || (bDoNotShowSubdirs && ISSUBDIRP (fdFiles))) {
			continue;
		}

		//try to force OS to flush file so we get accurate size/date info
		if (bForceFileFlush && !ISSUBDIRP (fdFiles)) {
			forceFileFlush (p0, fdFiles);
		}

		//only print if size is larger than specified, or test is disabled
		if (dLargeFiles != 0 && getFileSize (fdFiles) < dLargeFiles) {
			continue;
		}

		FILETIME ftLocalTime;
		if (sortBy == Date) {
			if (!FileTimeToLocalFileTime (&fdFiles->ftLastWriteTime, &ftLocalTime)) {
				printf ("%s: FileTimeToLocalFileTime() failed: %s", p0, formatLastError (GetLastError ()));
			}
		}

		//skip files that are older than cutoff time
		time_t t_file = windowsTickToUnixSeconds (fileTimeToUint64 (fdFiles->ftLastWriteTime));
		if (t_file < t_cutoff) {
			continue;
		}

		if (bBrief) {
			strcpy (line, p0);

		} else {
			if (bRecurseSubdirs) {
				char *p2 = formatFileinfo (fdFiles, FFI_SHOWFULLPATH);
				p2[39] = 0;
				sprintf (line, "%s%s", p2, p0);

			} else {
				strcpy (line, formatFileinfo (fdFiles));
			}
		}
		strcat (line, "\n");

		if (bShowAttrs) {
			char szAttribs[8];

			szAttribs[0] = ISSUBDIRP	(fdFiles) ? 'D' : Space;
			szAttribs[1] = ISARCHIVEP	(fdFiles) ? 'A' : Space;
			szAttribs[2] = ISSYSTEMP	(fdFiles) ? 'S' : Space;
			szAttribs[3] = ISHIDDENP	(fdFiles) ? 'H' : Space;
			szAttribs[4] = ISREADONLYP	(fdFiles) ? 'R' : Space;
			szAttribs[5] = ISCOMPRESSEDP(fdFiles) ? 'C' : Space;
			szAttribs[6] = Space;
			szAttribs[7] = 0;

//todo improve this
			char temp[MAX_PATH];
			strcpy (temp, szAttribs);
			strcat (temp, line);
			strcpy (line, temp);
		}

		//save fileList to be sorted
		if (sortBy != None) {
			myAssert (iNumFiles < iMaxFiles);
			fileList[iNumFiles].lpLine = strdup (line);
			fileList[iNumFiles].lpFileName = strdup (FILENAMEP (fdFiles));
			fileList[iNumFiles].lpFileSpec = strdup (p0);
			fileList[iNumFiles].dBytes = getFileSize (fdFiles);
			fileList[iNumFiles].ftLastWrite = ftLocalTime;

		} else {
			printf ("%s", line);
		}

		dBytes += getFileSize (fdFiles);
		iNumFiles++;
	}

	//if fileList should be sorted, sort it and print
	if (sortBy != None) {
		qsort (fileList, iNumFiles, sizeof (FILESLIST), compare1);

		for (int ii = 0; ii < iNumFiles; ii++) {
			printf ("%s", fileList[ii].lpLine);
		}
	}

	if (!bBrief) {
		if (iNumFiles == 0) {
			printf ("File not found\n");
		} else {
			printf ("%s", formatDirTrailer (iNumFiles, dBytes, *szPathOnly));
		}
	}

	return 0;
}
