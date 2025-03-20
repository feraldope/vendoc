//FileIO.C

//commented out 10/19/16
//#if !defined (_WIN32_WINNT) //added this after FileIO.C stopped building on duff
//#define _WIN32_WINNT 0x0400
//#endif

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <malloc.h>
//#include <direct.h>
#include "fileio.h"

#define BUFSZ MY_MAX_PATH

static char buf[BUFSZ];
static char *lpSlash = "\\";

//internal routines not visible to other modules
static char *_appendPathfile (const char *path_only, const char *file_only);
static char *_myFindAllFiles (const char *path, const char *file, FINDFILEDATA **fdUser, int iFlags, int iMaxSubdirLevels);
static int _countSlashes (const char *path);
static const char *_getChunk (const char *s, int slength, int marker);

int FileIoDebug;
int MallocDebug = 0;


///////////////////////////////////////////////////////////////////////////////
//myFileParse() takes a (possibly incomplete) file specification and breaks it
//	into a path (only) and filename (only), properly fleshed out, and
//	returns the complete path (or NULL if it fails)
//
//NOTE: THIS MODIFIES THE USER'S BUFFERS
//
char *myFileParse (const char *pInFullSpec, char *pOutPathOnly, char *pOutNameOnly)
{
#define FUNCNAME "myFileParse()"
	int iStatus = 1;
	int fChangedDrive = 0;
	int iDriveLetter;
	char *pSlash;
	char pOldPath[MY_MAX_PATH];
	char pOtherDiskOldPath[MY_MAX_PATH];
	static char szOutFullSpec[MY_MAX_PATH];
	char szWorkBuffer[MY_MAX_PATH] = "";
	char *pWorkBuffer = szWorkBuffer;

	//special handling for stdin
	if (usingStdin (pInFullSpec)) {
		strcpy (pOutPathOnly, pInFullSpec);
		strcpy (pOutNameOnly, pInFullSpec);
		return pOutNameOnly;
	}

	if (*pInFullSpec == '\\' && *(pInFullSpec + 1) == '\\') {
		fprintf (stderr, "%s: UNC paths not currently supported\n", FUNCNAME);
		return NULL;
	}

	//copy input filespec to our buffer and truncate the others
	strcpy (pWorkBuffer, pInFullSpec);
	szOutFullSpec[0] = pOutPathOnly[0] = pOutNameOnly[0] = 0;

	//remove any leading and/or trailing slashes
	if (*pWorkBuffer == '"')
		pWorkBuffer++;
	int iLength = strlen (pWorkBuffer);
	if (iLength > 1 && *(pWorkBuffer + iLength - 1) == '"')
		*(pWorkBuffer + iLength - 1) = 0;

	//save current path
	if (GetCurrentDirectory (MY_MAX_PATH - 1, pOldPath) == FALSE) {
		fprintf (stderr, "%s: GetCurrentDirectory() failed: %s", FUNCNAME, formatLastError (GetLastError ()));
		return NULL;
	}

	//strip disk from pWorkBuffer and change to it, or get current disk from OS
	if (isalpha (*pWorkBuffer) && *(pWorkBuffer + 1) == ':') {
		iDriveLetter = *pWorkBuffer;

		//if we change drives, we have to save that drive's pOldPath
		if (iDriveLetter != pOldPath[0]) {
			//fail if we can't change to the drive or get the path
			char szTemp[3] = "?:";
			szTemp[0] = iDriveLetter;
			if (SetCurrentDirectory (szTemp) == FALSE) {
				fprintf (stderr, "%s: SetCurrentDirectory (%s) failed: %s", FUNCNAME, szTemp, formatLastError (GetLastError ()));
				return NULL;
			}

			if (GetCurrentDirectory (MY_MAX_PATH - 1, pOtherDiskOldPath) == FALSE) {
				fprintf (stderr, "%s: branch not implemented\n", FUNCNAME);
				return NULL;
			}

			fChangedDrive = 1;
		}

		pWorkBuffer += 2;

	} else {
		//no drive specified, so get current drive
		char szTemp[MY_MAX_PATH];
		GetCurrentDirectory (MY_MAX_PATH - 1, szTemp);
		iDriveLetter = szTemp[0];
	}

	//if spec is empty, return current dir
	if (*pWorkBuffer == 0) {
		GetCurrentDirectory (MY_MAX_PATH - 1, szOutFullSpec);
		iDriveLetter = *szOutFullSpec;
		strcpy (pOutPathOnly, szOutFullSpec + 2);

	//see if remainder of filespec is path by trying to cd to it
	} else if (SetCurrentDirectory (pWorkBuffer) == TRUE) {
		GetCurrentDirectory (MY_MAX_PATH - 1, szOutFullSpec);
		iDriveLetter = *szOutFullSpec;
		strcpy (pOutPathOnly, szOutFullSpec + 2);

	//remainder of filespec is not valid path so find the final slash if there is one
	} else if ((pSlash = strrchr (pWorkBuffer, '\\')) != NULL) {
		char szTemp[MY_MAX_PATH];

		//assume final slash separates path from filename and proceed
		*pSlash++ = 0; //terminate path and point to following file

		strcpy (szTemp, pWorkBuffer);
		if (*szTemp == 0)
			strcpy (szTemp, "\\");

		//try to chdir to it
		if (SetCurrentDirectory (szTemp) == TRUE) {
			strcpy (pOutNameOnly, pSlash);

			//get the cwd and save it in the struct
			GetCurrentDirectory (MY_MAX_PATH - 1, szOutFullSpec);
			iDriveLetter = *szOutFullSpec;
			strcpy (pOutPathOnly, szOutFullSpec + 2);

		} else {
			iStatus = 0;
		}

	//no slash, assume what is left is a file name
	} else {
		strcpy (pOutNameOnly, pWorkBuffer);

		//get the cwd and save it in the struct
		GetCurrentDirectory (MY_MAX_PATH - 1, szOutFullSpec);
		iDriveLetter = *szOutFullSpec;
		strcpy (pOutPathOnly, szOutFullSpec + 2);
	}

	//if filename is empty, make it "*"
	if (pOutNameOnly[0] == 0) {
		strcpy (pOutNameOnly, "*");

	//if filename is ".ext" make it "*.ext"
	} else if (pOutNameOnly[0] == '.' && pOutNameOnly[1] != 0) {
		memmove (pOutNameOnly + 1, pOutNameOnly, strlen (pOutNameOnly) + 1);
		pOutNameOnly[0] = '*';
	}

	//now insert the drive designation in front of the path
	memmove (pOutPathOnly + 2, pOutPathOnly , strlen (pOutPathOnly) + 1);
	pOutPathOnly[0] = iDriveLetter;
	pOutPathOnly[1] = ':';

	//change back to our old disk and directory and if we changed
	//drives, change back to the other drive's old directory
	if (fChangedDrive)
		SetCurrentDirectory (pOtherDiskOldPath);
	SetCurrentDirectory (pOldPath);

	if (iStatus == 0 || *szOutFullSpec == 0) {
		fprintf (stderr, "%s: could not parse \"%s\"\n", FUNCNAME, pInFullSpec);
		return NULL;

	} else {
		appendSlash (szOutFullSpec);
		strcat (szOutFullSpec, pOutNameOnly);
		return szOutFullSpec;
	}
}
#undef FUNCNAME

////////////////////////////////////////////////////////////////////////////
char *myFindFile (const char *lpPathOnly, const char *lpFileOnly, FINDFILEDATA **fdFiles, int iFlags, int iAddlSubdirLevels)
{
	char *p0;
	char *p1;

	//special handling for stdin
	if (p0 = usingStdin (lpPathOnly)) //assign!
		return p0;

	while (1) {
		int iMaxSubdirLevels = _countSlashes (lpPathOnly) + iAddlSubdirLevels;

		p0 = _myFindAllFiles (lpPathOnly, lpFileOnly, fdFiles, iFlags, iMaxSubdirLevels);
		if (!p0) {
			return NULL;
		}

		if ((p1 = strrchr (p0, '\\')) == NULL) { //find trailing slash
			fprintf (stderr, "myFindFile(): strrchr failed\n");
			return NULL;
		}

		if (!(iFlags & MFF_RETURN_SUBDIRS) && ISSUBDIRP (*fdFiles)) {
			continue;
		}

		if ((iFlags & MFF_EXCLUDE_NONSOURCE) && !isSourceFile (*fdFiles, p0, iFlags)) {
			continue;
		}

		p1++; //step over trailing slash
		if (strcmp (p1, ".") && strcmp (p1, "..") &&
				matchWildcardList (p1, lpFileOnly, /* extendedFileMatching = */ 0)) {
			return p0;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//this returns a pointer to the next file in path and recursively searches
//it's subdirectories.
//
//NOTES and shortcomings:
//- you can't start the search over with new data if the previous search
//  didn't complete because cur is only cleared at the end
//- subsequent calls reuse same char array, destroying data from previous call
//- no checking is done to see if the path strings overflow the arrays
//
char *_myFindAllFiles (const char *path, const char *file, FINDFILEDATA **fdUser, int iFlags, int iMaxSubdirLevels)
{
	const int SIZENEEDED (sizeof (struct dirnode) + sizeof (FINDFILEDATA));

//	fprintf (stderr, "sizeof (struct dirnode) = %d\n", sizeof (struct dirnode));
//	fprintf (stderr, "sizeof FINDFILEDATA = %d\n", sizeof FINDFILEDATA);

	static struct dirnode *cur = NULL;
	static struct dirnode *tail = NULL;
	static HANDLE hFound;
	static int malloc_count = 0;
	static int free_count = 0;

#if 1 //debug
	struct dirnode *cur1 = cur;
	struct dirnode *next1 = 0;
	if (cur && cur->next)
		next1 = cur->next;
	struct dirnode *tail1 = tail;
#endif

	if (FileIoDebug)
		fprintf (stderr, "_myFindAllFiles() path=\"%s\", file=\"%s\"\n", path, file);

	//on the first pass, allocate the root struct and recursively call ourselves
	if (cur == NULL) {
		if ((cur = (struct dirnode *)malloc (SIZENEEDED)) == NULL) {
			perror ("Error: _myFindAllFiles() @ malloc 0");
			return NULL;
		}

		if (FileIoDebug && MallocDebug) {
			fprintf (stderr, "_myFindAllFiles() after malloc(%d) %d: 0x%08lX (%s)\n",
					SIZENEEDED, ++malloc_count, cur, path);
		}

		tail = cur;
		cur->next = NULL;
		cur->firstpass = 1;
		strcpy (cur->path, path);
		strcpy (cur->spec, _appendPathfile (path, "*.*"));
//		strcpy (cur->spec, _appendPathfile (path, file));
		cur->lpfdFiles = (FINDFILEDATA *) ((char*)cur + sizeof (struct dirnode));

		return _myFindAllFiles (cur->spec, file, fdUser, iFlags, iMaxSubdirLevels);
	}

	if ((cur->firstpass && (hFound = FindFirstFile (cur->spec, cur->lpfdFiles)) != INVALID_HANDLE_VALUE) ||
	   (!cur->firstpass && FindNextFile (hFound, cur->lpfdFiles))) {

		cur->firstpass = 0;
//		int currSubdirLevel = _countSlashes (cur->spec);
		int currSubdirLevel = _countSlashes (cur->path);

		//special handling for junctions to get the file last write time
		if ((ISJUNCTIONP (cur->lpfdFiles))) {
		    //call CreateFile to get a HANDLE to the file
			DWORD dwFlagsAndAttributes = FILE_FLAG_BACKUP_SEMANTICS; //necessary for directories?
			DWORD dwShareMode = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
			HANDLE handle = CreateFile (_appendPathfile (cur->path, FILENAMEP (cur->lpfdFiles)),
	    		                        GENERIC_READ, dwShareMode, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL);
			if (handle == INVALID_HANDLE_VALUE) {
				fprintf (stderr, "_myFindAllFiles() CreateFile failed on (%s) with error %d\n", FILENAMEP (cur->lpfdFiles), GetLastError());
//TODO?			return 0;
			}

			//get last write time and copy it into our object
			BY_HANDLE_FILE_INFORMATION fileInfo;
			if (GetFileInformationByHandle (handle, &fileInfo)) {
				FILETIME lastWriteTime = fileInfo.ftLastWriteTime;
				cur->lpfdFiles->ftLastWriteTime = lastWriteTime;
			} else {
				fprintf (stderr, "_myFindAllFiles() GetFileInformationByHandle failed on (%s) with error %d\n", FILENAMEP (cur->lpfdFiles), GetLastError());
//TODO?			return 0;
			}
		}

		//if this is a subdir, but not . or ..
		//and caller wants subdirs, add it to our tree
		if ((ISSUBDIRP (cur->lpfdFiles)) &&
			strcmp (FILENAMEP (cur->lpfdFiles), ".") != 0 &&
			strcmp (FILENAMEP (cur->lpfdFiles), "..") != 0 &&
			(iFlags & MFF_RECURSE_SUBDIRS) &&
			currSubdirLevel < iMaxSubdirLevels) {

			if ((tail->next = (struct dirnode *)malloc (SIZENEEDED)) == NULL) {
				perror ("Error: _myFindAllFiles() @ malloc 1");
				printf (" [last dir was %s]\n", cur->path);
				return NULL;
			}

			strcpy (tail->next->path, _appendPathfile (cur->path, FILENAMEP (cur->lpfdFiles)));

			if (FileIoDebug && MallocDebug) {
				fprintf (stderr, "_myFindAllFiles() after malloc(%d) %d: 0x%08lX (%s)\n",
						SIZENEEDED, ++malloc_count, tail->next, tail->next->path);
			}

			strcpy (tail->next->spec, _appendPathfile (tail->next->path, "*.*"));
			tail = tail->next;
			tail->next = NULL;
			tail->firstpass = 1;
			tail->lpfdFiles = (FINDFILEDATA *) ((char*)cur + sizeof (struct dirnode));
		}

		//return the file appended to the path
		*fdUser = cur->lpfdFiles;

		return _appendPathfile (cur->path, FILENAMEP (cur->lpfdFiles));

	} else {
		//no more files found in this directory so step down the tree and call ourselves again
		if (cur->next != NULL) {
			//step to the next node and delete the previous
			struct dirnode *tmp = cur;
			cur = cur->next;
			if (FileIoDebug && MallocDebug) {
				fprintf (stderr, "_myFindAllFiles() before free %d: 0x%08lX (%s)\n",
						++free_count, tmp, tmp->path);
			}
//BUG - memory leak? - why does this free cause a crash???
//			free (tmp);
			tmp = NULL;

			return _myFindAllFiles (cur->path, file, fdUser, iFlags, iMaxSubdirLevels);

		} else {
			//no more nodes in tree, free the last node before returning
			free (cur);
			cur = NULL;
			return NULL;
		}
	}
}

#if defined (_MSC_VER)
///////////////////////////////////////////////////////////////////////////////
//caller should delete returned array
wchar_t *toWide (const char *ascii)
{
	int len = strlen (ascii) + 1;
	wchar_t *wide = new wchar_t [len]; //memory leak!

//	if (FileIoDebug) {
//		int ii = MultiByteToWideChar (CP_ACP, 0, ascii, -1, wide, 0);
//		fprintf (stderr, "ii: %d\n", ii);
//	}

	MultiByteToWideChar (CP_ACP, 0, ascii, -1, wide, len);

//	if (FileIoDebug) {
//		fprintf (stderr, "ascii: '%s'\n", ascii);
//		wprintf (L"wide:  '%s'\n", wide);
//	}

	return wide;
}

///////////////////////////////////////////////////////////////////////////////
//caller should delete returned array
char *fromWide (const wchar_t *wide, int stripTrailingCarriageControl)
{
	int len = wcslen (wide) + 1;
	char *ascii = new char [len]; //memory leak!

//	if (FileIoDebug) {
//		int ii = WideCharToMultiByte (CP_ACP, 0, wide, -1, ascii, 0, NULL, NULL);
//		fprintf (stderr, "ii: %d\n", ii);
//	}

	WideCharToMultiByte (CP_ACP, 0, wide, -1, ascii, len, NULL, NULL);

	if (stripTrailingCarriageControl) {
		int len = strlen (ascii);
		if (len >= 2 && ascii[len - 2] == 0x0d && ascii[len - 1] == 0x0a)
			ascii[len - 2] = 0;
	}

//	if (FileIoDebug) {
//		wprintf (L"wide:  '%s'\n", wide);
//		fprintf (stderr, "ascii: '%s'\n", ascii);
//	}

	return ascii;
}

////////////////////////////////////////////////////////////////////////////
//support for UNICODE files
char *myfgets (char *line, int len, FILE *stream, int bUnicodeFile)
{
	if (bUnicodeFile) {
		if (feof (stream))
			return NULL;

		wchar_t *wide = new wchar_t [len + 1];
		fgetws (wide, len, stream);

		char *ascii = fromWide (wide);
		strcpy (line, ascii);
		delete [] wide;
		delete [] ascii;

		return line;

	} else {
		return fgets (line, len, stream);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//provide support for long filenames
FILE *myfreopen (const char *path, const char *mode, FILE *stream)
{
	FILE *file = NULL;

	if (strlen (path) >= _MAX_PATH) {
		const char *prefix = "\\\\?\\"; //prefix for long filenames
		char *buf = new char [strlen (path) + strlen (prefix) + 1];

		strcpy (buf, prefix);
		strcat (buf, path);

		file = freopen (buf, mode, stream);

		delete [] buf;

	} else {
		file = freopen (path, mode, stream);
	}

	return file;
}

///////////////////////////////////////////////////////////////////////////////
int _countSlashes (const char *path)
{
	int len = strlen (path);
	int slashes = 0;

	//special handling for the root of the drive
	if (len == 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
		return 0;

	for (int ii = 0; ii < len; ii++)
		if (path[ii] == '/' || path[ii] == '\\')
			slashes++;

//TODO - don't count trailing final slash if there???

	return slashes;
}

///////////////////////////////////////////////////////////////////////////////
char *getPath (const char *fileSpec) //not reentrant; returns path from complete fileSpec
{
	static char buffer[MY_MAX_PATH];

	strcpy (buffer, fileSpec);

	int len = strlen (buffer);
	if (len > 1)
		len--;

	//strip off last slash and everything after it
	while (len >= 0 && buffer[len] != '\\' && buffer[len] != '/')
		len--;
	buffer[len] = 0;

	return buffer;
}

////////////////////////////////////////////////////////////////////////////
//this returns a pointer to a string containing formatted file info
char *formatFileinfo (FINDFILEDATA *pfdFiles, int iFlags, const char *szPathOnly)
{
	static char buf[MY_MAX_PATH]; //return string in this buffer
	FILETIME ftLocalTime;
	SYSTEMTIME stSystemTime;
	char *pFilesize;
	char *pFilename;

	if (iFlags & FFI_SHOWFULLPATH) {
		if (szPathOnly && *szPathOnly) {
			static char buf1[MY_MAX_PATH];
			strcpy (buf1, szPathOnly);
			strcat (buf1, "\\");
			strcat (buf1, FILENAMEP (pfdFiles));
			pFilename = buf1;
		} else {
			pFilename = fileExists1 (FILENAMEP (pfdFiles));
		}
	} else
		pFilename = FILENAMEP (pfdFiles);

//not implemented yet
//#define FFI_ATTRIBUTES   2

	//if this is a subdir or junction, print string instead of size
	if (ISJUNCTIONP(pfdFiles))
		pFilesize = "<JUNCTION>    ";
	else if (ISSUBDIRP(pfdFiles))
		pFilesize = "<DIR>         ";
	else {
		double fileSize = getFileSize (pfdFiles);
		pFilesize = insertCommas (fileSize);
	}

	if (!FileTimeToLocalFileTime (&pfdFiles->ftLastWriteTime, &ftLocalTime))
		printf ("FileTimeToLocalFileTime() failed: %s", formatLastError (GetLastError ()));

#if 0 //truncate away any precision less than 1 second
	DWORD tmp1 = ftLocalTime.dwLowDateTime;
	DWORD tmp2 = tmp1 - tmp1 % 100000000;
	ftLocalTime.dwLowDateTime = tmp2;
#endif

	if (!FileTimeToSystemTime (&ftLocalTime, &stSystemTime))
		printf ("FileTimeToSystemTime() failed: %s", formatLastError (GetLastError ()));

	int bytes = sprintf (buf, "%02d/%02d/%02d  %02d:%02d:%02d.%03d %15s %s",
						 stSystemTime.wMonth, stSystemTime.wDay,
						 stSystemTime.wYear % 100, //Y2K
						 stSystemTime.wHour, stSystemTime.wMinute,
						 stSystemTime.wSecond, stSystemTime.wMilliseconds,
						 pFilesize, pFilename);
	myAssert (bytes < sizeof buf);

	return buf;
}

///////////////////////////////////////////////////////////////////////////////
//this builds a complete path from a path (only) and a filename (only)
//with a slash appended to the path, if there isn't one there already
//
//NOTE: subsequent calls reuse same char array, destroying data from previous call
char *_appendPathfile (const char *path_only, const char *file_only)
{
	const int MaxPathLen = 8 * _MAX_PATH;
	static char spec[MaxPathLen + 1];

	int len = strlen (path_only) + 1 + strlen (file_only);
	if (len >= MaxPathLen)
		fprintf (stderr, "**** _appendPathfile: the path is too long (%d > %d): \"%s\\%s\"\n",
				 len, MaxPathLen, path_only, file_only);
//TODO - truncate path string?

	strcpy (spec, path_only);
	appendSlash (spec);
	strcat (spec, file_only);

	if (len >= MaxPathLen)
		spec[MaxPathLen - 1] = 0;
//TODO - just truncating the string is not the correct solution

	return spec;
}

////////////////////////////////////////////////////////////////////////////
//returns TRUE if the filename matches one of the patterns in the comma-separated wildcardList (ignores differences in slashes)
int matchWildcardList (const char *userFilename, const char *userWildcardList, int extendedFileMatching)
{
	char *filename = strndup (userFilename);
	swapSlashes (filename);

	char *wildcardList = strndup (userWildcardList);
	swapSlashes (wildcardList);

	int found = 0;
	char *token = strtok (wildcardList, ",");
	while (token) {
		//test for exact match
		if (matchtest (filename, token)) {
			found = 1;
			break;
		}

		if (extendedFileMatching) {
			//if that did not match, try again as if it is the file name (i.e., match "*\<token>")
			char token1[MY_MAX_PATH] = { '*', '\\', 0 };
			strcat (token1, token);
			if (matchtest (filename, token1)) {
				found = 1;
				break;
			}

			//if that did not match, try again as if it is a path component (i.e., match "*\<token>\*")
			strcat (token1, "\\*");
			if (matchtest (filename, token1)) {
				found = 1;
				break;
			}
		}

		token = strtok (NULL, ",");
	}

	delete[] filename;
	delete[] wildcardList;

	return found;
}

////////////////////////////////////////////////////////////////////////////
//returns TRUE if string matches one of the strings in the comma-separated list
int matchStringInList (const char *lpString, const char *lpList, int ignoreCase)
{
	char *list = strndup (lpList);
	if (ignoreCase)
		strlwr (list);

	char *string = strndup (lpString);
	if (ignoreCase)
		strlwr (string);

	int found = 0;
	char *token = strtok (list, ",");
	while (token) {
		if (strstr (string, token)) {
			found = 1;
			break;
		}

		token = strtok (NULL, ",");
	}

	delete[] list;
	delete[] string;

	return found;
}

///////////////////////////////////////////////////////////////////////////////
//return TRUE if the file passed in is a 'source' file
int isSourceFile (FINDFILEDATA *pfdFiles, char *lpFileSpec, int iFlags)
{
	if (ISSUBDIRP (pfdFiles))
		return 0; //directories are not considered source files

	//find the extension, if any
	char *ext = strrchr (FILENAMEP (pfdFiles), '.');

	//files with no extension are considered source files
	if (ext == NULL)
		return 1;

	//step over period
	ext++;

	//list below every extension that IS source but is binary (i.e., not diff'able)
	int isBinarySource = (
		strcasecmp (ext, "aar")  == 0 ||
		strcasecmp (ext, "au")  == 0 ||
		strcasecmp (ext, "biar")== 0 ||
		strcasecmp (ext, "bmp") == 0 ||
		strcasecmp (ext, "dbf") == 0 ||
		strcasecmp (ext, "doc") == 0 ||
		strcasecmp (ext, "docx")== 0 ||
		strcasecmp (ext, "gdb") == 0 ||
		strcasecmp (ext, "gif") == 0 ||
		strcasecmp (ext, "gz")  == 0 ||
		strcasecmp (ext, "ico") == 0 ||
		strcasecmp (ext, "jpg") == 0 ||
		strcasecmp (ext, "jpeg")== 0 ||
		strcasecmp (ext, "mar") == 0 ||
		strcasecmp (ext, "mdb") == 0 ||
		strcasecmp (ext, "mp3") == 0 ||
		strcasecmp (ext, "mp4") == 0 ||
		strcasecmp (ext, "mpg") == 0 ||
		strcasecmp (ext, "pdf") == 0 ||
		strcasecmp (ext, "png") == 0 ||
		strcasecmp (ext, "ppt") == 0 ||
		strcasecmp (ext, "psd") == 0 ||
		strcasecmp (ext, "rar") == 0 ||
		strcasecmp (ext, "rpt") == 0 ||
		strcasecmp (ext, "rtf") == 0 ||
		strcasecmp (ext, "sbw") == 0 ||
		strcasecmp (ext, "sdf") == 0 ||
		strcasecmp (ext, "sln") == 0 ||
		strcasecmp (ext, "swf") == 0 ||
		strcasecmp (ext, "tar") == 0 ||
		strcasecmp (ext, "tgz") == 0 ||
		strcasecmp (ext, "tif") == 0 ||
		strcasecmp (ext, "ttf")  == 0 ||
		strcasecmp (ext, "tz")  == 0 ||
		strcasecmp (ext, "vm")  == 0 ||
		strcasecmp (ext, "wmv") == 0 ||
		strcasecmp (ext, "wav") == 0 ||
		strcasecmp (ext, "xlf") == 0 ||
		strcasecmp (ext, "xls") == 0 ||
		strcasecmp (ext, "xlsx")== 0 ||
		strcasecmp (ext, "zip") == 0 ||
		strcasecmp (ext, "z")   == 0
		);

	//incomplete list of extensions that are source
	//.clw .dep .dsp .dsw .rc .rc2 .plt

	//list below every extension that is NOT source (or that should always be skipped)
	int isNonSource = (
		matchtest (ext, "aaa%") == 1 || //oracle DB save files
		strcasecmp (ext, "a")   == 0 ||
		strcasecmp (ext, "ap_") == 0 ||
		strcasecmp (ext, "apk") == 0 ||
		strcasecmp (ext, "aps") == 0 ||
		strcasecmp (ext, "avi") == 0 ||
//		strcasecmp (ext, "bak") == 0 ||
		strcasecmp (ext, "bin") == 0 ||
		strcasecmp (ext, "bsc") == 0 ||
		strcasecmp (ext, "cab") == 0 ||
		strcasecmp (ext, "chm") == 0 ||
		strcasecmp (ext, "class") == 0 ||
		strcasecmp (ext, "_d")  == 0 ||
		strcasecmp (ext, "dep") == 0 ||
		strcasecmp (ext, "dex") == 0 ||
		strcasecmp (ext, "dll") == 0 ||
		strcasecmp (ext, "enum") == 0 ||
		strcasecmp (ext, "err") == 0 ||
		strcasecmp (ext, "exe") == 0 ||
		strcasecmp (ext, "exp") == 0 ||
		strcasecmp (ext, "fm")  == 0 ||
		strcasecmp (ext, "hh") == 0 ||
		strcasecmp (ext, "hlp") == 0 ||
		strcasecmp (ext, "hpj") == 0 ||
		strcasecmp (ext, "idb") == 0 ||
		strcasecmp (ext, "idx") == 0 ||
		strcasecmp (ext, "ilk") == 0 ||
		strcasecmp (ext, "img") == 0 ||
		strcasecmp (ext, "jar") == 0 ||
		strcasecmp (ext, "lib") == 0 ||
//		strcasecmp (ext, "log") == 0 ||
		strcasecmp (ext, "manifest") == 0 ||
		strcasecmp (ext, "map") == 0 ||
		strcasecmp (ext, "mdp") == 0 ||
		strcasecmp (ext, "mif") == 0 ||
		strcasecmp (ext, "mpp") == 0 ||
		strcasecmp (ext, "msi") == 0 ||
		strcasecmp (ext, "ncb") == 0 ||
		strcasecmp (ext, "o")   == 0 ||
		strcasecmp (ext, "obj") == 0 ||
		strcasecmp (ext, "opt") == 0 ||
		strcasecmp (ext, "pack") == 0 ||
		strcasecmp (ext, "pch") == 0 ||
		strcasecmp (ext, "pdb") == 0 ||
		strcasecmp (ext, "plg") == 0 ||
		strcasecmp (ext, "ppm") == 0 ||
		strcasecmp (ext, "pst") == 0 ||
		strcasecmp (ext, "rbj") == 0 ||
		strcasecmp (ext, "rde") == 0 ||
		strcasecmp (ext, "res") == 0 ||
		strcasecmp (ext, "sbr") == 0 ||
		strcasecmp (ext, "sde") == 0 ||
		strcasecmp (ext, "sio") == 0 ||
		strcasecmp (ext, "sl")  == 0 ||
		strcasecmp (ext, "sln") == 0 ||
		strcasecmp (ext, "so")  == 0 ||
		strcasecmp (ext, "stf") == 0 ||
		strcasecmp (ext, "suo") == 0 ||
		strcasecmp (ext, "tar.obsolete") == 0 ||
		strcasecmp (ext, "vcp") == 0 ||
		strcasecmp (ext, "war") == 0 ||
		strcasecmp (ext, "xpi") == 0 ||
		strcasecmp (ext, "xpm") == 0 ||
		strcasecmp (ext, "zip.obsolete") == 0 ||
		strcasecmp (FILENAMEP (pfdFiles), ".cmake.state") == 0 ||
		strcasecmp (FILENAMEP (pfdFiles), "BuildLog.htm") == 0 ||
//		strcasecmp (FILENAMEP (pfdFiles), "_copy.bat") == 0 ||
		strcasecmp (FILENAMEP (pfdFiles), "makefile.nutc.3.0") == 0 ||
		strcasecmp (FILENAMEP (pfdFiles), "makefile.nt.5.0") == 0 ||
		strncmp (FILENAMEP (pfdFiles), ".mvfs_", 6) == 0 ||
		matchtest (FILENAMEP (pfdFiles), "*.vcproj.*.*.user") == 1 ||
//TODO: this never sees files without an extension in the .git folder because they are caught by earlier code, so they show up as source
		(lpFileSpec && matchtest (lpFileSpec, "*\\workspaces\\*\\.git\\*") == 1) ||
		(lpFileSpec && matchtest (lpFileSpec, "*\\workspaces\\*\\bin\\*") == 1) ||
		(lpFileSpec && matchtest (lpFileSpec, "*\\workspaces\\*\\build\\*") == 1) ||
		(lpFileSpec && matchtest (lpFileSpec, "*\\lost+found\\*") == 1)
		);

	if ((iFlags & MFF_ISS_BOTH) == MFF_ISS_TEXT) {
		if (!isBinarySource && !isNonSource)
			return 1;
		else
			return 0;
	}

	if ((iFlags & MFF_ISS_BOTH) == MFF_ISS_BINARY) {
		if (isBinarySource && !isNonSource)
			return 1;
		else
			return 0;
	}

	return !isNonSource;
}

///////////////////////////////////////////////////////////////////////////////
char *formatLastError (int iError) //not reentrant - returns pointer to static buffer
{
	static char szBuffer[BUFSZ];

//	DWORD dwLanguageID = GetUserDefaultLangID ();
//	DWORD dwLanguageID = MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US);
	DWORD dwLanguageID = MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT);
	DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

	int iLength = sizeof (szBuffer);
	int iCount = FormatMessage (dwFlags, NULL, iError, dwLanguageID, szBuffer, iLength, NULL);

	//if the format failed, just print the number
	if (iCount == 0) {
#if defined (_MSC_VER)
		_itoa (iError, szBuffer, 10);
#elif defined (__GNUG__)
		sprintf (szBuffer, "error=%d", iError);
#endif
	}

#if 0 //remove trailing spaces and periods that Windows appends
	while (szBuffer[iCount - 1] == ' ' || szBuffer[iCount - 1] == '.') {
		szBuffer[--iCount] = 0;
	}
#endif

	return szBuffer; //FormatMessage() appends newline (0x0D, 0x0A)
}

///////////////////////////////////////////////////////////////////////////
char *formatDirHeader (const char *path)
{
	DWORD dwVolumeSerialNumber;
	char *pSlash;
	char tmppath[MY_MAX_PATH], volume[MY_MAX_PATH];
	static char buf[MY_MAX_PATH];

	//search for volume label in root directory of given path
	//(by, for example, converting "A:\SUBDIR" to "A:\*.*")
	strcpy (tmppath, path);
	tmppath[3] = 0;

	if (GetVolumeInformation (tmppath, volume, sizeof (volume), &dwVolumeSerialNumber,
							  NULL, NULL, NULL, 0) != TRUE)
		printf ("GetVolumeInformation() failed: %s", formatLastError (GetLastError ()));
	if (*volume == 0)
		strcpy (volume, "blank"); //todo - fix this

	//remove trailing slash from path if this is not the root directory
	strcpy (tmppath, path);
	pSlash = tmppath + strlen (tmppath) - 1;
	if (*pSlash == '\\')
		*pSlash = 0;

	//build directory header string
	sprintf (buf, "\n Volume in drive %c is %s\n"
					" Volume Serial Number is %04lX-%04lX\n\n"
					" Directory of %s\n\n",
					*tmppath, volume,
					HIWORD (dwVolumeSerialNumber),
					LOWORD (dwVolumeSerialNumber),
					tmppath);

	return buf;
}

///////////////////////////////////////////////////////////////////////////
char *formatDirTrailer (int iNumFiles, double dNumBytes, int iDrive)
{
	int iStatus;
	double dNumFreeBytes = 0;
	static char buf[MY_MAX_PATH];
	char szNumBytes[BUFSZ];
	char szNumFreeBytes[BUFSZ];

	char szRootPathName[] = "?:\\";
	DWORD dwSectorsPerCluster, dwBytesPerSector;
	DWORD dwNumberOfFreeClusters, dwTotalNumberOfClusters;
	szRootPathName[0] = toupper (iDrive);
	iStatus = GetDiskFreeSpace (szRootPathName, &dwSectorsPerCluster,
								&dwBytesPerSector, &dwNumberOfFreeClusters,
								&dwTotalNumberOfClusters);

	if (iStatus)
		dNumFreeBytes = (double) dwBytesPerSector * (double) dwSectorsPerCluster * (double) dwNumberOfFreeClusters;

	strcpy (szNumBytes, insertCommas (dNumBytes));
	strcpy (szNumFreeBytes, insertCommas (dNumFreeBytes));
	sprintf (buf, "%16d File(s)%15s bytes\n%39s bytes free\n",
				iNumFiles, szNumBytes, szNumFreeBytes);

	return buf;
}

#define MAXDIRS 100000
#define DEBUG_BUILD_DIR_TREE 0

///////////////////////////////////////////////////////////////////////////////
int _getDirTreeCompare (const void *ptr1, const void *ptr2)
{
	return strcasecmp (*(char**) (ptr1), *(char**) (ptr2));
}

///////////////////////////////////////////////////////////////////////////
//returns an array of pointers to parent and all its subdirectories
char **getDirTree (const char *parent, int *index, int sortResults)	//index = ptr to index of last element in array
{
	//NOTE that "*" is faster than "*.*" because (for some unknown reason) using
	//dos_findfirst (_A_SUBDIR) returns all files, not just dirs, but it won't
	//find any dirs with "." in the name
#ifdef WIN32
	static char *strall = "*";
#else
	static char *strall = "*.*";
#endif

	static char *tree[MAXDIRS];
	static char file_spec[MY_MAX_PATH];
	static int found;
	static int error_already_reported;
	FINDFILEDATA fdFiles;
#if defined (WIN32)
	HANDLE hFound;
	BOOL bFound;
#endif //OS

//	if (FileIoDebug)
//		fprintf (stderr, "getDirTree: called with parent = [%s]\n", parent);

	//if this is the first call, save the root directory before continuing
	if (*index == 0) {
		if ((tree[*index] = (char*) malloc (strlen (parent) + 1)) == NULL) {
			perror ("malloc");
			exit (EXIT_FAILURE);
		}

		strcpy (tree[(*index)++], parent);

		//reset this on first call
		error_already_reported = 0;
	}

	//signal the user if the data has overflowed the array
	if (*index >= (MAXDIRS - 1)) {
		//but only signal the error once
		if (!error_already_reported) {
			error_already_reported = 1;
			report_data_overflow_error();
		}
	}

	//signal the user if the data has overflowed the array
	if (strlen (parent) + strlen (strall) + 1 > sizeof (file_spec))
		report_data_overflow_error();

	//search this subdirectory for subdirectories
	strcpy (file_spec, parent);
	appendSlash (file_spec);
	strcat (file_spec, strall);


#if 0 //old way
	hFound = FindFirstFile (file_spec, &fdFiles);
#else //new way that (ostensibly) limits search to directories
	hFound = FindFirstFileEx (file_spec,
							  FindExInfoStandard,
							  &fdFiles,
							  FindExSearchLimitToDirectories,
							  NULL,
							  0); //FIND_FIRST_EX_LARGE_FETCH - supposedly better performance, but not supported in VC2005
#endif
	bFound = ((hFound == INVALID_HANDLE_VALUE) ? FALSE : TRUE);
	while (bFound) {
		//do all subdirectories (except dot and double dot)
		if (ISSUBDIR (fdFiles)
				&& strcmp (FILENAME (fdFiles), ".") != 0
				&& strcmp (FILENAME (fdFiles), "..") != 0) {

			//found a subdirectory, so save it
			int iSize = strlen (parent) + strlen (FILENAME (fdFiles)) + strlen (lpSlash) + 1;
			if ((tree[*index] = (char*) malloc (iSize)) == NULL) {
				perror ("malloc");
				exit (EXIT_FAILURE);
			}

			strcpy (tree[*index], parent);
			appendSlash (tree[*index]);
			strcat (tree[*index], FILENAME (fdFiles));

			if (FileIoDebug)
				fprintf (stderr, "getDirTree: tree[%d] = [%s]\n", *index, tree[*index]);

			//work down one level
#if 1 //this works, but should be re-written
			getDirTree (tree[(*index)++], index);
#else //this does not work
			getDirTree (tree[(*index)], index);
			(*index)++;
#endif

			//!!! kludge: once the error was reported, end all further searches
			if (error_already_reported) {
				bFound = 0;
				break;
			}
		}

		//look for the next matching file
		bFound = FindNextFile (hFound, &fdFiles);
	}

	if (sortResults)
		qsort (tree, *index, sizeof (char*), _getDirTreeCompare);

	return tree;
}

///////////////////////////////////////////////////////////////////////////
void report_data_overflow_error (void)
{
	fprintf (stderr, "warning: data has overflowed the array\n");
	abort();
}

///////////////////////////////////////////////////////////////////////////
char *printDouble (double value, int precision)
{
	int index;
	//NOTE: this uses a static buffer so you can
	//only call it once before using the results
	static char buf[BUFSZ / 2];

	//convert value to a real and strip any trailing zeros (after the decimal)
	if (precision >= 0)
		sprintf (buf, "%.*f", precision, value);
	else
		sprintf (buf, "%f", value);
	index = strlen (buf) - 1;
	while (buf[index] == '0')
		buf[index--] = 0;

	//if the number end in a decimal point, remove it
	if (buf[index] == '.')
		buf[index--] = 0;

	return buf;
}

///////////////////////////////////////////////////////////////////////////
double getNewDouble (const char *str, double value)
{
	static char buf[BUFSZ / 2];

	fprintf (stderr, "%s [%s]: ", str, printDouble (value));
	fgets (buf, BUFSZ - 1, stdin);

	//user hit return to accept default value
	if (*buf == '\n')
		return value;

	//user enters desired value
	else if (isdigit (*buf) || *buf == '.' || *buf == '-')
		return atof (buf);

	//any other input, exit
	else
		exit (0);

	return 0.; //keep compiler happy
}

///////////////////////////////////////////////////////////////////////////
//print an unsigned long with commas separating
//the one's, thousand's and million's
char *insertCommas (unsigned long bytes)
{
	int i, len;
	unsigned long tmp;
	static char str0[15];
	char str1[15];
	static char *blankPad = "    ";
	static char *zeroPad  = "000,";
	BOOL inNumber = 0;

/*TODO
	//trillion's
	if ((tmp = (bytes / 1000000000000L)) > 0)
		{
		sprintf (str0, "%3lu,", tmp);
		bytes -= tmp * 1000000000000L;
		inNumber = 1;
		}
	else
		strcpy (str0, blankPad);
*/
	//billion's
	if ((tmp = (bytes / 1000000000L)) > 0)
		{
		sprintf (str0, "%3lu,", tmp);
		bytes -= tmp * 1000000000L;
		inNumber = 1;
		}
	else
		{
		if (inNumber)
			strcat (str0, zeroPad);
		else
			strcat (str0, blankPad);
		}

	//million's
	if ((tmp = (bytes / 1000000L)) > 0)
		{
		sprintf (str1, "%03lu,", tmp);
		strcat (str0, str1);
		bytes -= tmp * 1000000L;
		inNumber = 1;
		}
	else
		{
		if (inNumber)
			strcat (str0, zeroPad);
		else
			strcat (str0, blankPad);
		}

	//thousand's
	if ((tmp = (bytes / 1000L)) > 0)
		{
		sprintf (str1, "%03lu,", tmp);
		strcat (str0, str1);
		bytes -= tmp * 1000L;
		inNumber = 1;
		}
	else
		{
		if (inNumber)
			strcat (str0, zeroPad);
		else
			strcat (str0, blankPad);
		}

	//one's
	sprintf (str1, "%03lu", bytes);
	strcat (str0, str1);

	//remove leading 0's
	len = strlen (str0);
	for (i=0; i<len; i++)
		{
		//leave the last 0 if it's the only one
		if (i == (len - 1))
			break;

		//remove leading 0's and commas
		if (str0[i] == '0' || str0[i] == ',')
			continue;

		//and stop when we hit a non-zero digit
		else if (isdigit (str0[i]))
			break;
		}

	return str0 + i;
}

///////////////////////////////////////////////////////////////////////////
//print a double with commas separating the one's, thousand's, million's, etc.
//kilobyte, megabyte, gigabyte, terabyte, petabyte, exabyte
char *insertCommas (double bytes)
{
	static char str0[32];
	char str1[32];
	char comma = ',';
	int len = sprintf (str0, "%.0f", bytes);
	int shift;

	int chars = 18; //18 digits = exabytes
	while (chars > 0) {
		if (len > chars) {
			shift = len - chars;
			strcpy (str1, str0);
			strcpy (str1 + shift + 1, str0 + shift);
			str1[shift] = comma;
			strcpy (str0, str1);
			len++;
		}
		chars -= 3;
	}

	return str0;
}

///////////////////////////////////////////////////////////////////////////////
double unitSuffixScale (char *str)
{
//	unsigned long ulong = labs (atol (str));
	double value = atof (str);

	int lastChar = toupper (str[strlen (str) - 1]); //bug - doesn't handle empty string

	if (lastChar == 'K')
		value *= 1024;

	else if (lastChar == 'M')
		value *= 1024 * 1024;

	else if (lastChar == 'G')
		value *= 1024 * 1024 * 1024;

	return value;
}

////////////////////////////////////////////////////////////////////////////
//original from http://stackoverflow.com/questions/1566645/filetime-to-int64
ULONGLONG fileTimeToUint64 (const FILETIME fileTime)
{
	ULARGE_INTEGER uLarge;
	uLarge.LowPart = fileTime.dwLowDateTime;
	uLarge.HighPart = fileTime.dwHighDateTime;

	return uLarge.QuadPart;
}

///////////////////////////////////////////////////////////////////////////
double uint64todouble (ULARGE_INTEGER uint64)
{
	const double shift = pow (2., 32.);
	double high = (double) uint64.HighPart;
	double low = (double) uint64.LowPart;
	double value = high * shift + low;

	return value;
}

///////////////////////////////////////////////////////////////////////////
double int64todouble (LARGE_INTEGER int64)
{
	const double shift = pow (2., 32.);
	double high = (double) int64.HighPart;
	double low = (double) int64.LowPart;
	double value = high * shift + low;

	return value;
}

///////////////////////////////////////////////////////////////////////////
double getFileSize (FINDFILEDATA *pfdFiles)
{
	ULARGE_INTEGER ularge;

	ularge.u.LowPart = pfdFiles->nFileSizeLow;
	ularge.u.HighPart = pfdFiles->nFileSizeHigh;
	double fileSize = uint64todouble (ularge);

	return fileSize;
}

////////////////////////////////////////////////////////////////////////////
BOOL dirExists (const char *filename)
{
//#ifndef INVALID_FILE_ATTRIBUTES
//# define INVALID_FILE_ATTRIBUTES ((DWORD)-1) //should be defined by windows
//#endif

	DWORD attributes = GetFileAttributes (filename);

	if (attributes == INVALID_FILE_ATTRIBUTES) {
		int error = GetLastError ();

		if (error == ERROR_FILE_NOT_FOUND ||
			error == ERROR_PATH_NOT_FOUND ||
			error == ERROR_INVALID_NAME)
			return 0;
	}

	if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		return 1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////
//this version works for files and directories
BOOL fileExists (const char *filename)
{
//#ifndef INVALID_FILE_ATTRIBUTES
//# define INVALID_FILE_ATTRIBUTES ((DWORD)-1) //should be defined by windows
//#endif

	DWORD attributes = GetFileAttributes (filename);

	if (attributes == INVALID_FILE_ATTRIBUTES) {
		int error = GetLastError ();

		if (error == ERROR_FILE_NOT_FOUND ||
			error == ERROR_PATH_NOT_FOUND ||
			error == ERROR_INVALID_NAME)
			return 0;
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////
//this version works for files but not for directories
char *fileExists1 (const char *filename)
{
	HFILE hFile;
	static OFSTRUCT ofSrc;

	hFile = OpenFile (filename, &ofSrc, OF_EXIST);
	if (hFile == HFILE_ERROR) {
		int error = GetLastError ();
		return NULL;

	} else
		return ofSrc.szPathName; //not thread safe: returning pointer to static struct
}

////////////////////////////////////////////////////////////////////////////
time_t fileTime (const char *filename)
{
	DWORD dwFlagsAndAttributes = 0; //FILE_FLAG_BACKUP_SEMANTICS; //necessary for directories?

	HANDLE hFile = CreateFile (filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		int error = GetLastError ();
		return 0;
	}

	FILETIME creationTime;
	FILETIME lastWriteTime;
	if (!GetFileTime (hFile, &creationTime, NULL, &lastWriteTime)) {
		int error = GetLastError ();
		return 0;
	}

	FILETIME ftLocalTime; //Contains a 64-bit value representing the number of 100-nanosecond intervals since 01/01/1601 (UTC)
	SYSTEMTIME stSystemTime;

	if (!FileTimeToLocalFileTime (&lastWriteTime, &ftLocalTime)) {
		int error = GetLastError ();
//		printf ("FileTimeToLocalFileTime() failed: %s", formatLastError (GetLastError ()));
		return 0;
	}

	if (!FileTimeToSystemTime (&ftLocalTime, &stSystemTime)) {
		int error = GetLastError ();
//		printf ("FileTimeToSystemTime() failed: %s", formatLastError (GetLastError ()));
		return 0;
	}

	return timeFromSystemTime (&stSystemTime);
}

////////////////////////////////////////////////////////////////////////////
//original from http://www.codeguru.com/forum/archive/index.php/t-139510.html
time_t timeFromSystemTime (const SYSTEMTIME *pTime)
{
	struct tm tm;
	memset (&tm, 0, sizeof (tm));

	tm.tm_year = pTime->wYear - 1900;
	tm.tm_mon = pTime->wMonth - 1;
	tm.tm_mday = pTime->wDay;

	tm.tm_hour = pTime->wHour;
	tm.tm_min = pTime->wMinute;
	tm.tm_sec = pTime->wSecond;

	tm.tm_isdst = -1;

	return mktime (&tm);
}

////////////////////////////////////////////////////////////////////////////
//original from http://stackoverflow.com/questions/6161776/convert-windows-filetime-to-second-in-unix-linux
time_t windowsTickToUnixSeconds(long long windowsTicks)
{
	const long long WINDOWS_TICK = 10000000;
	const long long SEC_TO_UNIX_EPOCH = 11644473600LL;

	long long secs = (windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
	time_t t = (time_t) secs;
	if (secs != (long long) t)	//checks for truncation/overflow/underflow
		return (time_t) -1;		//value not representable as a POSIX time

	return t;
}

////////////////////////////////////////////////////////////////////////////
//Note the non 64-bit version, GetTickCount(), wraps after 49.7 days
ULONGLONG myGetTickCount64 ()
{
	typedef ULONGLONG (WINAPI *FPTR_GetTickCount64) (void);

	ULONGLONG ullCount = 0; //this will likely cause problems downstream

	FPTR_GetTickCount64 pGetTickCount64 = (FPTR_GetTickCount64) GetProcAddress (GetModuleHandle ("kernel32.dll"), "GetTickCount64");
	if (!pGetTickCount64) {
		fprintf (stderr, "Error: GetProcAddress(GetTickCount64) failed: %s", formatLastError (GetLastError ()));

	} else {
		ullCount = (*pGetTickCount64) ();
	}

	return ullCount;
}

////////////////////////////////////////////////////////////////////////////
int compareFileSize (double bytes1, double bytes2)
{
	if (bytes1 > bytes2)
		return 1;
	else if (bytes1 < bytes2)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////
int compareFileExten (char *filename1, char *filename2)
{
//TODO - only look for extension on last path element

	//find the extension, if any
	char *ext1 = strrchr (filename1, '.');
	char *ext2 = strrchr (filename2, '.');

	if (ext1 == NULL)
		ext1 = "";
	if (ext2 == NULL)
		ext2 = "";

	return strcasecmp (ext1, ext2);
}

////////////////////////////////////////////////////////////////////////////
//NOTE this modifies user's buffer
char *appendSlash (char *filename)
{
	//append slash to end, if not already there (buffer must be large enough)
	if (filename[strlen (filename) - 1] != '\\')
		strcat (filename, lpSlash);
	return filename;
}

////////////////////////////////////////////////////////////////////////////
//NOTE this modifies user's buffer
char *swapSlashes (char *lpFilename)//, BOOL bUseForwardSlashes);
{
	int ii, jj;

	int len = strlen (lpFilename);
	for (ii = 0; ii < len; ii++) {
		if (lpFilename[ii] == '/')
			lpFilename[ii] = '\\';
	}

	//collapse multiple backslashes
	for (ii = 0, jj = 0; ii < len; ii++) {
		if (lpFilename[ii] == '\\' && ii < len - 1 && lpFilename[ii + 1] == '\\')
			ii++; //skip redundant slash
		lpFilename[jj] = lpFilename[ii];
		jj++;
	}
	lpFilename[jj] = 0;

	return lpFilename;
}

////////////////////////////////////////////////////////////////////////////
//quote file if necessary (i.e., it has spaces or other invalid characters in the name)
const char *quoteFile (const char* filename)
{
	int needsQuotes = 0;

	if (strchr (filename, ' ') != 0) {
		needsQuotes = 1;

	} else if (strchr (filename, '+') != 0) {
		needsQuotes = 1;

	} else if (strchr (filename, '=') != 0) {
		needsQuotes = 1;
	}

	if (!needsQuotes)
		return filename;

	char* quoted = new char [strlen (filename) + 3]; // memory leak
	strcpy (quoted, "\"");
	strcat (quoted, filename);
	strcat (quoted, "\"");

	return quoted;
}

////////////////////////////////////////////////////////////////////////////
/*
<<< TURRIS::TURRIS$DUA18:[NOTES$LIBRARY]VAXC.NOTE;2 >>>
-< VAX C Notes >-
================================================================================
Note 2360.11 - what's a portable replacement for STR$MATCH_WILD - 11 of 13
BOLT::MINOW "Pere Ubu is coming soon, are you ready" 69 lines  27-SEP-1989 14:24
-< Here's the match routine >-
--------------------------------------------------------------------------------
Here is a routine that does RT11-style wildcard ('*' matches anything, '?'
matches a single character).  I can provide you with regular expression
matchin, too.)
--------------------------------------------------------------------------------
*/

#define FAIL_AT_END_OF_PATTERN 1
#define EOS '\0'

int matchtest (
	const char *name,	//what to look for
	const char *pattern	//may have wildcard
	)
//Recursive routine to match "name" against "pattern".
//Returns TRUE if successful, FALSE if failure.
//
//pattern follows Dec filename wildcard conventions:  '*' matches any
//string (even null), '?' matches any single (non-null) byte.
//(extended to accept '%' as well as '?' for single byte wildcard)
{
	register char pattbyte;

	for (;;) {
#if 0
		fprintf(stderr, "(%s) (%s), namebyte = '%c', pattbyte = '%c'\n",
							name, pattern, *name, *pattern);
#endif

#if FAIL_AT_END_OF_PATTERN
		//First check for one-byte pattern "*" -- this must succeed
		if ((pattbyte = *pattern++) == '*' && *pattern == EOS)
			return TRUE;
		//If not, then if both strings finish equally, it succeeds.
		if (*name == EOS && pattbyte == EOS)
			return TRUE;
#if 1 //fix bug that allowed extra trailing '?' to match (i.e., 'foo?' should not match 'foo')
		if ((*name != EOS) ^ (pattbyte != EOS))
			return FALSE;
#endif
#else
		//Assume that patterns are terminated -- implicitly --
		//by '*', allowing all left-matches to succeed.
		if ((pattbyte = *pattern++) == EOS
		 || (pattbyte == '*' && *pattern == EOS))
			return TRUE;
#endif
		//Not at end of the name string.
		switch (pattbyte) {
#if FAIL_AT_END_OF_PATTERN
			case EOS:			//End of pattern -> failure
				return FALSE;
#endif
		case '*':			//Wild card means "advance"
			do {
				if (matchtest (name, pattern))
					return TRUE;
			} while (*name++ != EOS);
			return FALSE;		//Did our best

		case '?':			//One byte joker
		case '%':			//One byte joker
			break;			//succeeds with this byte

		default:			//Others much match exactly
#if 0 //set to 1 for CASE_SENSITIVE
			if (*name != pattbyte)
#else
			if (toupper (*name) != toupper (pattbyte))
#endif //CASE_SENSITIVE
				return FALSE;
			break;
		}
		name++;			//Advance name byte
	}
}

///////////////////////////////////////////////////////////////////////////////
//remap special chars and expand tabs to spaces
//NOTE: this overwrites the users buffer
char *remapChars (char *line, int iBufferLength, int iFlags)
{
	char *line2 = new char [iBufferLength * 4]; //may need extra room
	int iLength = strlen (line);

	int iQuiteMode = 0;
	if (iFlags & RMC_QUIETMODE)
		iQuiteMode = 1;

#define TABSIZE 4
	int iTabSize = TABSIZE;
	if (iFlags & RMC_TABSIZEMASK)
		iTabSize = iFlags & RMC_TABSIZEMASK;

	if (iFlags & RMC_CARRIAGECONTROL) //remove trailing carriage control, if amy
		while (iLength > 0 && (line[iLength - 1] == 0x0A || line[iLength - 1] == 0x0D))
			line[--iLength] = 0;

	int jj = 0;
	for (int ii = 0; ii < iLength; ii++) {
		int c = line[ii] & 0xFF;
		switch (c) {
		case '\t':
			if (iFlags & RMC_TABS) {
				//fill buffer with spaces up to next tab stop
				int kk = iTabSize * ((jj / iTabSize) + 1);
				while (jj < kk)
					line2[jj++] = ' ';
			}
			else
				line2[jj++] = c;
			break;

		case '\f':
			if (iFlags & RMC_FORMFEEDS) {
				//write out form feeds as <FF>
				line2[jj++] = '<';
				line2[jj++] = 'F';
				line2[jj++] = 'F';
				line2[jj++] = '>';
			}
			else
				line2[jj++] = c;
			break;

		case '(':
		case ')':
		case '\\':
			if (iFlags & RMC_PARENS) {
				//escape these chars with a backslash
				line2[jj++] = '\\';
				line2[jj++] = c;
			}
			else
				line2[jj++] = c;
			break;

		case '%':
			if (iFlags & RMC_PERCENT) {
				//escape these chars with a percent (for printf)
				line2[jj++] = c;
				line2[jj++] = c;
			}
			else
				line2[jj++] = c;
			break;

		default:
//aaa - is this the complete list of unprintable chars??
			if ((iFlags & RMC_MULTINATIONAL) &&
					(c >= 0xA0 && c <= 0xFE))
				//replace these unprintable chars
				line2[jj++] = '?';
			else
				line2[jj++] = c;
			break;
		}
	}

	//terminate buffer and copy new line to source
	if (jj > iBufferLength - 1) {
		jj = iBufferLength - 1;
//TODO - this can silently truncate the buffer!
//		if (!iQuiteMode)
//			fprintf (stderr, "remapChars() buffer truncated at %d bytes\n", jj);
	}
	line2[jj] = 0;

	strcpy (line, line2);
	delete (line2);

	return line;
}

///////////////////////////////////////////////////////////////////////////////
//dup the string from 'start' for 'count' bytes (defaults to entire string)
//caller is responsible to delete returned buffer
char *strndup (const char *s0, int start, int count)
{
	int len = strlen (s0);

	if (start < 0) { //default case: dup entire string
		start = 0;
		count = len;
	}

	myAssert (start + count <= len);

	char *s1 = (char*) new char [count + 1];
	myAssert (s1);

	strncpy (s1, s0 + start, count);
	s1[count] = 0;

	return s1;
}

///////////////////////////////////////////////////////////////////////////////
//caller must delete memory
char *strlower (const char *str)
{
	if (str == 0)
		return (char*) str;

	char *lower = strndup (str);
	int len = strlen (lower);
	for (int ii = 0; ii < len; ii++)
		lower[ii] = tolower (lower[ii]);

	return lower;
}

///////////////////////////////////////////////////////////////////////////////
//caller must delete memory
char *strupper (const char *str)
{
	if (str == 0)
		return (char*) str;

	char *upper = strndup (str);
	int len = strlen (upper);
	for (int ii = 0; ii < len; ii++)
		upper[ii] = toupper (upper[ii]);

	return upper;
}

///////////////////////////////////////////////////////////////////////////////
int isHomeMachine ()
{
	char *logonserver = getenv ("LOGONSERVER");

	if (strcasecmp (&logonserver[2], "dope") == 0)
		return 1;
	if (strcasecmp (&logonserver[2], "oink") == 0)
		return 1;
	if (strcasecmp (&logonserver[2], "swine") == 0)
		return 1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int isProcessRunning (const char *exeName)
{
	PROCDATA procData[200];
	int numPids = getProcessData (exeName, procData);

	return numPids > 0;
}

///////////////////////////////////////////////////////////////////////////////
int getProcessData (const char *exeName, PROCDATA *procData)
{
	//original example from: http://www.extalia.com/forums/viewtopic.php?f=21&t=2269
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof (PROCESSENTRY32);

	HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

	//make second string that includes .exe extension
	char exeName2[MY_MAX_PATH];
	strcpy (exeName2, exeName);
	strcat (exeName2, ".exe");

	int numPids = 0;
	if (Process32First (hSnapshot, &pe32)) {
		do	{
			if (FileIoDebug)
				printf ("getProcessData: %d:\t\"%s\"\n", pe32.th32ProcessID, pe32.szExeFile);

			if (matchtest (pe32.szExeFile, exeName) || matchtest (pe32.szExeFile, exeName2)) {
				procData[numPids].pid = pe32.th32ProcessID;
				procData[numPids].name = strdup (pe32.szExeFile); //memory leak
				numPids++;
			}

		} while (Process32Next (hSnapshot, &pe32));
	}

	if (hSnapshot != INVALID_HANDLE_VALUE)
		CloseHandle (hSnapshot);

	return numPids;
}

///////////////////////////////////////////////////////////////////////////////
int enableProcessPrivilege (LPCTSTR privName)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	if (!OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		char *error = formatLastError (GetLastError ());
		fprintf (stderr, "Error: OpenProcessToken() failed: %s", error);
		return 0;
	}

	LookupPrivilegeValue (NULL, privName, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges (hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	if (GetLastError () != ERROR_SUCCESS) {
		char *error = formatLastError (GetLastError ());
		fprintf (stderr, "Error: AdjustTokenPrivileges() failed: %s", error);
		return 0;
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
//try to force OS to flush file so we get accurate size/date info
BOOL forceFileFlush (const char *fileSpec, FINDFILEDATA *fdFiles)
{
	BOOL status = FALSE;

	FILE *infile;
	if ((infile = fopen (fileSpec, "r")) == NULL) {
		fprintf (stderr, "\nfopen error (%d): ", errno);
		perror (fileSpec);

	} else {
		if (fseek (infile, 0L, SEEK_END) != 0) {
			fprintf (stderr, "\nfseek error (%d): ", errno);
			perror (fileSpec);

		} else if (FileIoDebug) {
			fprintf (stderr, "forceFileFlush() fileSpec=\"%s\", ftell=%ld\n", fileSpec, ftell (infile));
		}

		fclose (infile);
	}

	//replace file attribute data after forcing flush
	FINDFILEDATA f1;
	HANDLE hFound = FindFirstFile (fileSpec, &f1);
	if (hFound == INVALID_HANDLE_VALUE) {
		printf ("%s: FindFirstFile() failed: %s", fileSpec, formatLastError (GetLastError ()));

	} else {
		memcpy (fdFiles, &f1, sizeof (FINDFILEDATA));
		status = TRUE;
	}

	return status;
}

///////////////////////////////////////////////////////////////////////////////
//to launch the default app for a file, use command = "rundll32 url.dll,FileProtocolHandler"
//for this to work, there must be an app associated with the extension of the file
int launchCommand (const char *command, const char *args)
{
	char commandLine[MY_MAX_PATH];
	strcpy (commandLine, command);
	strcat (commandLine, " ");
	strcat (commandLine, args);

	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;
	memset (&startupInfo, 0, sizeof (STARTUPINFO));
	startupInfo.cb = sizeof (STARTUPINFO);
	startupInfo.wShowWindow = SW_SHOWNORMAL;
	BOOL status = CreateProcess (NULL, commandLine, NULL, NULL, FALSE, NULL, NULL, NULL, &startupInfo, &processInfo);
	int error = (status ? 0 : GetLastError ());

	return error;
}

///////////////////////////////////////////////////////////////////////////////
int launchPlot (const char *plotFile)
{
	//look for plot.exe:
	//- the current directory
	//- the directory that that the binary is run from
	//- fall back to well-known home
	char *plotExe = "plot.exe";
	char plotPath[MY_MAX_PATH];

	strcpy (plotPath, plotExe);

	if (!fileExists (plotPath)) {
		char exePath[MY_MAX_PATH];
		GetModuleFileName (NULL, exePath, MY_MAX_PATH - 1);
		strcpy (plotPath, exePath);
		strcat (plotPath, "/");
		strcat (plotPath, plotExe);
	}

	if (!fileExists (plotPath)) {
		strcpy (plotPath, "C:/users/bin/plot/release/plot.exe");
	}

	if (!fileExists (plotPath)) {
		fprintf (stderr, "Error in launchPlot: '%s' not found\n", plotExe);
		return 0;
	}

	char command[MY_MAX_PATH];
	strcpy (command, plotPath);
	strcat (command, " ");
	strcat (command, plotFile);

	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;
	memset (&startupInfo, 0, sizeof (STARTUPINFO));
	startupInfo.cb = sizeof (STARTUPINFO);
	startupInfo.wShowWindow = SW_SHOWNORMAL;
	BOOL status = CreateProcess (NULL, command, NULL, NULL, FALSE, NULL, NULL, NULL, &startupInfo, &processInfo);
	if (!status) {
		int error = GetLastError ();
		int breakHere = 1; //debug
	}

	return status;
}

////////////////////////////////////////////////////////////////////////////
//can be used to filter clearcase versions that are labels
BOOL stringIsOnlyDigits (const char *filename)
{
	if (!filename)
		return FALSE;

	int len = strlen (filename);
	for (int ii = 0; ii < len; ii++) {
		if (!isdigit (filename[ii]))
			return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
BOOL isRedirectedHandle (DWORD nStdHandle)
{
	HANDLE handle = GetStdHandle (nStdHandle);
	CONSOLE_SCREEN_BUFFER_INFO info;

	return !GetConsoleScreenBufferInfo (handle, &info);
}

///////////////////////////////////////////////////////////////////////////////
int getWindowWidth (int defaultWidth)
{
	static int windowWidth = -1;

	if (windowWidth < 0) {
		HANDLE hOutput = GetStdHandle (STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO info;

		if (GetConsoleScreenBufferInfo (hOutput, &info)) {
			windowWidth = info.dwMaximumWindowSize.X; //maximum width of window when maximized

		} else {
			fprintf (stderr, "getWindowWidth: GetConsoleScreenBufferInfo failed: %s", formatLastError (GetLastError ()));
			windowWidth = defaultWidth;
		}
	}

	return windowWidth;
}

///////////////////////////////////////////////////////////////////////////////
//checks to see if we should read input from stdin
//returns name if yes, NULL otherwise
char *usingStdin (const char *filename)
{
	if (strcmp (filename, "stdin") == 0)
		return (char*) filename;
	else
		return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//case-insensitive version of strstr
char *strstri (const char *str, const char *sub)
{
	char *strl = strlwr (strdup (str));
	char *subl = strlwr (strdup (sub));

	char *p0 = strstr (strl, subl);
	if (p0 == 0) {
		free (strl);
		free (subl);
		return p0;
	}

	int offset = p0 - strl;
	char *p1 = (char*) str + offset;
	free (strl);
	free (subl);

	return p1;
}

///////////////////////////////////////////////////////////////////////////////
//originally based on AlphanumComparator.java
//note this has memory leak
static const char *_getChunk (const char *s, int slength, int marker)
{
	char chunk[MY_MAX_PATH];
	int chunkIndex = 0;

	char c = s[marker];
	chunk[chunkIndex++] = c;
	marker++;

	if (isdigit(c)) {
		while (marker < slength) {
			c = s[marker];
			if (!isdigit (c))
				break;
			chunk[chunkIndex++] = c;
			marker++;
		}

	} else {
		while (marker < slength) {
			c = s[marker];
			if (isdigit (c))
				break;
			chunk[chunkIndex++] = c;
			marker++;
		}
	}

	chunk[chunkIndex++] = 0;
	return strndup (chunk); // memory leak!
}

///////////////////////////////////////////////////////////////////////////////
//originally based on AlphanumComparator.java
//note _getChunk has memory leak
int alphanumComp (const char *s1, const char *s2)
{
	int thisMarker = 0;
	int thatMarker = 0;
	int s1Length = strlen (s1);
	int s2Length = strlen (s2);

	while (thisMarker < s1Length && thatMarker < s2Length) {
		const char *thisChunk = _getChunk (s1, s1Length, thisMarker);
		thisMarker += strlen (thisChunk);

		const char *thatChunk = _getChunk (s2, s2Length, thatMarker);
		thatMarker += strlen (thatChunk);

		// If both chunks contain numeric characters, sort them numerically
		int result = 0;
		if (isdigit (thisChunk[0]) && isdigit (thatChunk[0])) {
			// Simple chunk comparison by length.
			int thisChunkLength = strlen (thisChunk);
			result = thisChunkLength - strlen (thatChunk);
			// If equal, the first different number counts
			if (result == 0) {
				for (int i = 0; i < thisChunkLength; i++) {
					result = thisChunk[i] - thatChunk[i];
					if (result != 0) {
						return result;
					}
				}
			}

		} else {
			result = strcasecmp (thisChunk, thatChunk); //sort case-insensitive
		}

		if (result != 0)
			return result;
	}

	return s1Length - s2Length;
}

/////////////////////////////////////////////////////////////////////////////
void waitForDebugger ()
{
	fprintf (stderr, "waiting for debugger to attach");
	while (1) {
		fprintf (stderr, ".");
		Sleep (1000); //to debug, set breakpoint here
	}
	int setNextStatementHere = 1;
}
