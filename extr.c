// extr.C

// STL reference at http://www.halpernwightsoftware.com/stdlib-scratch/quickref.html
// MFC reference at http://www.cppdoc.com/example/mfc/classdoc/MFC/CString.html
// Case-insensitive Comparison of strings at http://www.devx.com/tips/Tip/13633
// STL examples at http://www.codeproject.com/vcpp/stl/ostringstream.asp


#define VERS_STR "(V5.44)"
#define APP_NAME "Extr"

#pragma warning (disable:4786)
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <strstream>
#include <sstream>
#include <cassert>
#include "fileio.h"

using namespace std;

#define BUF_SIZE 1024
#define MAX_FILE_SIZE (500 * 1000 * 1000)
#define MIN_THUMB_SIZE 200
#define DEFAULT_COLUMNS 2
#define DEFAULT_IMAGE_SIZE 300
#define DEFAULT_TIMER_SECS 5
#define DEFAULT_TITLE "extr"

#define isOdd(x) ((x) & 1)

//list is not sorted
typedef list<string, allocator<string> > StringList;
//set maintains sorted and uniq'ed list
typedef set<string, less<string>, allocator<string> > StringSet;
//vector allows us to define case-insensitive sort
typedef vector<string, allocator<string> > StringVector;

const string FileUrlHdr2 = "file://";
const string FileUrlHdr3 = "file:///";
const string HtmlExten = ".htm";
const string NewLine = "\n";
const string Space = " ";
const string Tab = "\t";

const char Dot = '.';
const char ForwardSlash = '/';
const char BackSlash = '\\';

const int outMode = ios::trunc | ios::out;
const int inMode = ios::binary | ios::in;

int proc01 ();
int proc02 (const string &wildName);
int proc03 (const string &file, int columns, const string &title);
int proc03L (const string &file, int columns, const string &title, const string &tail, bool useShortLink);
int proc04a (const string &format, int i0, int i1, int j0, int j1, int iStep, int jStep);
int proc04b (const string &format, const string &baseFilename, int iNumPerFile,
			 int iStart, int iCount, int jStart, int jCount, int jStep);

int proc05 (const string &file);
int proc06 (const string &wildName);
int proc07 ();
int proc09 (const string &file, int timer);
int proc10 (const string &file);
int proc11 (const string &file);
int proc12a (const string &file);
int proc12b (const string &file);
int proc12c (int timeoutSecs);
int proc13 (const string &file);
int proc14 (const string &file);

int deleteFiles (const string &filePath, const string &wildName);
int regDeleteKey (const string &lpKey, const string &lpSubkey, bool ignoreNotExistError = TRUE);
int regDeleteValue (const string &lpKey, const string &lpSubkey, const string &lpValueName,
					bool ignoreNotExistError = TRUE);
int getPartialCksum (string fileUrl);
bool isImage (string fileName);
int getImageSize (string fileUrl, int *width, int *height, bool test = FALSE);
int extractImageSize (const char *buf, int *width, int *height);
void addImage (string alt, string img, int imageWidth, int imageHeight,
			   int maxImageWidth, int maxImageHeight, int align, bool clipImageToMaxSize);
string getCurrentDirectory (bool useForwardSlashes = TRUE);
string swapSlashes (string &str, bool useForwardSlashes = TRUE);
void addLinks ();
string fileName (const string &path);
string asUrl (const string &file);
string asWildName (const string &file);
int readFile (const string &filePath, char **buffer);
int getWindowSize (bool getWidthOrHeight);
string formatElapsedTime (DWORD millisecs);

typedef struct tagWINDATA {
	LPSTR winClass;
	LPSTR winName;
	HWND hWnd;
} WINDATA, *PWINDATA;

BOOL CALLBACK myEnumWindowsCallbackProc (HWND hWnd1, LPARAM lParam);
int waitForWindow (LPSTR windowName, LPSTR windowClass, DWORD timeoutMillis);


/////////////////////////////////////////////////////////////////////////////
class lessViaInt {
public:
	bool operator () (const string& s1, const string& s2) {
		int i1 = atoi (s1.c_str ());
		int i2 = atoi (s2.c_str ());
		return i1 < i2;
	}
};

/////////////////////////////////////////////////////////////////////////////
class lessNoCase {
public:
	bool operator () (const string& s1, const string& s2) {

		int stat = _stricmp (s1.c_str (), s2.c_str ());
//debug
//		cout << s1.c_str () << "\t" << s2.c_str () << "\t" << stat << endl;
		return stat < 0;
	}
};

/////////////////////////////////////////////////////////////////////////////
class lessViaLength {
public:
	bool operator () (const string& s1, const string& s2) {
		//sort longer strings first
		int i1 = s1.length ();
		int i2 = s2.length ();
		if (i1 != i2) {
			return i2 < i1;
		}

		//then by case-insensitive alpha 
		int stat = _stricmp (s1.c_str (), s2.c_str ());
		return stat < 0;
	}
};

/////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
//	cerr << APP_NAME << Space << VERS_STR << endl;

//	waitForDebugger ();

	if (argc == 3) {
		//got /norm <file> (normalize links - eg., www10.msn.com -> www.msn.com)
		if (strcmp (argv[1], "/norm") == 0) {
			return proc11 (argv[2]);
		}

		//got /fcl <file> (find corrupted lines)
		if (strcmp (argv[1], "/fcl") == 0) {
			return proc14 (argv[2]);
		}

		//got /fix <file> (fix lines in file by wrapping on redirection)
		if (strcmp (argv[1], "/fix") == 0) {
			return proc10 (argv[2]);
		}

		//got /special <file> (special handling to cleanup bad history data)
		if (strcmp (argv[1], "/special") == 0) {
			return proc13 (argv[2]);
		}
	}

	if (argc == 2) {
		//got /c (cleanup)
		if (strcmp (argv[1], "/c") == 0)
			return proc07 ();

		//got /w (wipe disk)
		if (strcmp (argv[1], "/w") == 0)
			return proc01 ();

		//got /t (test code)
		if (strcmp (argv[1], "/t") == 0) {
			printf ("browser display area = %d x %d\n", getWindowSize (TRUE), getWindowSize (FALSE));
			return 0;
		}
	}

	if (argc >= 2 && argc <= 3) {
		//got /wff [<timeout in seconds>] (wait for FireFox)
		if (strcmp (argv[1], "/wff") == 0) {
			int timeoutSecs = 0;
			if (argc >= 3) {
				timeoutSecs = atoi (argv[2]);
				if (timeoutSecs < 0)
					timeoutSecs = 0;
			}

			return proc12c (timeoutSecs);
		}
	}

	if (argc == 3) {
		//got /bn (basename)
		if (strcmp (argv[1], "/bn") == 0)
			return proc12a (argv[2]);

		//got /split
		if (strcmp (argv[1], "/split") == 0)
			return proc12b (argv[2]);

		//got /dup (test for duplicates via file size)
		if (strcmp (argv[1], "/dup") == 0)
			return proc06 (argv[2]);

		//got /t (test code - find location of file sizes in images)
		if (strcmp (argv[1], "/test") == 0)
			return proc05 (argv[2]);
	}

	if (argc >= 3 && argc <= 5) {
		//got /html <file> [<columns>] [<title>] (output html images from file list given)
		if (strcmp (argv[1], "/html") == 0) {
			int columns = DEFAULT_COLUMNS;
			if (argc >= 4) {
				columns = atoi (argv[3]);
				if (columns == 0)
					columns = DEFAULT_COLUMNS;
			}

			string title = DEFAULT_TITLE;
			if (argc >= 5)
				title = argv[4];

			return proc03 (argv[2], columns, title);
		}
	}

	if (argc >= 3 && argc <= 7) {
		//got /list <file> [/s | /nos] [<columns>] [<title>] [<tail>] (output html list [links] from file list given)
		if (strcmp (argv[1], "/list") == 0) {
			bool useShortLink = FALSE;
			if (argc >= 4 && strcmp (argv[3], "/s") == 0)
				useShortLink = TRUE;
			if (argc >= 4 && strcmp (argv[3], "/nos") == 0)
				useShortLink = FALSE;

			int columns = DEFAULT_COLUMNS;
			if (argc >= 5) {
				columns = atoi (argv[4]);
				if (columns == 0)
					columns = DEFAULT_COLUMNS;
			}

			string title = DEFAULT_TITLE;
			if (argc >= 6)
				title = argv[5];

			string tail;
			if (argc >= 7)
				tail = argv[6];

			return proc03L (argv[2], columns, title, tail, useShortLink);
		}
	}

	if (argc >= 3 && argc <= 10) {
		//got /all <spec1> [... <spec7>] (create alball files)
		if (strcmp (argv[1], "/all") == 0) {
			string wildName;
			for (int ii = 2; ii < argc; ii++) {
				wildName += argv[ii];
				if (argc > ii + 1)
					wildName += ",";
			}

			return proc02 (wildName);
		}
	}

	if (argc == 3 || argc == 4) {
		//got /ss <file> [<timer>] (slide show)
		if (strcmp (argv[1], "/ss") == 0) {
			int timer = DEFAULT_TIMER_SECS;
			if (argc == 4)
				timer = atoi (argv[3]);

			return proc09 (argv[2], 1000 * timer);
		}
	}

	if (argc == 7 || argc == 10) {
		//got /fmt1 <format> <baseFilename> iNumPerFile iStart iCount [jStart jCount [jStep]]
		if (strcmp (argv[1], "/fmt1") == 0) {
			string baseFilename = argv[3];
			int iNumPerFile = atoi (argv[4]);
			int iStart = atoi (argv[5]);
			int iCount = atoi (argv[6]);

			int jStart = -1;
			int jCount = -1;
			int jStep = -1;
			if (argc >= 9) {
				jStart = atoi (argv[7]);
				jCount = atoi (argv[8]);
			}
			if (argc >= 10)
				jStep = atoi (argv[9]);

			return proc04b (argv[2], baseFilename, iNumPerFile, iStart, iCount, jStart, jCount, jStep);
		}
	}

	if (argc == 5 || argc == 7 || argc == 9) {
		//got /fmt2 <format> i0 i1 [j0 j1 [iStep jStep]]
		if (strcmp (argv[1], "/fmt2") == 0) {
			int i0 = atoi (argv[3]);
			int i1 = atoi (argv[4]);

			int j0 = -1;
			int j1 = -1;
			if (argc == 7 || argc == 9) {
				j0 = atoi (argv[5]);
				j1 = atoi (argv[6]);
			}

			int iStep = 1;
			int jStep = 1;
			if (argc == 9) {
				iStep = atoi (argv[7]);
				jStep = atoi (argv[8]);
			}

			return proc04a (argv[2], i0, i1, j0, j1, iStep, jStep);
		}
	}

	//get to here means not handled
	cerr << "Error: unrecognized usage: ";
	for (int ii = 0; ii < argc; ii++)
		cerr << argv[ii] << Space;
	cerr << endl;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// wipe
/////////////////////////////////////////////////////////////////////////////
int proc01 ()
{
	int numFiles = 1000;
	int numLines = 1000;
	string name;
	fstream out;

	for (int ii = 0; ii < numFiles; ii++) {
		ostringstream nameStrm;
		nameStrm << getCurrentDirectory ();
		nameStrm << "/wipe";
		nameStrm << setfill ('0') << setw (5) << ii;
		nameStrm << ".dat";
		name = nameStrm.str ();

		out.open (name.c_str (), outMode);
		if (!out.is_open ()) {
			cerr << "Error: could not open output file " << name << endl;

		} else {
			string str = "longlineoftextlonglineoftextlonglineoftextlonglineoftextlonglineoftext\n";
			int length = str.length ();
			for (int jj = 0; jj < numLines; jj++)
				out.write (str.c_str (), length);
			out.close ();
		}
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// creates size-limited lst files (for later processing with extr /html)
/////////////////////////////////////////////////////////////////////////////
int proc02 (const string &wildName)
{
	//vector allows us to define case-insensitive sort
	StringVector vec1;
	StringVector::const_iterator k;

	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;
	string filePath = getCurrentDirectory ();
	char *p0;

	int ffFlags = MFF_RECURSE_SUBDIRS;
	while ((p0 = myFindFile (filePath.c_str (), wildName.c_str (), &fdFiles, ffFlags)) != NULL) {
		string str = FileUrlHdr2 + p0;
		swapSlashes (str);

		vec1.push_back (str); //insert string (complete file URL)
	}

	int files = vec1.size ();
	cerr << files << " files found" << endl;
	if (!files)
		return 0;

	sort (vec1.begin (), vec1.end (), lessNoCase ());

	string name;
	fstream out;
	int outlines;

	int imagesPerAlbum = 100;
	int numAlbums = files / imagesPerAlbum;
	//handle case where files is evenly divisible by imagesPerAlbum
	if ((numAlbums * imagesPerAlbum) != files)
		numAlbums++;

	k = vec1.begin ();
	for (int ii = 0; ii < numAlbums; ii++) {
		ostringstream nameStrm;
		nameStrm << string (filePath);
		nameStrm << "/alball";
		nameStrm << setfill ('0') << setw (2) << ii;
		nameStrm << ".lst";
		name = nameStrm.str ();

		out.open (name.c_str (), outMode);
		if (!out.is_open ())
			cerr << "Error: could not open output file " << name << endl;
		else {
			outlines = 0;
			for (int jj = 0; jj < imagesPerAlbum; jj++) {
				outlines++;
				string str = *k + NewLine;
				int length = str.length ();
				out.write (str.c_str (), length);
				++k;
				if (k == vec1.end ())
					break;
			}
			out.close ();
			cerr << "wrote " << outlines << " lines to " << name << endl;
		}
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// process image list file into html
/////////////////////////////////////////////////////////////////////////////
int proc03 (const string &file, int columns, const string &title)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

/* format of input file is:
http://www.node.com/subdir/file1.jpg
http://www.node.com/subdir/file2.jpg

or (generated via dir/s/b):
C:\dir\subdir\file1.jpg
C:\dir\subdir\file2.jpg
*/

	//list is not sorted, for file contents
	StringList list1;
	StringList::const_iterator k;

	istrstream in (wholeFile);

	//read and process input stream
	char buf[BUF_SIZE];
	int lineNumber = 0;
	int numImages = 0;
	while (in.getline (buf, BUF_SIZE)) {
		lineNumber++;

		//find start of URL
		char *p0 = strstri (buf, "http:"); //net url
		if (!p0)
			p0 = strstri (buf, "https:"); //net url
		if (!p0)
			p0 = strstri (buf, "file:"); //file url
		if (!p0) {
			p0 = buf;
			if (isalpha (buf[0]) && buf[1] == ':') { //just a plain file
				//prepend tag to string
				string tmp = FileUrlHdr2 + buf;
				strcpy (buf, tmp.c_str ());
			} else
				continue; //not an url or filespec
		}

		string str = p0;
		swapSlashes (str);

		list1.push_back (str);
		numImages++;
	}

	if (numImages == 0) {
		cerr << "Warning: no images found" << endl;
		return 0;
	}

	cerr << lineNumber << " lines read" << endl;
	cerr << numImages << " images found" << endl;

	int align = 0; //center
	int maxRows = 500; //this limits number of images in file

	int maxImageWidth, maxImageHeight;
	bool clipImageToMaxSize;

	//columns: specify a negative number to prevent images from being shrunk to fit
	if (columns < 2) {
		columns = abs (columns);
		maxImageWidth = maxImageHeight = 9999; //ignored
		clipImageToMaxSize = FALSE;
	} else {
		maxImageWidth = getWindowSize (TRUE) / columns;
		maxImageHeight = getWindowSize (FALSE);
		clipImageToMaxSize = TRUE;
	}

	cout << "<HTML>" << endl
		 << "<TITLE>" << title << "</TITLE>" << endl
		 << "<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">" << endl
		 << "<CENTER>" << endl << endl;

	addLinks ();

	cout << endl;

	numImages = 0;
	k = list1.begin ();
	for (int row = 0; row < maxRows; row++) {
//		cout << "<TABLE WIDTH=" << "100%" << " BORDER=1 CELLSPACING=0 CELLPADDING=2>" << endl << endl;
		cout << "<TABLE WIDTH=" << "100%" << " BORDER=0 CELLSPACING=0 CELLPADDING=2>" << endl << endl;
		cout << "<TR>" << endl;

		for (int col = 0; col < columns; col++) {
			string img (*k);
			string alt (*k);
			int slashAt = img.rfind (ForwardSlash);
			if (slashAt > 0)
				alt = img.substr (slashAt + 1, MAX_PATH);

			int imageWidth, imageHeight;
			if (clipImageToMaxSize) {
				int status = getImageSize (img, &imageWidth, &imageHeight);
				if (!status)
					imageWidth = imageHeight = DEFAULT_IMAGE_SIZE;
			}

			if (columns == 2)
				align = (isOdd (col) ? (-1) : 1);

			addImage (alt, img, imageWidth, imageHeight,
					  maxImageWidth, maxImageHeight, align, clipImageToMaxSize);
			numImages++;

			++k;
			if (k == list1.end ())
				break;
		}

		cout << "</TR>" << endl << endl;
		cout << "</TABLE>" << endl << endl;

		if (k == list1.end ())
			break;
	}

	cerr << numImages << " images written" << endl;

	addLinks ();

	cout << endl
		 << "</CENTER>" << endl
		 << "</BODY>" << endl
		 << "</HTML>" << endl;

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// process image list file into html
/////////////////////////////////////////////////////////////////////////////
int proc03L (const string &file, int columns, const string &title, const string &tail, bool useShortLink)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

/* format of input file is:
http://www.node.com/subdir/file1.jpg
http://www.node.com/subdir/file2.jpg

or (generated via dir/s/b):
C:\dir\subdir\file1.jpg
C:\dir\subdir\file2.jpg
*/

	//list is not sorted, for file contents
	StringList list1;
	StringList::const_iterator k;

	istrstream in (wholeFile);

	//read and process input stream
	char buf[BUF_SIZE];
	int lineNumber = 0;
	int numImages = 0;
	while (in.getline (buf, BUF_SIZE)) {
		lineNumber++;

		//find start of URL
		char *p0 = strstri (buf, "http:"); //net url
		if (!p0)
			p0 = strstri (buf, "https:"); //net url
		if (!p0)
			p0 = strstri (buf, "file:"); //file url
		if (!p0) {
			p0 = buf;

			if (isalpha (buf[0]) && buf[1] == ':') { //just a plain file
				//prepend tag to string
				string tmp = FileUrlHdr2 + buf;
				strcpy (buf, tmp.c_str ());
			} else
				continue; //not an url or filespec
		}

		string str = p0;
		swapSlashes (str);

		list1.push_back (str);
		numImages++;
	}

	if (numImages == 0) {
		cerr << "Warning: no images found" << endl;
		return 0;
	}

	cerr << lineNumber << " lines read" << endl;
	cerr << numImages << " images found" << endl;

	int maxRows = 16000; //this limits number of links in file

	cout << "<HTML>" << endl
		 << "<TITLE>" << title << "</TITLE>" << endl
		 << "<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">" << endl
		 << "<CENTER>" << endl << endl
//		 << "<TABLE WIDTH=200 BORDER=1 CELLSPACING=0 CELLPADDING=0>" << endl << endl;
//		 << "<TABLE WIDTH=200 BORDER=0 CELLSPACING=0 CELLPADDING=2>" << endl << endl;
		 << "<TABLE WIDTH=100% BORDER=0 CELLSPACING=0 CELLPADDING=2>" << endl << endl;

	int count = 0;
	const string nbsps = "&nbsp&nbsp&nbsp&nbsp";

	k = list1.begin ();
	for (int row = 0; row < maxRows; row++) {
		cout << "<TR>" << endl;

		for (int col = 0; col < columns; col++) {
			string img (*k);
			string alt (*k);

			if (useShortLink) {
				int slashAt = img.rfind (ForwardSlash);
				if (slashAt > 0)
					alt = img.substr (slashAt + 1, MAX_PATH);
			}

			sprintf (buf, "%d / %d%s", ++count, numImages, nbsps.c_str ());

			cout << "<TD VALIGN=TOP ALIGN=LEFT>" << endl
				 << Tab << "<FONT FACE=\"Arial\" SIZE=2>" << buf << endl
				 << Tab << "<A HREF=\"" << img << tail << "\" target=view>"
				 << alt << tail << "</A>" << endl
				 << Tab << "</FONT>" << endl
				 << "</TD>" << endl;

			++k;
			if (k == list1.end ())
				break;
		}

		cout << "</TR>" << endl << endl;

		if (k == list1.end ())
			break;
	}

	cout << "</TABLE>" << endl
		 << "</CENTER>" << endl
		 << "</BODY>" << endl
		 << "</HTML>" << endl;

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// create list of URL's given format string and min and max integers
/////////////////////////////////////////////////////////////////////////////
int proc04a (const string &fmt, int i0, int i1, int j0, int j1, int iStep, int jStep)
{
	string format = fmt + NewLine;

	bool debug = FALSE;
	if (debug) {
		fprintf (stderr, "First string, last string:\n");
		fprintf (stderr, format.c_str (), i0);
	}

	if (j0 == -1 && j1 == -1)
		for (int ii = i0; ii <= i1; ii += iStep)
			printf (format.c_str (), ii);
	else
		for (int ii = i0; ii <= i1; ii += iStep)
			for (int jj = j0; jj <= j1; jj += jStep)
				printf (format.c_str (), ii, jj);

	if (debug)
		fprintf (stderr, format.c_str (), i1);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// create files of list of URL's given format string and min and count
/////////////////////////////////////////////////////////////////////////////
int proc04b (const string &format, const string &baseFilename, int iNumPerFile,
			 int iStart, int iCount, int jStart, int jCount, int jStep)
{
	string urlFormat = format + NewLine;

	string filename; //initialized below

	fstream out;

	int outlines = 0;
	int needNewAlbum = 1;
	int albumNumber = 0;

	int iCurrentCount = iStart;
	for (int i0 = iStart; i0 < iStart + iCount; i0++) {
		if (needNewAlbum) {
			needNewAlbum = 0;

			if (albumNumber != 0) {
				out.close ();
				cerr << "wrote " << outlines << " lines to " << filename << endl;
				outlines = 0;
			}

			ostringstream strm;
			strm << getCurrentDirectory () << "/" << baseFilename
				 << setfill ('0') << setw (2) << albumNumber++ << ".lst";

			filename = strm.str ();
			out.open (filename.c_str (), outMode);
			if (!out.is_open ()) {
				cerr << "Error: could not open output file " << filename << endl;
				return 1;
			}
		}

		string str;

		if (jStart == -1 || jCount == -1) {
			char buf[1024];
			sprintf (buf, urlFormat.c_str (), iCurrentCount, iCurrentCount);
			str = buf;
			out.write (str.c_str (), str.length ());
			outlines++;

		} else {
			if (jStep == -1)
				jStep = 1;

			for (int j0 = jStart; j0 < jStart + jCount; j0 += jStep) {
				char buf[1024];
				sprintf (buf, urlFormat.c_str (), iCurrentCount, j0);
				str = buf;
				out.write (str.c_str (), str.length ());
				outlines++;
			}
		}

		iCurrentCount++;
		if (((iCurrentCount - iStart) % iNumPerFile) == 0)
			needNewAlbum = 1;
	}

	out.close ();
	cerr << "wrote " << outlines << " lines to " << filename << endl;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// test code - find location of image size in image files
/////////////////////////////////////////////////////////////////////////////
int proc05 (const string &fileName)
{
	fprintf (stderr, "this function was commented out to reduce build time\n");
/*
	//set maintains sorted and uniq'ed list
	StringSet set1;
	StringSet::const_iterator k;

	//get list of all files
	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;
	char *filePath = getCurrentDirectory ();
	char *p0;
	int files = 0;
	while ((p0 = myFindFile (filePath, fileName, &fdFiles)) != NULL) {
		string str = FileUrlHdr2;
		str += p0;
		swapSlashes (str);

		set1.insert (str); //insert string (complete file URL)
		files++;
	}

	//process each file
	int imageWidth = MAXIMAGESIZE;
	int imageHeight = MAXIMAGESIZE;
	for (k = set1.begin (); k != set1.end (); ++k) {
		string img (*k);
		getImageSize (img, &imageWidth, &imageHeight, 1);
	}
*/
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// test for duplicates via file size
// NOTE: this is not compatible with filenames that contain spaces!
/////////////////////////////////////////////////////////////////////////////
int proc06 (const string &wildName)
{
	StringVector::const_iterator k;
	StringSet::const_iterator j;
	StringSet::const_iterator m;

	DWORD start = GetTickCount ();

	string marker = "(!)@";
	TCHAR separator = ' ';
	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;
	string filePath = getCurrentDirectory ();

	//get list of all files of form "<size> 0x%%08x <filename>" then sort it
	StringVector vec1;
	char *p0;
	int ffFlags = MFF_RECURSE_SUBDIRS;
	while ((p0 = myFindFile (filePath.c_str (), wildName.c_str (), &fdFiles, ffFlags)) != NULL) {
		ostringstream ostrm;
		DWORD size = fdFiles->nFileSizeLow;

		ostrm << setfill ('0') << setw (9) << size;
		ostrm << separator << marker << separator;
		ostrm << p0;

		string str (ostrm.str ());
		swapSlashes (str);

		vec1.push_back (str); //insert string
	}
	sort (vec1.begin (), vec1.end (), lessNoCase ());
	cerr << vec1.size () << " files found" << endl;

	DWORD end = GetTickCount ();
	cerr << "Elapsed time: " << formatElapsedTime (end - start) << endl;
	start = GetTickCount ();

	string str, prevStr;
	int firstSpaceAt, lastSpaceAt;
	int currBytes, currCksum, prevCksum;

	//make a new sorted list of all dup files, based on size of file
	StringSet set1;
	int prevBytes = 0;
	for (k = vec1.begin (); k != vec1.end (); ++k) {
		string currStr = *k;
		currBytes = atoi (currStr.c_str ());
		if (prevBytes == currBytes) {
			set1.insert (currStr);
			set1.insert (prevStr);
		}
		prevBytes = currBytes;
		prevStr = currStr;
	}
	cerr << set1.size () << " possible dups found" << endl;

	end = GetTickCount ();
	cerr << "Elapsed time: " << formatElapsedTime (end - start) << endl;
	start = GetTickCount ();

	//make a new list of all dup files, this time injecting cksum
	StringVector vec2;
	for (j = set1.begin (); j != set1.end (); ++j) {
		string currStr = *j;

		lastSpaceAt = currStr.rfind (separator);
		string currFile = currStr.substr (lastSpaceAt + 1);

		currCksum = getPartialCksum (currFile);
		ostringstream ostrm;
		ostrm << "0x" << setfill ('0') << setw (8) << hex << currCksum;

		int idx = currStr.find (marker);
		currStr.replace (idx, marker.length (), ostrm.str ());

		vec2.push_back (currStr);
	}
	sort (vec2.begin (), vec2.end (), lessNoCase ());
	cerr << vec2.size () << " files processed" << endl;

	end = GetTickCount ();
	cerr << "Elapsed time: " << formatElapsedTime (end - start) << endl;
	start = GetTickCount ();

	//go through list, collecting all files that have matching size and cksum
	StringSet set2;
	StringVector vec3;
	for (k = vec2.begin (); k != vec2.end (); ++k) {
		string currStr = *k;
		currBytes = atoi (currStr.c_str ());
		firstSpaceAt = currStr.find (separator);
		str = currStr.substr (firstSpaceAt + 1, MAX_PATH);
		sscanf (str.c_str (), "%li", &currCksum); //read hex cksum

		if (prevBytes == currBytes &&
			prevCksum == currCksum) {

			//duplicate sizes and checksums found - extract and save both filenames
			lastSpaceAt = currStr.rfind (separator);
			string currFile = currStr.substr (lastSpaceAt + 1, MAX_PATH);

			lastSpaceAt = prevStr.rfind (separator);
			string prevFile = prevStr.substr (lastSpaceAt + 1, MAX_PATH);

			//create string for sorting by first file (name only)
			string triple = fileName (currFile) + separator + currFile + separator + prevFile;
			vec3.push_back (triple);

//			cout << triple << endl; //debug
		}
		prevBytes = currBytes;
		prevCksum = currCksum;
		prevStr = currStr;
	}

	//sort and print the list of dups to stdout
	StringVector vec4;
	sort (vec3.begin (), vec3.end (), lessNoCase ());
	for (k = vec3.begin (); k != vec3.end (); ++k) {
		string triple = k->c_str ();

		firstSpaceAt = triple.find (separator);
		lastSpaceAt = triple.rfind (separator);
		string file1 = triple.substr (firstSpaceAt + 1, lastSpaceAt - firstSpaceAt - 1);
		string file2 = triple.substr (lastSpaceAt + 1, MAX_PATH);

		cout << file1 << endl;
		cout << file2 << endl;

		//save this pair (if it has not already been saved)
		string pair = asWildName (file1) + Space + asWildName (file2);
		m = set2.find (pair);
		if (m == set2.end ()) {
			set2.insert (pair);
			vec4.push_back (pair);
		}
	}

	//sort and print the list of pairs to stderr
	sort (vec4.begin (), vec4.end (), lessNoCase ());
	for (k = vec4.begin (); k != vec4.end (); ++k)
		cerr << k->c_str () << endl;

	end = GetTickCount ();
	cerr << "Elapsed time: " << formatElapsedTime (end - start) << endl;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// cleanup
/////////////////////////////////////////////////////////////////////////////
int proc07 ()
{
	const string envVar = "USERPROFILE";
	char *userProfile = getenv (envVar.c_str ());
	if (userProfile) {
		string path = userProfile + string ("/Recent");
		deleteFiles (path, "*.lnk");
	} else
		cerr << "Error: " << envVar << " environment variable not set" << endl;

	char *keys[] = { "*", "avi", "bat", "bmp", "dat", "dhtml", "gif", "htm",
					 "html", "jfif", "jpeg", "jpg", "mpeg", "mpg", "pjpeg",
					 "shtml", "wmv" };
	const int numKeys = sizeof keys / sizeof keys[0];

	char root[256] = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSaveMRU\\";
	char key[256];

	for (int ii = 0; ii < numKeys; ii++) {
		strcpy (key, root);
		strcat (key, keys[ii]);
		regDeleteKey ("HKCU", key);
	}

	regDeleteKey ("HKCU", "Software\\Microsoft\\MediaPlayer\\Player\\RecentFileList");
	regDeleteKey ("HKCU", "Software\\Microsoft\\MediaPlayer\\Player\\RecentURLList");

	char value[64];

//	strcpy (key, "Software\\Microsoft\\DevStudio\\6.0\\Search");
	strcpy (key, "Software\\Microsoft\\VisualStudio\\8.0\\Find");

	regDeleteValue ("HKCU", key, "Find");
	for (int ii = 0; ii <= 30; ii++) {
		sprintf (value, "%s %d", "Find", ii);
		regDeleteValue ("HKCU", key, value);
	}

	regDeleteValue ("HKCU", key, "Replace");
	for (int ii = 0; ii <= 30; ii++) {
		sprintf (value, "%s %d", "Replace", ii);
		regDeleteValue ("HKCU", key, value);
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// removes leading path
string fileName (const string &path)
{
	string file = path;
	int lastSlashAt = path.rfind (ForwardSlash);
	if (lastSlashAt > 0)
		file = file.substr (lastSlashAt + 1, MAX_PATH);

	return file;
}

/////////////////////////////////////////////////////////////////////////////
string asUrl (const string &file)
{
	string url = file;
	int length = url.length ();
	for (int ii = 0; ii < length; ii++) {
		if (url[ii] == BackSlash)
			url[ii] = ForwardSlash;
		if (url[ii] == ':')
			url[ii] = '|';
	}

	url = FileUrlHdr3 + url;

	return url;
}

/////////////////////////////////////////////////////////////////////////////
// removes leading path and replaces last number (and everything after it) with "*"
string asWildName (const string &file)
{
	static char wildName[_MAX_PATH + 1];
int todo_convert_method_to_use_strings; //aaa

	//extract filename only (if there are slashes)
	const char *lastSlash = strrchr (file.c_str (), ForwardSlash);
	if (lastSlash != NULL)
		strcpy (wildName, ++lastSlash);
	else
		strcpy (wildName, file.c_str ());

	int index = strlen (wildName) - 1;

	//find end of last number
	while (index > 0 && !isdigit (wildName[index]))
		index--;

	//find start of last number
	while (index > 0 && isdigit (wildName[index]))
		index--;

	index++;
	if (isdigit (wildName[index]))
		strcpy (&wildName[index], "*");

	return wildName;
}

/////////////////////////////////////////////////////////////////////////////
// process image list file into slide show
/////////////////////////////////////////////////////////////////////////////
int proc09 (const string &file, int timer)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

/* format of input file is:
http://www.node.com/subdir/file1.jpg
http://www.node.com/subdir/file2.jpg

or (generated via dir/s/b):
C:\dir\subdir\file1.jpg
C:\dir\subdir\file2.jpg
*/

	//list is not sorted, for file contents
	StringList list1;
	StringList::const_iterator k;

	istrstream in (wholeFile);

	//read and process input stream
	char buf[BUF_SIZE];
	int lineNumber = 0;
	int numImages = 0;
	while (in.getline (buf, BUF_SIZE)) {
		lineNumber++;

		//find start of URL
		char *p0 = strstri (buf, "http:"); //net url
		if (!p0)
			p0 = strstri (buf, "https:"); //net url
		if (!p0)
			p0 = strstri (buf, "file:"); //file url
		if (!p0) {
			p0 = buf;
			if (isalpha (buf[0]) && buf[1] == ':') { //just a plain file
				//prepend tag to string
				string tmp = FileUrlHdr2 + buf;
				strcpy (buf, tmp.c_str ());
			} else
				continue; //not an url or filespec
		}

		string str = p0;
		swapSlashes (str);

		list1.push_back (str);
		numImages++;
	}

	if (numImages == 0) {
		cerr << "Warning: no images found" << endl;
		return 0;
	}

	cerr << lineNumber << " lines read" << endl;
	cerr << numImages << " images found" << endl;

/////////////////////////////
static char *format =
"<HTML>\n"
"<BODY>\n"
"<FORM>\n"
"<META HTTP-EQUIV=\"pragma\" CONTENT=\"no-cache\">\n"
"<META HTTP-EQUIV=\"expires\" CONTENT=\"0\">\n"
"<NOSCRIPT>\n"
"<H1>JavaScript not enabled.</H1>\n"
"</NOSCRIPT>\n"
"<SCRIPT>\n"
"var timerID;\n"
"timerID = setTimeout ('reloadPage()', %d);\n"
"function reloadPage()\n"
"{\n"
"        location.replace ('%s');\n"
"}\n"
"</SCRIPT>\n"
"<A HREF='%s'>%s</A>\n"
"<TABLE CELLSPACING=0>\n"
"<TR ALIGN=TOP>\n"
"<TD ALIGN=LEFT><IMG SRC='%s'>\n"
"</TD>\n"
"</TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"</TD><TD><FONT SIZE=25>&nbsp</FONT></TD></TR><TR>\n"
"<TD ALIGN=RIGHT><IMG SRC='%s'>\n"
"</TD>\n"
"</TR></TABLE>\n"
"</CENTER>\n"
"</FORM>\n"
"</BODY>\n"
"</HTML>\n";

	char body[2048];

	string workingDir = getCurrentDirectory () + string ("/tmp/");
	string firstFile; //initialized below

	k = list1.begin ();
	int fileCount = 0;
	bool exitLoop = FALSE;
	while (1) {
		fileCount++;

		string currImg (*k);
		string currAlt;

		++k;
		if (k == list1.end ()) {
			k = list1.begin (); //last file points back to first
			exitLoop = TRUE;
		}

		string nextImg (*k);
		string nextAlt;

		int slashAt = currImg.rfind (ForwardSlash);
		if (slashAt > 0)
			currAlt = currImg.substr (slashAt + 1, MAX_PATH);

		slashAt = nextImg.rfind (ForwardSlash);
		if (slashAt > 0)
			nextAlt = nextImg.substr (slashAt + 1, MAX_PATH);

		string currFile = workingDir + currAlt + HtmlExten;

		if (fileCount == 1)
			firstFile = currFile;

		string nextFile = workingDir + nextAlt + HtmlExten;

		char currAlt1[128];
		sprintf (currAlt1, "%s &nbsp&nbsp (%d/%d)", currAlt.c_str (), fileCount, numImages);

		fstream fout;
		fout.open (currFile.c_str (), outMode);
		if (!fout.is_open ()) {
			cerr << "Error: could not open output file " << currFile << endl;
			return 0;
		}

		string url = asUrl (nextFile);
		int bytes = sprintf (body, format, timer, url.c_str (), currImg.c_str (),
							 currAlt1, currImg.c_str (), nextImg.c_str ());
		fout.write (body, bytes);
		fout.close ();

//		cerr << currAlt << " (" << fileCount << "/" << numImages << ")" << endl;

		if (exitLoop)
			break;
	}

	cerr << FileUrlHdr3 << firstFile << endl;

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// fix lines in file by wrapping on special chars
/////////////////////////////////////////////////////////////////////////////
int proc10 (const string &file)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

//aaa - maybe test should be for "http:" and "http%" (both case insensitive, BTW)
//	char header[] = "http:"; //can't use this, ":" may be hex encoded
	char header[] = "http";
	const int headerLen = sizeof header - 1;

	int iInUrl = -1;
	for (int ii = 0; ii < size; ii++) {
		char *p = &wholeFile[ii];
		if (_strnicmp (p, header, headerLen) == 0) {
			cout << endl;
			iInUrl = 1;
		}

		if (iInUrl == 1) {

			//convert escape sequences
			if (*p == '%') {
				if (*(p + 1) == '2' && tolower (*(p + 2)) == 'f') {
					ii += 2;
					*p = '/';
				} else if (*(p + 1) == '3' && tolower (*(p + 2)) == 'a') {
					ii += 2;
					*p = ':';
				}
			}

			//handle special chars and sequences
			switch (*p) {
			case '\\':
				if (*(p + 1) == '\n') { //backslash + EOL is an escape, so remove them (in history.dat)
					ii += 2;
					*p = wholeFile[ii];
				} else if (*(p + 1) == '\\') { //double backslashes, convert to single slash
					ii += 1;
					*p = '/';
				} else { //single backslash, convert to single slash
					*p = '/';
				}
				break;

			case '/': //slash at EOL, insert a newline
				if (*(p + 1) == '\n') {
					iInUrl = 0;
					cout << endl;
				}
				break;

			case ':': //two colons in a row, insert a newline
				if (*(p + 1) == ':') {
					iInUrl = 0;
					cout << endl;
				}
				break;

			case 'x': //these sequences, insert a newline
				if (!strncmp (p, "xucjx1xucjx0", 12) ||
					!strncmp (p, "xxuyx1xxuyx0", 12)) {
					iInUrl = 0;
					cout << endl;
				}
				break;

			case '_': //these sequences, insert a newline
				if (!strncmp (p, "__p_", 4) ||
					!strncmp (p, "__thcl", 6) ||
					!strncmp (p, "__thumb_", 8)) {
					iInUrl = 0;
					cout << endl;
				}
				break;

			//these chars, insert a newline
			case '?':
			case '=':
			case '&':
			case '\'':
			case '"':
			case '<':
			case '>':
			case '*':
			case '#':
			case '%':
			case '(':
			case ')':
			case '[':
			case ']':
			case ' ':
			case ',':
			case ';':
			case '|':
				iInUrl = 0;
				cout << endl;
				//fall through
			default:
				break;
			}
		}

		if (iInUrl == 0 || iInUrl == 1)
			cout << *p;
	}

	cout << endl;

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// normalize links: for example, change www10.msn.com to www.msn.com and
//					remove trailing slashes
/////////////////////////////////////////////////////////////////////////////
int proc11 (const string &file)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

	char header[] = "http://www";
	const int headerLen = sizeof header - 1;

	for (int ii = 0; ii < size; ii++) {
		char *p = &wholeFile[ii];

		if (_strnicmp (p, header, headerLen) == 0) {
			cout << header;
			ii += headerLen;
			while (isdigit (wholeFile[ii]))
				ii++;
			cout << wholeFile[ii];

		} else if ((*p == '/' || *p == '\\') && (*(p + 1) == '\n' || *(p + 1) == '\r')) {
			//skip trailing slashes
			ii++;
			cout << '\n';


		} else
			cout << *p;
	}

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// get filename component
/////////////////////////////////////////////////////////////////////////////
string getLeaf (const string &filespec)
{
//	cerr << "getLeaf: filespec=" << filespec << endl;
	string leaf;
	string base (filespec);
	int lastBackSlash = base.rfind (BackSlash);
	if (lastBackSlash != (-1)) {
		base = base.substr (0, lastBackSlash);

		lastBackSlash = base.rfind (BackSlash);
		int length = base.length ();
		length -= lastBackSlash + 1;
		leaf = base;
		leaf = leaf.substr (lastBackSlash + 1, MAX_PATH);
	}
	
	return leaf;
}

/////////////////////////////////////////////////////////////////////////////
// get list of subdirs - sorted with longer subdirs first, alpha within
// exactLength < 0 - shuffle subfolder list
// exactLength == 0 - get all subfolders
// exactLength > 0 - only get subfolders that have the same length as "exactLength"

/////////////////////////////////////////////////////////////////////////////
string dirList (const string &filespec, int exactLength)
{
	//vector allows us to define sort
	StringVector vec1;
	StringVector::const_iterator k;

	//get list of all files
	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;
	char *p0;
	int ffFlags = MFF_RETURN_SUBDIRS;
	while ((p0 = myFindFile ((char*)filespec.c_str (), "*", &fdFiles, ffFlags)) != NULL) {
		if (ISSUBDIRP(fdFiles)) {
			string subdir = getLeaf (string (p0) + string ("\\"));
			vec1.push_back (subdir);
		}
	}

	if (exactLength >= 0) {
		sort (vec1.begin (), vec1.end (), lessViaLength ());
	} else {
		srand (unsigned (time (NULL))); //seed default random number generator
		random_shuffle (vec1.begin (), vec1.end ());
	}

	string subdirs;
	for (k = vec1.begin (); k != vec1.end (); ++k) {
		string subdir = string (*k);
		if (exactLength <= 0 || exactLength == subdir.length()) {
			subdirs += subdir + " ";
		}
	}

	return subdirs;
}

/////////////////////////////////////////////////////////////////////////////
// create a BAT file with env vars to extract components of filename
/////////////////////////////////////////////////////////////////////////////
int proc12a (const string &filespec)
{
	//split file from path
	string path, file;
	string basename (filespec);
	int lastBackSlash = basename.rfind (BackSlash);
	int albumSubFolderLength = 2;

	if (dirExists (basename.c_str ())) {
		path = basename;

	} else if (lastBackSlash == (-1)) {
		path = getCurrentDirectory (0);
		file = basename;

	} else {
		path = basename;
		path = path.substr (0, lastBackSlash);

		int length = basename.length ();
		length -= lastBackSlash + 1;
		file = basename;
		file = file.substr (lastBackSlash + 1, MAX_PATH);
	}

	char *prRoot = getenv ("PR_ROOT");
	if (prRoot != NULL) {
		for (int ii = 5; ii >= 1; --ii) {
			string subFolder = prRoot + string ("/jroot/") + file.substr (0, ii);
			if (dirExists (subFolder.c_str ())) {
				albumSubFolderLength = ii;
				break;
			}
		}
	}

	//split last extension from base
	string ext;
	string base (file);
	int lastDot = file.rfind (Dot);
	if (lastDot != (-1)) {
		base = base.substr (0, lastDot);

		int length = file.length ();
		length -= lastDot + 1;
		ext = file;
		ext = ext.substr (lastDot + 1, MAX_PATH);
	}

	//get file name only (no extensions)
	string fileOnly (file);
	int firstDot = file.find (Dot);
	if (firstDot != (-1)) {
		fileOnly = fileOnly.substr (0, firstDot);
	}

	//get complete/absolute path
	char unused1[128];
	char unused2[128];
	char *p0 = myFileParse ((char*)filespec.c_str (), unused1, unused2);
	if (p0) {
		//remove trailing asterix '*'
		int len = strlen (p0);
		if (len > 0 && *(p0 + len - 1) == '*')
			*(p0 + len - 1) = 0;

	} else {
		p0 = "NULL";
	}

	//get leaf
	string leaf = getLeaf (p0);

	//get subdirs
	string subdirsRnd = dirList (path, -1);
	string subdirsAll = dirList (path, 0);
	string subdirs1 = dirList (path, 1);
	string subdirs2 = dirList (path, 2);
	string subdirs3 = dirList (path, 3);
	string subdirs4 = dirList (path, 4);
	string subdirs5 = dirList (path, 5);
	
//	cout << "set BASENAME_PATH=" << appendSlash ((char*)path.c_str ()) << endl;
	cout << "set BASENAME_PATH=" << path << endl;
	cout << "set BASENAME_FILE=" << file << endl;
	cout << "set BASENAME_FILE_ONLY=" << fileOnly << endl;
	cout << "set BASENAME_FILE_FIRST=" << file.substr (0, 1) << endl;
	cout << "set BASENAME_FILE_ALBUM_SUBFOLDER=" << file.substr (0, albumSubFolderLength) << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS_ALL=" << subdirsAll << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS_RND=" << subdirsRnd << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS1=" << subdirs1 << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS2=" << subdirs2 << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS3=" << subdirs3 << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS4=" << subdirs4 << endl;
	cout << "set BASENAME_ALBUM_SUBFOLDERS5=" << subdirs5 << endl;
	cout << "set BASENAME_BASE=" << base << endl;
	cout << "set BASENAME_EXT=" << ext << endl;
	cout << "set BASENAME_LEAF=" << leaf << endl;
	cout << "set BASENAME_LEAF_LOWER=" << strlower (leaf.substr (0, 1).c_str ()) << leaf.substr (1, leaf.length ()) << endl;
	cout << "set BASENAME_LEAF_UPPER=" << strupper (leaf.substr (0, 1).c_str ()) << leaf.substr (1, leaf.length ()) << endl;
	cout << "set BASENAME_ABS=" << p0 << endl;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// create a BAT file with env vars to extract components of string
/////////////////////////////////////////////////////////////////////////////
int proc12b (const string &model)
{
	//split string on separator (defaults to dash '-')
	string part1, part2;
	char separator = '-';
	int lastSeparator = model.rfind (separator);

	if (lastSeparator == (-1)) {
		part1 = model;
		part2 = "";

	} else {
		part1 = model.substr (0, lastSeparator);

		int length = model.length ();
		length -= lastSeparator + 1;
		part2 = model.substr (lastSeparator + 1, MAX_PATH);
	}

	cout << "set PART1=" << part1 << endl;
	cout << "set PART2=" << part2 << endl;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// wait for FireFox window to be visible
/////////////////////////////////////////////////////////////////////////////
int proc12c (int timeoutSecs)
{
	waitForWindow ("*Mozilla Firefox", "MozillaWindowClass", timeoutSecs * 1000); //pass milliseconds

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// special special handling to cleanup bad history data
/////////////////////////////////////////////////////////////////////////////
int proc13 (const string &file)
{
/*
example bad line (exactly 73 chars long):
http://www.yyyyyyyyyyyyy.zzz/ffffffffffffffff/eeeeeeeeeee/sssssssssssssss
*/

	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

	// turn every newline into a NULL
	for (int ii = 0; ii < size; ii++) {
		char *p = &wholeFile[ii];
		if (*p == '\n')
			*p = 0;
	}

	for (int ii = 0; ii < size; ii++) {
		char *p = &wholeFile[ii];

		int len = strlen (p);
		if (strlen (p) == 73) { //bad lines contain 73 chars
			//save any lines that end in 3, 4, 5 char extensions
			if (*(p + 67) == Dot || *(p + 68) == Dot || *(p + 69) == Dot)
				cout << p << '\n'; //good lines
			else
				cerr << p << '\n'; //bad lines
		} else
			cout << p << '\n'; //good lines

		ii += strlen (p);
	}

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// fcl - find and print corrupt lines (lines with no slash after header??)
/////////////////////////////////////////////////////////////////////////////
int proc14 (const string &file)
{
	char *wholeFile;
	int size = readFile (file.c_str (), &wholeFile);
	if (size <= 0) //readFile () prints error
		return 0;

	char header1[] = "http://";
	char header2[] = "https://";
	const int header1Len = sizeof header1 - 1;
	const int header2Len = sizeof header2 - 1;

	int foundSlash;

	for (int ii = 0; ii < size; ii++) {
		char *p = &wholeFile[ii];

		foundSlash = 0;
		if (_strnicmp (p, header1, header1Len) == 0 || _strnicmp (p, header2, header2Len) == 0) {
			if (_strnicmp (p, header1, header1Len) == 0) {
				ii += header1Len;
			} else { // if (_strnicmp (p, header2, header2Len) == 0) {
				ii += header2Len;
			}
			while (wholeFile[ii] != '\n' && wholeFile[ii] != '\r') {
				if (wholeFile[ii] == '/' || wholeFile[ii] == '\\')
					foundSlash = 1;
				ii++;
			}
		}

		wholeFile[ii] = 0;
		if (foundSlash)
			cout << p << '\n'; //good lines
		else
			cerr << p << '\n'; //bad lines
	}

	delete wholeFile;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// open the given file and get a simple checksum
// return cksum on success, 0 on failure
/////////////////////////////////////////////////////////////////////////////
int getPartialCksum (string fileUrl)
{
	string name = fileUrl;

	bool isFileUrl = (fileUrl.find (FileUrlHdr2) == 0);
	if (isFileUrl)
		name = fileUrl.substr (FileUrlHdr2.length (), MAX_PATH); //strip prefix

	fstream in;

	in.open (name.c_str (), inMode);
	if (!in.is_open ()) {
		cerr << "Error: could not open input file " << name << endl;
		return 0;
	}

	static char buf[1024 * 6];
	int bufLength = sizeof (buf);

	if (isImage (fileUrl))
		bufLength = 1024; //images - read smaller buffer
	else
		in.seekg (256 * 1024); //non-images - skip uninteresting stuff at beginning

	in.read (buf, bufLength);
	in.close ();

	int *intBuf = (int*) buf;
	int intBufLen = bufLength / sizeof (int);

	int cksum = 0;
	for (int ii = 0; ii < intBufLen; ii++)
		cksum ^= intBuf[ii];

	return cksum;
}

/////////////////////////////////////////////////////////////////////////////
bool isImage (string fileName)
{
int todo; //aaa - what if the user passes 'foo*'?  this might be an image...
	const char jpg[] = ".jpg";
	const int jpgLen = sizeof jpg - 1;

	string lowName = strlower (fileName.c_str ());

	string exten;
	int ii = lowName.rfind (".");
	if (ii != string::npos)
		exten = lowName.substr (ii, MAX_PATH);

	bool match = (exten.compare (jpg) == 0);
	return match;
}

/////////////////////////////////////////////////////////////////////////////
// open the image file and get its size
// return 1 on success, 0 on failure
/////////////////////////////////////////////////////////////////////////////
int getImageSize (string fileUrl, int *width, int *height, bool test)
{
	bool isFileUrl = (fileUrl.find (FileUrlHdr2) == 0);
	if (!isFileUrl) {
		cerr << "Warning: skipping file (not a file url) " << fileUrl << endl;
		return 0;
	}

	if (!isImage (fileUrl)) {
		cerr << "Warning: skipping file (not an image) " << fileUrl << endl;
		return 0;
	}

	int length = fileUrl.length ();
	int right = length - FileUrlHdr2.length ();
	string name = fileUrl.substr (FileUrlHdr2.length (), MAX_PATH); //strip prefix

	fstream in;
	const int bufLength = 1024 * 16;
	char buf[bufLength];

	in.open (name.c_str (), inMode);
	if (!in.is_open ()) {
		cerr << "Error: could not open input file " << name << endl;
		return 0;
	}

	int found = 0;
	char sig0[] = { (char) 0xFF, (char) 0xC0, 0x00, 0x11, 0x08, 0 };
	char sig1[] = { (char) 0xFF, (char) 0xC2, 0x00, 0x11, 0x08, 0 };

	if (test) { ///////////////////////////////////////////////////////////
/*
		int ii;
		string img (name);
		string alt;
		int slashAt = img.rfind (ForwardSlash);
		if (slashAt > 0) {
			int length = img.length ();
			int right = length - slashAt - 1;
			alt = img.Right (right);
		}

		in.read (buf, bufLength);

#if 1 //looking for the signatures
		for (int ii = 0; ii < (bufLength - 5); ii++) {
			if (strcmp (sig0, buf + ii) == 0) {
				found++;
				cout << alt << ": found sig0 at ";
//setbase don't work ??
				cout << setbase (ios_base::hex) << ii << endl;
				extractImageSize (buf + ii, width, height);
			} else if (strcmp (sig1, buf + ii) == 0) {
				found++;
				cout << alt << ": found sig1 at ";
//setbase don't work ??
				cout << setbase (ios_base::hex) << ii << endl;
				extractImageSize (buf + ii, width, height);
			}
		}
#endif
*/
	} else { //////////////////////////////////////////////////////////////
		int readSize = bufLength / 8;
		while (!found && readSize <= bufLength) {
			in.read (buf, readSize);
			for (int ii = 0; ii < (readSize - 5) && !found; ii++) {
				if (strcmp (sig0, buf + ii) == 0 ||
					strcmp (sig1, buf + ii) == 0) {
					extractImageSize (buf + ii, width, height);
int todo_improve_this; //aaa - look for largest size, but still use smaller if that's all there is?
					//if this size is too small, keep looking
					if (*width > MIN_THUMB_SIZE || *height > MIN_THUMB_SIZE) {
						found = 1;
						break;
					}
				}
			}
		readSize *= 2;
		}
	}

	if (test)
		cout << ": " << *width << "x" << *height << endl;

	if (!found) {
		cerr << swapSlashes (name, false) << ": size info not found (or less than "
			 << MIN_THUMB_SIZE << "x" << MIN_THUMB_SIZE << ")" << endl;
		return 0;
	}

	in.close ();
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
// extract the image size from the current position in buf
/////////////////////////////////////////////////////////////////////////////
int extractImageSize (const char *buf, int *width, int *height)
{
	//read size from stream
	int wid = ((buf[7] & 0xFF) << 8) + (buf[8] & 0xFF);
	int hgt = ((buf[5] & 0xFF) << 8) + (buf[6] & 0xFF);

	*width = (int) wid;
	*height = (int) hgt;

	return 1;
}

/////////////////////////////////////////////////////////////////////////////
// add html for image
/////////////////////////////////////////////////////////////////////////////
void addImage (string alt1, string img,
			   int imageWidth, int imageHeight,
			   int maxImageWidth, int maxImageHeight,
			   int align, bool clipImageToMaxSize)
{
	//add spaces around name for easier text selecting in browser
	const string nbsps = "&nbsp&nbsp&nbsp&nbsp";

	string alt = nbsps + Space + alt1 + Space + nbsps;

	if (clipImageToMaxSize) {
		if (imageWidth > maxImageWidth || imageHeight > maxImageHeight) {
			double dWidth = imageWidth;
			double dHeight = imageHeight;

			double widthScale = maxImageWidth / dWidth;
			double heightScale = maxImageHeight / dHeight;
			double scale;
			if (widthScale < 1 && heightScale < 1)
				scale = min (widthScale, heightScale);
			else if (widthScale > 1 && heightScale < 1)
				scale = heightScale;
			else if (widthScale < 1 && heightScale > 1)
				scale = widthScale;

			dWidth *= scale;
			dHeight *= scale;

			imageWidth = (int) dWidth;
			imageHeight = (int) dHeight;

			//show how much image has been shrunk
			char buf[16];
			sprintf (buf, " (%d%%)", (int) (scale * 100 + .5));
			alt += buf;
		}
	}

	char *alignStr = "CENTER";
	if (align < 0)
		alignStr = "LEFT";
	else if (align > 0)
		alignStr = "RIGHT";

	cout << "<TD VALIGN=BOTTOM ALIGN=" << alignStr << ">" << endl
		 << Tab << "<FONT FACE=\"Arial\" SIZE=2>" << alt << "</FONT>" << endl
		 << Tab << "<A HREF=\"" << img << "\" target=view>" << endl
		 << Tab << "<IMG SRC=\"" << img << "\" ";
	if (clipImageToMaxSize)
		cout << "WIDTH=" << imageWidth << " HEIGHT=" << imageHeight;
	cout << " ALT=" << alt << " ALIGN=TOP></A>" << endl
		 << Tab << "</FONT>" << endl
		 << "</TD>" << endl;
}

/////////////////////////////////////////////////////////////////////////////
int emitLink (const string &name, const string &file, bool addAlways = FALSE)
{
	string href = file;

	if (addAlways || fileExists (href.c_str ())) {
		cout << "<A HREF=\"file://" << getCurrentDirectory ()
			 << "/" << file << "\">"
			 << name << "</A>" << endl;

		return 1;
	}

	return -1; //failure
}

/////////////////////////////////////////////////////////////////////////////
// add links to page
/////////////////////////////////////////////////////////////////////////////
void addLinks ()
{
	emitLink ("new", string ("albnew") + HtmlExten, 0);
	emitLink ("dir", string ("albdir") + HtmlExten, 1);
	emitLink ("dup", string ("albdup") + HtmlExten, 1);

	for (int ii = 0; ii <= 50; ii++) {
		char name[64];
		char file[64];
		sprintf (name, "%02d", ii);
		sprintf (file, "alball%02d%s", ii, HtmlExten.c_str ());
		emitLink (name, file, 1);
	}
}

/////////////////////////////////////////////////////////////////////////////
string getCurrentDirectory (bool useForwardSlashes)
{
	static char currentDir[_MAX_PATH + 1];

	GetCurrentDirectory (_MAX_PATH, currentDir);
	int length = strlen (currentDir);

	if (useForwardSlashes)
		for (int ii = 0; ii < length; ii++)
			if (currentDir[ii] == BackSlash)
				currentDir[ii] = ForwardSlash;

	return currentDir;
}

/////////////////////////////////////////////////////////////////////////////
string swapSlashes (string &str, bool useForwardSlashes)
{
	char fromSlash = useForwardSlashes ? BackSlash : ForwardSlash;
	char toSlash   = useForwardSlashes ? ForwardSlash : BackSlash;

	int index = 0;
	while ((index = str.find (fromSlash, index)) > 0)
		str[index] = toSlash;

	return str;
}

/////////////////////////////////////////////////////////////////////////////
int deleteFiles (const string &filePath, const string &wildName)
{
	FINDFILEDATA f0;
	FINDFILEDATA *fdFiles = &f0;
	char *p0;
	int files = 0;
	while ((p0 = myFindFile (filePath.c_str (), wildName.c_str (), &fdFiles)) != NULL) {
		if (DeleteFile (p0))
			files++;
		else
			fprintf (stderr, "DeleteFile (%s) failed: %s", p0, formatLastError (GetLastError ()));
	}
	fprintf (stderr, "%d files deleted\n", files);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
int regDeleteKey (const string &lpKey, const string &lpSubkey, bool ignoreNotExistError)
{
	//verify this is a key we support deleting
	HKEY hTopKey;
	if (lpKey.compare ("HKLM") == 0)
		hTopKey = HKEY_LOCAL_MACHINE;
	else if (lpKey.compare ("HKCU") == 0)
		hTopKey = HKEY_CURRENT_USER;
	else {
		fprintf (stderr, "unsupported key (%s) in %s\n", lpKey.c_str (), "regDeleteKey");
		return 0;
	}

	//must have a subkey with at least one backslash
	if (!lpSubkey.find (BackSlash)) {
		fprintf (stderr, "unsupported subkey (%s) in %s\n", lpSubkey.c_str (), "regDeleteKey");
		return 0;
	}

	//delete the key
	int status = RegDeleteKey (hTopKey, lpSubkey.c_str ());
	if (status != ERROR_SUCCESS) {
		if (!(status == ERROR_FILE_NOT_FOUND && ignoreNotExistError)) {
			fprintf (stderr, "regDeleteKey could not delete key \"%s\", error=%d\n",
					 lpSubkey.c_str (), status);
		}
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////////
int regDeleteValue (const string &lpKey, const string &lpSubkey, const string &lpValueName, bool ignoreNotExistError)
{
	//verify this is a key we support deleting from
	HKEY hTopKey;
	if (lpKey.compare ("HKLM") == 0)
		hTopKey = HKEY_LOCAL_MACHINE;
	else if (lpKey.compare ("HKCU") == 0)
		hTopKey = HKEY_CURRENT_USER;
	else if (lpKey.compare ("HKCR") == 0)
		hTopKey = HKEY_CLASSES_ROOT;
	else {
		fprintf (stderr, "unsupported key (%s) in %s\n", lpKey.c_str (), "regDeleteValue");
		return 0;
	}

	//open the key
	HKEY hKey;
	int status = RegOpenKeyEx (hTopKey, lpSubkey.c_str (), 0,
							   KEY_READ | KEY_WRITE, &hKey);
	if (status != ERROR_SUCCESS) {
		if (!(status == ERROR_FILE_NOT_FOUND && ignoreNotExistError)) {
			fprintf (stderr, "regDeleteValue could not open key \"%s\", error=%d\n",
					lpSubkey.c_str (), status);
			return 1;
		}
	}

	//delete the value
	status = RegDeleteValue (hKey, lpValueName.c_str ());
	if (status != ERROR_SUCCESS) {
		if (!(status == ERROR_FILE_NOT_FOUND && ignoreNotExistError)) {
			fprintf (stderr, "regDeleteValue could not delete value \"%s\\%s\", error=%d\n",
					lpSubkey.c_str (), lpValueName.c_str (), status);
		}
	}

	return status;
}

/////////////////////////////////////////////////////////////////////////////
// read entire file (or stdin) into new'ed buffer
// caller is responsible to delete buffer
/////////////////////////////////////////////////////////////////////////////
int readFile (const string &filePath, char **buffer)
{
	myAssert (buffer);

	if (filePath.length () == 0) {
		cerr << "Error: must specify source file" << endl;
		return 0;
	}

	bool isFile = filePath.compare ("-") == 1;

	char *wholeFile;
	int size;

	char *tmpFile = new char [MAX_FILE_SIZE];
	myAssert (tmpFile);

	if (isFile) { //read from file
		fstream fin;
		fin.open (filePath.c_str (), ios::in);
		if (!fin.is_open ()) {
			cerr << "Error: could not open input file " << filePath << endl;
			return 0;
		}

		fin.read (tmpFile, MAX_FILE_SIZE);
		size = fin.gcount ();
		tmpFile[size++] = 0;
		myAssert (size < MAX_FILE_SIZE - 2);
		wholeFile = new char [size];
		myAssert (wholeFile);
		memcpy (wholeFile, tmpFile, size);
		fin.close ();
		delete tmpFile;

//		cerr << size << " bytes read from input file " << filePath << endl;

	} else { //read from stdin
		char tmp;
		size = 0;

		while ((tmp = getchar ()) != EOF)
			tmpFile[size++] = tmp;
		tmpFile[size++] = 0;
		myAssert (size < MAX_FILE_SIZE - 2);
		wholeFile = new char [size];
		myAssert (wholeFile);
		memcpy (wholeFile, tmpFile, size);
		delete tmpFile;

/* this way seems to swallow whitespace
		char tmpFile[MAX_FILE_SIZE];
		int size = 0;
		char tmp;
		while (!cin.eof () && size < MAX_FILE_SIZE) {
			cin >> tmp;
			tmpFile[size++] = tmp;
		}
//		tmpFile[size++] = 0;
		myAssert (size < MAX_FILE_SIZE - 2);
		wholeFile = new char [size];
		myAssert (wholeFile);
		memcpy (wholeFile, tmpFile, size);
*/

//		cerr << size << " bytes read from standard input" << endl;
	}

	*buffer = wholeFile;
	return size;
}

/////////////////////////////////////////////////////////////////////////////
int getWindowSize (bool getWidthOrHeight)
{
	//allow user to specify how much of width of screen to use
	//(66 = 66% = 2/3 of the screen width)
	int factor = 100;
	char *envVar = getenv ("WID");
	if (envVar) {
		int tmp = atoi (envVar);
		if (tmp >= 10 && tmp <= 100)
			factor = tmp;
	}

	//add in some slop
	const int horzOverhead = 60;
	const int vertOverhead = 220;

	if (getWidthOrHeight) {
		int width = GetDeviceCaps (GetDC (GetDesktopWindow()), HORZRES) - horzOverhead;
		if (factor != 100) {
			width *= factor;
			width /= 100;
		}
		return width;

	} else
		return GetDeviceCaps (GetDC (GetDesktopWindow()), VERTRES) - vertOverhead;
}

/////////////////////////////////////////////////////////////////////////////
string formatElapsedTime (DWORD millisecs)
{
	static char buf[_MAX_PATH + 1];

	double seconds = millisecs/ 1000.;

	if (seconds < 60) {				//short time format
		int precision = (seconds < 10. ? 2 : 1);
		sprintf (buf, "%.*f seconds", precision, seconds);

	} else {						//long time format
		int hours = (int) seconds / 3600;
		seconds -= hours * 3600;
		int minutes = (int) seconds / 60;
		seconds -= minutes * 60;
		sprintf (buf, "%d:%02d:%02d", hours, minutes, (int) seconds);
	}

	return buf;
}

////////////////////////////////////////////////////////////////////////////////
//Callback proc for EnumWindows(); handles wildcards in window name
BOOL CALLBACK myEnumWindowsCallbackProc (HWND hWnd1, LPARAM lParam)
{
	PWINDATA pData = (PWINDATA) lParam;

	if (IsWindowVisible (hWnd1)) {
		char windowText[_MAX_PATH];
		GetWindowText (hWnd1, windowText, _MAX_PATH);

		if (0) { //debug - print name of every window examined
			char className[_MAX_PATH];
			GetClassName (hWnd1, className, _MAX_PATH);
			printf ("myEnumWindowsCallbackProc: WindowText = \"%s\", Class = \"%s\"\n", windowText, className);
			return TRUE;
		}

		if (matchtest (windowText, pData->winName)) {
			HWND hWnd2 = FindWindow (pData->winClass, windowText);

			if (hWnd2) {
				pData->hWnd = hWnd2;

				if (FileIoDebug)
					printf ("\nfound window matching name \"%s\" (\"%s\", \"%s\")\n", pData->winName, windowText, pData->winClass);

				return FALSE;
			}
		}
	}

	//no matching window found
	SetLastError (ERROR_FILE_NOT_FOUND);
	pData->hWnd = NULL;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
//returns 0 if window found, otherwise time waited (millis)
//Window name and class can be found with MSVC utility SPYXX.EXE
int waitForWindow (LPSTR windowName, LPSTR windowClass, DWORD timeoutMillis)
{
	WINDATA data;

	data.winClass = windowClass;
	data.winName = windowName; //wildcards allowed
	data.hWnd = NULL;

	DWORD start = GetTickCount ();
	DWORD elapsed = 0;

	do {
		EnumWindows (myEnumWindowsCallbackProc, (LPARAM) &data);

		if (data.hWnd)
			return 0; //found matching window

		elapsed = GetTickCount () - start;
	} while (elapsed < timeoutMillis);

	return elapsed; //timed out
}
