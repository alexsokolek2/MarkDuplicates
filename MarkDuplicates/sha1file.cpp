////////////////////////////////////////////////////////////////////////////////////////////////////
// sha1file.cpp : Provides a class wrapper for the sha1 algorithm.
//                Based on RFC 3174 from www.faqs.org.
////////////////////////////////////////////////////////////////////////////////////////////////////


extern "C" {
#include "sha1.h"
}

#include "framework.h"
#include "sha1file.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////////////////////////
sha1file::sha1file()
{
	_IsOK = true;
	_LastAPILine = 0;
	_LastAPIError = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor - Not needed as there are no allocations that need to be freed.
////////////////////////////////////////////////////////////////////////////////////////////////////
sha1file::~sha1file()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Process a file
// 
// 
// 
// FileName  - Name of file to read.
// iRepeatCount - Number of iterations to run and check.
//                If zero, one iteration with no checking and no summary.
// 
// 
// Digest    - 61 character (SHA_DIGEST_LEN * 3 + 1) array to hold the hash.
// pszSummary   - If iRepeatCount > 0, the summary of the testing performed.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool sha1file::Process(const TCHAR* pszFileName, int iRepeatCount, TCHAR* pszDigest, TCHAR* pszSummary)
{
	int i, err, iRepeat;
	BOOL bDataError = false;

	// Get start time
	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
	double dElapsedTimeMS;
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	SHA1Context sha;
	uint8_t MessageDigest[SHA_DIGEST_LEN];
	uint8_t MessageDigestCheckHex[SHA_DIGEST_LEN * 3 + 1];
	uint8_t PassOneMessageDigest[SHA_DIGEST_LEN];
	char szMessageDigestHex[SHA_DIGEST_LEN * 3 + 1];
	
	if (iRepeatCount > 0) // Test against a check file.
	{
		// Read check file.
		TCHAR* pszCheckFile = new TCHAR[lstrlen(pszFileName) + 1];

		// First, change extension from ".dat" to ".chk".
		StringCchCopy(pszCheckFile, lstrlen(pszFileName) * 3 + 1, pszFileName);
		#ifdef UNICODE
			_LastAPILine = __LINE__ + 1;
			TCHAR* pszExtension = wcsstr(pszCheckFile, _T(".dat"));
		#else
			_LastAPILine = __LINE__ + 1;
			TCHAR* pszExtension = strstr(pszCheckFile, _T(".dat"));
		#endif
		if (pszExtension == NULL) // ".dat" not found.
		{
			FormatErrorAndAbort(_T("sha1file::Process::CreateFileChkExtension"), (DWORD)ERROR_PATH_NOT_FOUND);
		}
		StringCchCopy(pszExtension, 5, _T(".chk"));

		// Second, open file.
		_LastAPILine = __LINE__ + 1;
		HANDLE hFileChk = CreateFile(pszCheckFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFileChk == INVALID_HANDLE_VALUE)
		{
			FormatErrorAndAbort(_T("sha1file::Process::CreateFileChk"), GetLastError());
		}
		delete[] pszCheckFile;

		// Third, read file and load hex expected digest.
		DWORD cbFileCheckBuffer, dwCheck;
		_LastAPILine = __LINE__ + 1;
		BOOL bReadCheck = ReadFile(hFileChk, MessageDigestCheckHex, SHA_DIGEST_LEN * 3 - 1, &cbFileCheckBuffer, NULL);
		if (!bReadCheck) // File I/O error ?
		{
			dwCheck = GetLastError();
			CloseHandle(hFileChk);
			FormatErrorAndAbort(_T("sha1file::Process::ReadFileCheck"), dwCheck);
		}
		MessageDigestCheckHex[SHA_DIGEST_LEN * 3 - 1] = '\0'; // Add end-of-string terminator.

		// Fourth, close the file and verify the number of bytes read is correct.
		CloseHandle(hFileChk);
		if (cbFileCheckBuffer != SHA_DIGEST_LEN * 3 - 1)
		{
			FormatErrorAndAbort(_T("sha1file::Process::ReadFileCheck"), (DWORD)ERROR_INVALID_DATA);
		}
	}
	// Repeat for the number of iterations requested.
	for (iRepeat = 0; iRepeat < (int)((iRepeatCount > 0) ? iRepeatCount : 1); ++iRepeat)
	{
		// Open data file for shared reading.
		_LastAPILine = __LINE__ + 1;
		HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			FormatErrorAndAbort(_T("sha1file::Process::CreateFileDat"), GetLastError(), pszFileName);
		}

		// Reset the SHA context.
		_LastAPILine = __LINE__ + 1;
		err = SHA1Reset(&sha);
		if (err)
		{
			CloseHandle(hFile);
			FormatErrorAndAbort(_T("sha1file::Process::SHA1Reset"), err);
		}

		// Read file and process into the sha1 context.
		uint8_t FileBuffer[FILE_BLOCK_LEN];
		BOOL bRead;
		DWORD cbFileBuffer, dw;

		while (true) // Until EOF.
		{
			// Read a buffer.
			_LastAPILine = __LINE__ + 1;
			bRead = ReadFile(hFile, FileBuffer, FILE_BLOCK_LEN, &cbFileBuffer, NULL);
			if (!bRead) // I/O error
			{
				dw = GetLastError();
				CloseHandle(hFile);
				FormatErrorAndAbort(_T("sha1file::Process::ReadFile"), dw);
			}
			if (cbFileBuffer == 0) // EOF
			{
				CloseHandle(hFile);
				break;
			}

			// Process the buffer. The last buffer may be short, but that's OK.
			_LastAPILine = __LINE__ + 1;
			err = SHA1Input(&sha, FileBuffer, cbFileBuffer);
			if (err)
			{
				CloseHandle(hFile);
				FormatErrorAndAbort(_T("sha1File::Process::SHA1Input"), err);
			}
		}
	
		// Retrieve the resulting digest.
		_LastAPILine = __LINE__ + 1;
		err = SHA1Result(&sha, MessageDigest);
		if (err)
		{
			FormatErrorAndAbort(_T("sha1File::Process::SHA1Result"), err);
		}

		if (iRepeat == 0) // Save results for pass one.
		{
			for (i = 0; i < SHA_DIGEST_LEN; ++i) PassOneMessageDigest[i] = MessageDigest[i];
		}
		else if (iRepeatCount > 0) // Compare results for other passes.
		{
			for (i = 0; i < SHA_DIGEST_LEN; ++i) if (PassOneMessageDigest[i] != MessageDigest[i]) bDataError = true;
		}
	}

	// Convert to hexadecimal - "xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx".
	for (i = 0; i < SHA_DIGEST_LEN; ++i) sprintf_s(&(szMessageDigestHex[i * 3]), 3 + 1, "%02X ", MessageDigest[i]);
	szMessageDigestHex[SHA_DIGEST_LEN * 3 - 1] = '\0'; // Change last space to a null

	// Verify against expected results.
	BOOL bDataCheckError = false;
	if (iRepeatCount > 0)
	{
		for (i = 0; i < SHA_DIGEST_LEN * 3; ++i)
		{
			if (szMessageDigestHex[i] != MessageDigestCheckHex[i]) bDataCheckError = true;
		}
	}
		// If UNICODE in effect, convert to UNICODE and return result to caller.
		// (Note that this is only valid for ASCII codes 0x00 through 0x7F)
		// (Since its hexadecimal, we will only see 0-9, A-F, and space.)
		#ifdef UNICODE
			struct TwoBytes {
				unsigned char lobyte;
				unsigned char hibyte;
			} *TB = (TwoBytes*)pszDigest;
		
			for (i = 0; i <= SHA_DIGEST_LEN * 3; ++i)
			{
				TB[i].lobyte = szMessageDigestHex[i];
				TB[i].hibyte = '\0';
			}
		#else
			StringCchCopy(pszDigest, SHA_DIGEST_LEN * 3 + 1, szMessageDigestHex);
		#endif

	// Return now if caller does not want the summary.
	if (iRepeatCount == 0) return true;

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	dElapsedTimeMS = (double)ElapsedMicroseconds.QuadPart * 1000. / Frequency.QuadPart;

	StringCchPrintf(pszSummary, SHA_SUMMARY_LEN + 1, 
		_T("Iterations: %d   Elapsed: %.3f mS   One Iteration: %.3f mS\nData check: %s   Result check: %s"),
		iRepeatCount, dElapsedTimeMS, dElapsedTimeMS / iRepeatCount,
		bDataError ? _T("FAILED") : _T("PASSED"), bDataCheckError ? _T("FAILED") : _T("PASSED"));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Format error message for a WinAPI caLL
////////////////////////////////////////////////////////////////////////////////////////////////////
void sha1file::FormatErrorAndAbort(const TCHAR* pszSource, DWORD dwLastError)
{
	// Retrieve the system error message for the last-error code.

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

	// Display the error message and exit the process.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)pszSource) + 64) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed at line %d with error %d:\n\n%s"),
		pszSource, _LastAPILine, dwLastError, lpMsgBuf);
	MessageBeep(MB_ICONEXCLAMATION);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error!"), MB_OK | MB_ICONEXCLAMATION);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dwLastError);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Format error message for a WinAPI caLL with a file name
////////////////////////////////////////////////////////////////////////////////////////////////////
void sha1file::FormatErrorAndAbort(const TCHAR* pszSource, DWORD dwLastError, const TCHAR* pszFileName)
{
	// Retrieve the system error message for the last-error code.

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

	// Display the error message and exit the process.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)pszSource) + 64) * sizeof(TCHAR) +
		                              lstrlen((LPCTSTR)pszFileName));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed at line %d with error %d:\n%s\n\n%s"),
		pszSource, _LastAPILine, dwLastError, pszFileName, lpMsgBuf);
	MessageBeep(MB_ICONEXCLAMATION);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error!"), MB_OK | MB_ICONEXCLAMATION);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dwLastError);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Format error message for a SHA1 caLL
////////////////////////////////////////////////////////////////////////////////////////////////////
void sha1file::FormatErrorAndAbort(const TCHAR* pszSource, int err)
{
	// Generate the SHA1 error message for the last-error code.

	LPVOID lpMsgBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 30 * sizeof(TCHAR));
	LPVOID lpDisplayBuf;

	switch (err)
	{
	case shaNull:         StringCchCopy((LPTSTR)lpMsgBuf, 30 * sizeof(TCHAR), _T("Null pointer parameter"));      break;
	case shaInputTooLong: StringCchCopy((LPTSTR)lpMsgBuf, 30 * sizeof(TCHAR), _T("input data too long"));         break;
	case shaStateError:   StringCchCopy((LPTSTR)lpMsgBuf, 30 * sizeof(TCHAR), _T("called Input after Result"));   break;
	default:              StringCchCopy((LPTSTR)lpMsgBuf, 30 * sizeof(TCHAR), _T("err return value is invalid"));
	}

	// Display the error message and exit the process.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)pszSource) + 64) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed at line %d with error %d:\n\n%s"),
		pszSource, _LastAPILine, err, lpMsgBuf);
	MessageBeep(MB_ICONEXCLAMATION);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error!"), MB_OK | MB_ICONEXCLAMATION);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(err);
}
