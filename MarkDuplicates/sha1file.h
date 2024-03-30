////////////////////////////////////////////////////////////////////////////////////////////////////
// sha1file.h : Header for the sha1file class
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "framework.h"

#define MAX_ERROR_LEN 128
#define FILE_BLOCK_LEN 1024
#define SHA_DIGEST_LEN 20
#define SHA_SUMMARY_LEN 150

class sha1file
{
private:
	int          _LastAPILine;
	DWORD        _LastAPIError;
	bool         _IsOK;
	void         FormatErrorAndAbort(const TCHAR* pszSource, int err); // for SHA1 errors
	void         FormatErrorAndAbort(const TCHAR* pszFunction, DWORD Error, const TCHAR* pszFileName);   // for SHA1 errors with a file name
	void         FormatErrorAndAbort(const TCHAR* pszFunction, DWORD Error); // for API errors
public:
	sha1file();
	~sha1file();
	int          GetMessageDigestLength() { return SHA_DIGEST_LEN; }
	int          GetMessageSummaryLength() { return SHA_SUMMARY_LEN; }
	bool         Process(const TCHAR* pszFileName, int iRepeatCount, TCHAR* pszDigest, TCHAR* pszSummary);
	int          GetLastAPILine() { return _LastAPILine; }
	int          GetLastAPIError() { return _LastAPIError; }
	bool         IsOK() { return _IsOK; }
};