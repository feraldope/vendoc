//TDir is a better dir

#define APP_NAME "TDir"
#define VERS_STR " (V5.05)"

#include <windows.h>
#include <AclAPI.h>
#include <Mq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "fileio.h"

typedef struct tagFILESLIST {
	char *lpLine;		//complete formatted output line
	char *lpFileName;	//file name only
	char *lpFileSpec;	//file name plus path
	double dBytes;
	FILETIME ftLastWrite;
	} FILESLIST;
const int iMaxFiles = 1000000;
FILESLIST fileList[iMaxFiles];

enum SortBy {None, Date, Exten, Name, Size};
SortBy sortBy;
int sortOrder = 1; //1 for normal sort, -1 for reverse sort

const int Space = 0x20; //ASCII space char

int bShowAttrs, bRecurseSubdirs, iAddlSubdirLevels, bIncludeDirs, bExcludeNonSource, bBrief;
int bForceFileFlush, bShowOnlySubdirs, bDoNotShowSubdirs;
int bShowACLs, bSkipLabels, bClearCaseViewFile, bInterixPathname, iMaxFilesToPrint;


#if defined (_MSC_VER) //print file owner to stdout
///////////////////////////////////////////////////////////////////////////////
//original example code from http://msdn.microsoft.com/en-us/library/windows/desktop/aa446629%28v=vs.85%29.aspx
int printOwnerInfoToStdout (const char *filename)
{
	DWORD dwRtnCode = 0;
	PSID pSidOwner = NULL;
	BOOL bRtnBool = TRUE;
	LPTSTR pwAccountName = NULL;
	LPTSTR pwDomainName = NULL;
	DWORD dwAcctName = 1, dwDomainName = 1;
	SID_NAME_USE eUse = SidTypeUnknown;
	PSECURITY_DESCRIPTOR pSD = NULL;

	// Get the handle of the file object.
	HANDLE hFile = CreateFile (filename,
							   GENERIC_READ,
							   FILE_SHARE_READ,
							   NULL,
							   OPEN_EXISTING,
							   FILE_FLAG_BACKUP_SEMANTICS, //necessary for directories
							   NULL);

	// Check GetLastError for CreateFile error code.
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("CreateFile error = %s\n", formatLastError (GetLastError ()));
		return -1;
	}

	// Get the owner SID of the file.
	dwRtnCode = GetSecurityInfo (hFile,
								 SE_FILE_OBJECT,
								 OWNER_SECURITY_INFORMATION,
								 &pSidOwner,
								 NULL,
								 NULL,
								 NULL,
								 &pSD);

	// Check GetLastError for GetSecurityInfo error condition.
	if (dwRtnCode != ERROR_SUCCESS) {
		printf("GetSecurityInfo error = %s\n", formatLastError (GetLastError ()));
		return -1;
	}

	// First call to LookupAccountSid to get the buffer sizes.
	bRtnBool = LookupAccountSid (NULL,           // local computer
								 pSidOwner,
								 pwAccountName,
								 (LPDWORD)&dwAcctName,
								 pwDomainName,
								 (LPDWORD)&dwDomainName,
								 &eUse);

	// Reallocate memory for the buffers.
	pwAccountName = (LPTSTR)GlobalAlloc (GMEM_FIXED, dwAcctName);

	// Check GetLastError for GlobalAlloc error condition.
	if (pwAccountName == NULL) {
		printf("GlobalAlloc error = %s\n", formatLastError (GetLastError ()));
		return -1;
	}

		pwDomainName = (LPTSTR)GlobalAlloc (GMEM_FIXED, dwDomainName);

		// Check GetLastError for GlobalAlloc error condition.
		if (pwDomainName == NULL) {
			printf("GlobalAlloc error = %s\n", formatLastError (GetLastError ()));
			return -1;
		}

		// Second call to LookupAccountSid to get the account name.
		bRtnBool = LookupAccountSid (NULL,                   // name of local or remote computer
									 pSidOwner,              // security identifier
									 pwAccountName,          // account name buffer
									 (LPDWORD)&dwAcctName,   // size of account name buffer
									 pwDomainName,           // domain name
									 (LPDWORD)&dwDomainName, // size of domain name buffer
									 &eUse);                 // SID type

		// Check GetLastError for LookupAccountSid error condition.
		if (bRtnBool == FALSE) {
			DWORD dwErrorCode = GetLastError();

			if (dwErrorCode == ERROR_NONE_MAPPED)
				printf("Account owner not found for specified SID.\n");
			else
				printf("Error in LookupAccountSid, error = %s\n", formatLastError (dwErrorCode));
			return -1;

		} else if (bRtnBool == TRUE) {
			printf("Account owner: ");
			if (pwDomainName[0] != 0)
				printf("%s\\", pwDomainName);
			printf("%s\n", pwAccountName);
		}

		return 0;
}
#else
int printOwnerInfoToStdout (const char *filename)
{
	//stub function
}
#endif

///////////////////////////////////////////////////////////////////////////////
#if defined (_MSC_VER) //print ACL information to stdout
typedef struct {
	DWORD maskBit;
	wchar_t *name;
	} PERMISSIONNAMES;
PERMISSIONNAMES names[] = {
	{ MQSEC_QUEUE_GENERIC_ALL, L"Full Control" },
	{ MQSEC_DELETE_QUEUE, L"Delete" },
	{ MQSEC_RECEIVE_MESSAGE, L"Receive Message" },
	{ MQSEC_DELETE_MESSAGE, L"Delete Message" },
	{ MQSEC_PEEK_MESSAGE, L"Peek Message" },
	{ MQSEC_RECEIVE_JOURNAL_MESSAGE, L"Receive Journal Message" },
	{ MQSEC_DELETE_JOURNAL_MESSAGE, L"Delete Journal Message" },
	{ MQSEC_GET_QUEUE_PROPERTIES, L"Get Properties" },
	{ MQSEC_SET_QUEUE_PROPERTIES, L"Set Properties" },
	{ MQSEC_GET_QUEUE_PERMISSIONS, L"Get Permissions" },
	{ MQSEC_CHANGE_QUEUE_PERMISSIONS, L"Set Permissions" },
	{ MQSEC_TAKE_QUEUE_OWNERSHIP, L"Take Ownership" },
	{ MQSEC_WRITE_MESSAGE, L"Send Message" },
};
const int numPermissions = sizeof (names) / sizeof (names [0]);

///////////////////////////////////////////////////////////////////////////////
void DisplayPermissions (LPCWSTR pwDomainName, LPCWSTR pwAccountName, LPCWSTR pwAccessType, ACCESS_MASK amMask)
{
	for (int ii = 0; ii < numPermissions; ii++) {
		if ((amMask & names[ii].maskBit) == names[ii].maskBit) {
			if (pwDomainName[0] != 0)
				wprintf(L"%s\\", pwDomainName);
			wprintf(L"%s: %s: %s\n", pwAccountName, pwAccessType, names[ii].name);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//original example code from http://msdn.microsoft.com/en-us/library/windows/desktop/ms700142%28v=vs.85%29.aspx
HRESULT DisplayDaclInfo (PSECURITY_DESCRIPTOR pSecurityDescriptor, LPCWSTR wszComputerName)
{
	// Validate the input parameters.
	if (pSecurityDescriptor == NULL || wszComputerName == NULL) {
		return MQ_ERROR_INVALID_PARAMETER;
	}

	PACL pDacl = NULL;
	ACL_SIZE_INFORMATION aclsizeinfo;
	ACCESS_ALLOWED_ACE * pAce = NULL;
	SID_NAME_USE eSidType;
	DWORD dwErrorCode = 0;
	HRESULT hr = MQ_OK;

	// Create buffers that may be large enough.
	const DWORD INITIAL_SIZE = 256;
	DWORD cchAccName = 0;
	DWORD cchDomainName = 0;
	DWORD dwAccBufferSize = INITIAL_SIZE;
	DWORD dwDomainBufferSize = INITIAL_SIZE;
	DWORD cAce;
	WCHAR * wszAccName = NULL;
	WCHAR * wszDomainName = NULL;

	// Retrieve a pointer to the DACL in the security descriptor.
	BOOL fDaclPresent = FALSE;
	BOOL fDaclDefaulted = TRUE;
	if (GetSecurityDescriptorDacl (pSecurityDescriptor, &fDaclPresent, &pDacl, &fDaclDefaulted) == FALSE) {
		printf ("GetSecurityDescriptorDacl failed. GetLastError returned: %s\n", formatLastError (GetLastError ()));
		return HRESULT_FROM_WIN32 (dwErrorCode);
	}

	// Check whether no DACL or a NULL DACL was retrieved from the security descriptor buffer.
	if (fDaclPresent == FALSE || pDacl == NULL) {
		printf ("No DACL was found (all access is denied), or a NULL DACL (unrestricted access) was found.\n");
		return MQ_OK;
	}

	// Retrieve the ACL_SIZE_INFORMATION structure to find the number of ACEs in the DACL.
	if (GetAclInformation (pDacl, &aclsizeinfo, sizeof (aclsizeinfo), AclSizeInformation) == FALSE) {
		printf ("GetAclInformation failed. GetLastError returned: %s\n", formatLastError (GetLastError ()));
		return HRESULT_FROM_WIN32 (dwErrorCode);
	}

	// Create buffers for the account name and the domain name.
	wszAccName = new WCHAR[dwAccBufferSize];
	if (wszAccName == NULL) {
		return MQ_ERROR_INSUFFICIENT_RESOURCES;
	}
	wszDomainName = new WCHAR[dwDomainBufferSize];
	if (wszDomainName == NULL) {
		return MQ_ERROR_INSUFFICIENT_RESOURCES;
	}
	memset (wszAccName, 0, dwAccBufferSize * sizeof (WCHAR));
	memset (wszDomainName, 0, dwDomainBufferSize * sizeof (WCHAR));


	// Set the computer name string to NULL for the local computer.
	if (wcscmp(wszComputerName, L".") == 0) {
		wszComputerName = L"\0";
	}

	// Loop through the ACEs and display the information.
	for (cAce = 0; cAce < aclsizeinfo.AceCount && hr == MQ_OK; cAce++) {
		// Get ACE info
		if (GetAce(pDacl, cAce, (LPVOID*)&pAce) == FALSE) {
			printf ("GetAce failed. GetLastError returned: %s\n", formatLastError (GetLastError ()));
			continue;
		}

		// Obtain the account name and domain name for the SID in the ACE.
		while (1) {
			// Set the character-count variables to the buffer sizes.
			cchAccName = dwAccBufferSize;
			cchDomainName = dwDomainBufferSize;
			if (LookupAccountSidW (wszComputerName, // NULL for the local computer
								   &pAce->SidStart,
								   wszAccName,
								   &cchAccName,
								   wszDomainName,
								   &cchDomainName,
								   &eSidType) == TRUE) {
				break;
			}

			// Check if one of the buffers was too small.
			if (cchAccName > dwAccBufferSize || cchDomainName > dwDomainBufferSize) {
				// Reallocate memory for the buffers and try again.
				printf ("The name buffers were too small. They will be reallocated.\n");
				delete [] wszAccName;
				delete [] wszDomainName;
				wszAccName = new WCHAR[cchAccName];
				if (wszAccName == NULL) {
					return MQ_ERROR_INSUFFICIENT_RESOURCES;
				}
				wszDomainName = new WCHAR[cchDomainName];
				if (wszDomainName == NULL) {
					return MQ_ERROR_INSUFFICIENT_RESOURCES;
				}
				memset (wszAccName, 0, cchAccName * sizeof (WCHAR));
				memset (wszDomainName, 0, cchDomainName * sizeof (WCHAR));
				dwAccBufferSize = cchAccName;
				dwDomainBufferSize = cchDomainName;
				continue;
			}

			// Something went wrong in the call to LookupAccountSid.
			// Check if an unexpected error occurred.
			if (GetLastError () == ERROR_NONE_MAPPED) {
				printf ("An unexpected error occurred during the call to LookupAccountSid. A name could not be found for the SID.\n" );
				wszDomainName[0] = L'\0';
				if (dwAccBufferSize > wcslen (L"!Unknown!")) {
						 // ************************************
						 // You must copy the string "!Unknown!" into the
						 // wszAccName buffer.
						 // ************************************
						 wszAccName[dwAccBufferSize - 1] = L'\0';
				}
				break;

			} else {
				dwErrorCode = GetLastError ();
				printf ("LookupAccountSid failed. GetLastError returned: %s\n", formatLastError (GetLastError ()));
				delete [] wszAccName;
				delete [] wszDomainName;
				return HRESULT_FROM_WIN32 (dwErrorCode);
			}
		}

		switch (pAce->Header.AceType) {
			case ACCESS_ALLOWED_ACE_TYPE:
				DisplayPermissions (wszDomainName, wszAccName, L"Granted", pAce->Mask);
			break;

			case ACCESS_DENIED_ACE_TYPE:
				DisplayPermissions (wszDomainName, wszAccName, L"Denied", pAce->Mask);
			break;

			default:
				printf ("Unknown ACE Type");
		}
	}

	// Free memory allocated for buffers.
	delete [] wszAccName;
	delete [] wszDomainName;

	return MQ_OK;
}

///////////////////////////////////////////////////////////////////////////////
void printAclInfoToStdout (char *filename)
{
	static wchar_t wComputerName[MAX_COMPUTERNAME_LENGTH + 1] = L"";
	if (wComputerName[0] == 0) {
		DWORD arraySize = MAX_COMPUTERNAME_LENGTH + 1;
		GetComputerNameW (wComputerName, &arraySize); //TODO check for error
	}

	PSID psidOwner;
	PSID psidGroup;
	PACL pDacl;
	PACL pSacl;
	SE_OBJECT_TYPE ObjectType = SE_FILE_OBJECT;
	SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION |
										GROUP_SECURITY_INFORMATION |
										OWNER_SECURITY_INFORMATION |
										SACL_SECURITY_INFORMATION;

	PSECURITY_DESCRIPTOR pSecurityDescriptor;

	DWORD status = GetNamedSecurityInfo (filename,
										 ObjectType,
										 SecurityInfo,
										 &psidOwner,
										 &psidGroup,
										 &pDacl,
										 &pSacl,
										 &pSecurityDescriptor);

	if (status != ERROR_SUCCESS)
		printf ("GetNamedSecurityInfo() failed: %s", formatLastError (status));

	printOwnerInfoToStdout (filename);

	DisplayDaclInfo (pSecurityDescriptor, (LPCWSTR) wComputerName);
	LocalFree (pSecurityDescriptor);
}
#else
void printAclInfoToStdout (char *filename)
{
	//stub function
}
#endif

///////////////////////////////////////////////////////////////////////////////
void printUsageAndDie (char *message = 0)
{
	if (message)
		fprintf (stderr, "\n%s: %s; use /h for help\n", APP_NAME, message);
	else
		fprintf (stderr, "\nUsage: %s [/a[x]] [/a[-]d] [/b] [/d] [/e[=]<file>[,<file>]] [/f] [/h] [/l=<bytes>{K|M}] [/m=<max files>] [/nod] [/o[-]{d|e|n|s}] [/s] [/sl] [/s<addlLevels>] [/vobs/...] [/dev/fs/...] [/x]\n", APP_NAME);

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
	bShowACLs = bSkipLabels = bClearCaseViewFile = bInterixPathname = 0;
	iMaxFilesToPrint = iMaxFiles;

	bIncludeDirs = MFF_RETURN_SUBDIRS;
	iAddlSubdirLevels = 999;
	while (argc > 1 && ((c0 = **++argv) == '-' || c0 == '/')) {
		--argc;
		switch (c1 = (*++*argv | 040)) { //assign!
			case 'a':
//				checkArgAndDieOnError (*argv, 2, c0);
				if (strcasecmp (*argv, "a") == 0) {
					bShowAttrs = 1;
				} else if (strcasecmp (*argv, "ax") == 0) {
					bShowACLs = 1;
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
				//special handling for Interix (SFU/SUA) pathnames of form: /dev/fs/Z/frameworks/ccf/cuLib/cu.C
				c2 = *((*argv)+1);
//				checkArgAndDieOnError (*argv, 1, c0);
				if (strncmp (*argv, "dev/fs/", 7) == 0) {
					bInterixPathname = 1;
					szInSpec[0] = *((*argv)+7);
					szInSpec[1] = ':';
					strcpy (&szInSpec[2], (*argv)+8);
				} else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else {
					debug = 1;
					FileIoDebug = 1;
				}
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

			case 'm':
				c2 = *((*argv)+1);
				if (isdigit (c2)) {
					iMaxFilesToPrint = atol ((*argv)+1);
				} else {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
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
				c2 = *((*argv)+1);
				if (c2 == 'l') { //user specified /sl
					//special handling for ClearCase Version Extended Pathnames
					bSkipLabels = TRUE;
				} else if (isdigit (c2)) {
					iAddlSubdirLevels = atol ((*argv)+1);
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS; //user specified /s<N>
				} else if (c2 != 0) {
					char buf[64];
					sprintf (buf, "unrecognized argument '%c%c%c'", c0, c1, c2);
					printUsageAndDie (buf);
				} else
					bRecurseSubdirs = MFF_RECURSE_SUBDIRS; //user specified /s
				break;

			case 'v':
				//special handling for ClearCase Version Extended Pathnames of form: /vobs/frameworks/ccf/cuLib/cu.C@@/main/gto/7
//				checkArgAndDieOnError (*argv, 1, c0);
				if (strncmp (*argv, "vobs", 4) == 0) {
					bClearCaseViewFile = 1;

					char *viewDrive = getenv ("CMS_VIEW_DRIVE");
					if (viewDrive)
						strcpy (szInSpec, viewDrive);
					else
						strcpy (szInSpec, "Z:"); //default to Z:

					strcat (szInSpec, (*argv)+4);
				} else {
					char buf[64];
					sprintf (buf, "unrecognized argument '%s'", *argv);
					printUsageAndDie (buf);
				}
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
	} else if (!bClearCaseViewFile && !bInterixPathname) {
		strcpy (szInSpec, ALLFILES);
	}

	//override these values
	if (bClearCaseViewFile || bInterixPathname) {
		bBrief = 1;
		bIncludeDirs = MFF_RETURN_SUBDIRS;
	}

	if (bShowACLs) {
		enableProcessPrivilege (SE_SECURITY_NAME);
	}

	//convert path to windows format
	swapSlashes (szInSpec);

	if (debug) {
		printf ("%s\n", szInSpec);
	}

	p0 = myFileParse (szInSpec, szPathOnly, szFileOnly);
	if (!p0)
		return 1; //myFileParse() prints error message

	if (!bBrief) {
		printf (formatDirHeader (szPathOnly));
	}

	int iNumFiles = 0;
	double dBytes = 0;

	int ffFlags = bIncludeDirs | bRecurseSubdirs | bExcludeNonSource;
	while ((p0 = myFindFile (szPathOnly, szFileOnly, &fdFiles, ffFlags, iAddlSubdirLevels)) != NULL) {
		//skip files in the exclude file list, if requested
		if (pExcludeFileList && matchWildcardList (p0, pExcludeFileList))
			continue;

		if ((bShowOnlySubdirs && !ISSUBDIRP (fdFiles)) || (bDoNotShowSubdirs && ISSUBDIRP (fdFiles)))
			continue;

		if (bSkipLabels && !stringIsOnlyDigits (FILENAMEP (fdFiles)))
			continue;

		//try to force OS to flush file so we get accurate size/date info
		if (bForceFileFlush && !ISSUBDIRP (fdFiles)) {
			forceFileFlush (p0, fdFiles);
		}

		//only print if size is larger than specified, or test is disabled
		if (dLargeFiles != 0 && getFileSize (fdFiles) < dLargeFiles)
			continue;

		FILETIME ftLocalTime;
		if (sortBy == Date) {
			if (!FileTimeToLocalFileTime (&fdFiles->ftLastWriteTime, &ftLocalTime)) {
				printf ("%s: FileTimeToLocalFileTime() failed: %s", p0, formatLastError (GetLastError ()));
			}
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

		if (bShowACLs) {
			printAclInfoToStdout (p0);
		}

		//save fileList to be sorted
		if (sortBy != None || iMaxFilesToPrint < iMaxFiles) {
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

		int max = min (iNumFiles, iMaxFilesToPrint);
		int first = max < iMaxFiles ? iNumFiles - max : 0;
		for (int ii = first; ii < iNumFiles; ii++) {
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
