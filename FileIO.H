//FileIO.H

#ifndef _FILEIO_H_
#define _FILEIO_H_

#include <windows.h>
#include <tlhelp32.h>
#include <time.h>

#if defined (_MSC_VER)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#define strlwr _strlwr
#elif defined (__GNUG__)
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wconversion-null"
#endif

#define FINDFILEDATA WIN32_FIND_DATA
#define FILENAME(file)		((file).cFileName)
#define FILENAMEP(file)		((file)->cFileName)
//#define FILESIZE(file)		((file).nFileSizeLow + ((file).nFileSizeHigh * MAXDWORD))
//#define FILESIZEP(file)		((file)->nFileSizeLow + ((file)->nFileSizeHigh * MAXDWORD))
//attributes
#define ISREADONLY(file)	((file).dwFileAttributes & FILE_ATTRIBUTE_READONLY)
#define ISREADONLYP(file)	((file)->dwFileAttributes & FILE_ATTRIBUTE_READONLY)
#define ISSUBDIR(file)		((file).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#define ISSUBDIRP(file)		((file)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#define ISJUNCTION(file)	((file).dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
#define ISJUNCTIONP(file)	((file)->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
#define ISCOMPRESSED(file)	((file).dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
#define ISCOMPRESSEDP(file)	((file)->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
#define ISSYSTEM(file)		((file).dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
#define ISSYSTEMP(file)		((file)->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
#define ISHIDDEN(file)		((file).dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
#define ISHIDDENP(file)		((file)->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
#define ISARCHIVE(file)		((file).dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
#define ISARCHIVEP(file)	((file)->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
//#endif //OS

//originally added to support junctions (MS doc claims this should be in winbase.h)
typedef struct _FILE_STANDARD_INFO {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  DWORD         NumberOfLinks;
  BOOLEAN       DeletePending;
  BOOLEAN       Directory;
} FILE_STANDARD_INFO, *PFILE_STANDARD_INFO;

//originally added to support junctions (MS doc claims this should be in winbase.h)
typedef struct _FILE_STANDARD_INFORMATION {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG         NumberOfLinks;
  BOOLEAN       DeletePending;
  BOOLEAN       Directory;
} FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

#define Bool BOOL
#define True TRUE
#define False FALSE
#define null NULL
#define ALLFILES "*"

extern int FileIoDebug;

#if !defined (_MAX_PATH)
#define _MAX_PATH 256
#endif
#define MY_MAX_PATH (2 * _MAX_PATH) //support for long filenames

struct dirnode {
	char    path[MY_MAX_PATH];         //path only
	char    spec[MY_MAX_PATH];         //path plus filename
	int     firstpass;
	struct  dirnode *next;
	FINDFILEDATA *lpfdFiles;
	};

int matchWildcardList (const char *lpFilename, const char *lpWildcardList, int extendedFileMatching = 1);
int matchStringInList (const char *lpString, const char *lpList, int ignoreCase = 1);
char *formatLastError (int iError);
#define MFF_RECURSE_SUBDIRS		1
#define MFF_RETURN_SUBDIRS		2
#define MFF_EXCLUDE_NONSOURCE	4
#define MFF_ISS_BINARY			8
#define MFF_ISS_TEXT			16
#define MFF_ISS_BOTH			(MFF_ISS_BINARY	| MFF_ISS_TEXT)
int isSourceFile (FINDFILEDATA *pfdFiles, char *lpFileSpec = NULL, int iFlags = MFF_ISS_BOTH);
char *myFindFile (const char *path, const char *file, FINDFILEDATA **fdUser, int iFlags = 0, int iAddlSubdirLevels = 999);
char *myFileParse (const char *pInFullSpec, char *pOutPathOnly, char *pOutNameOnly);
#if defined (_MSC_VER)
wchar_t *toWide (const char *ascii);
char *fromWide (const wchar_t *wide, int stripTrailingCarriageControl = 0);
char *myfgets (char *line, int n, FILE *stream, int bUnicodeFile);
#endif
FILE *myfreopen (const char *path, const char *mode, FILE *stream);
char **getDirTree (const char *parent, int *index, int sortResults = 1);
char *formatDirHeader (const char *path);
char *formatDirTrailer (int iNumFiles, double dNumBytes, int iDrive);
char *getPath (const char *fileSpec); // returns path from complete fileSpec
#define FFI_DEFAULT      0 //none
#define FFI_SHOWFULLPATH 1
#define FFI_ATTRIBUTES   2
char *formatFileinfo (FINDFILEDATA *lpfdFiles, int iFlags = 0, const char *szPathOnly = "");
int matchtest (const char *name, const char *pattern);
#define RMC_TABSIZEMASK     0x00ff
#define RMC_QUIETMODE       0x0100
#define RMC_TABS            0x0200
#define RMC_FORMFEEDS       0x0400
#define RMC_PARENS          0x0800
#define RMC_CARRIAGECONTROL 0x1000 //strip trailing carriage control
#define RMC_MULTINATIONAL   0x2000
#define RMC_PERCENT         0x4000
char *remapChars (char *line, int iBufferLength, int iFlags = 0);

void report_data_overflow_error (void);
char *printDouble (double value, int precision = -1);
double getNewDouble (const char *str, double value);
char *insertCommas (unsigned long bytes);
char *insertCommas (double bytes);
double unitSuffixScale (char *str);
ULONGLONG fileTimeToUint64 (const FILETIME fileTime);
double uint64todouble (ULARGE_INTEGER uint64);
double int64todouble (LARGE_INTEGER int64);
double getFileSize (FINDFILEDATA *fdFiles);
BOOL dirExists (const char *filename);
BOOL fileExists (const char *filename);
char *fileExists1 (const char *filename);
time_t fileTime (const char *filename);
time_t timeFromSystemTime (const SYSTEMTIME *pTime);
time_t windowsTickToUnixSeconds (long long windowsTicks);
ULONGLONG myGetTickCount64 ();
int compareFileSize (double bytes1, double bytes2);
int compareFileExten (char *filename1, char *filename2);
char *appendSlash (char *lpFilename);
char *swapSlashes (char *lpFilename);//, BOOL bUseForwardSlashes);
const char *quoteFile (const char* filename);
char *strndup (const char *s0, int start = -1, int count = 0);
char *strlower (const char *str);
char *strupper (const char *str);
int isHomeMachine ();
BOOL forceFileFlush (const char *fileSpec, FINDFILEDATA *fdFiles);
int launchCommand (const char *command, const char *args);
int launchPlot (const char *plotFile);
BOOL stringIsOnlyDigits (const char *filename);
BOOL isRedirectedHandle (DWORD nStdHandle);
int getWindowWidth (int defaultWidth);
char *usingStdin (const char *filename);
char *strstri (const char *str, const char *sub);
int alphanumComp (const char *s1, const char *s2);
void waitForDebugger ();

typedef struct tagPROCDATA {
	int pid;
	char *name;
} PROCDATA;

int isProcessRunning (const char *exeName);
int getProcessData (const char *exeName, PROCDATA *procData);
int enableProcessPrivilege (LPCTSTR privName);

#if defined (_MSC_VER)
//copied from assert.h
#ifdef  __cplusplus
extern "C" {
#endif
_CRTIMP void __cdecl _assert(const void *, const void *, unsigned);
#ifdef  __cplusplus
}
#endif
#define myAssert(exp) (void)( (exp) || (_assert(#exp, __FILE__, __LINE__), 0) )
#elif defined (__GNUG__)
//#define myAssert(exp) (void)( (exp) || (assert(#exp, __FILE__, __LINE__), 0) )
#define myAssert(exp) assert(#exp)
#endif

#endif //_FILEIO_H_
