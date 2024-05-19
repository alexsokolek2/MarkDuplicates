///////////////////////////////////////////////////////////////////////////////
// HashedFiles.cpp - Implementation of the class HashedFiles.
//
// This is an array of pointers to nodes containing a Duplicate flag,
// a FileHash, a FileDate, a FileTime, a FileSize, and a FileName. The
// length of the array is dynamically allocated in chunks of Increment
// pointers. This represents the files in a directory specified by the
// user. Various sorting options are available. The base sort option is
// by FileHash, then by FileName, which places identical files together
// with the second and subsequent file(s) marked as duplicates. Save and
// Load methods are provide to save the class and load it back later.
///////////////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "HashedFiles.h"

//=============================================================================
// Constructor - Initialize and allocate <increment> nodes.
//=============================================================================

HashedFiles::HashedFiles(int Increment)
{
	_NodeList = new FileNode[Increment];
	_NodeCount = 0;
	_Allocated = _Increment = Increment;
	_NextNode = 0;
	_NodesProcessed = 0;
	_BytesProcessed = 0;
}

//=============================================================================
// AddNode - Allocate nodes if needed and load FileHash, DateTime, FileSize, and FileName.
//=============================================================================
void HashedFiles::AddNode
	(const wstring& FileHash, const wstring& FileDate, const wstring& FileTime,
	 const wstring& FileSize, const wstring& FileName)
{
	if (_NodeCount == _Allocated)
	{
		// Allocate the pointers.
		FileNode* NewNodeList = new FileNode[_NodeCount + _Increment];
		memcpy(NewNodeList, _NodeList, sizeof(NewNodeList[0]) * _NodeCount);
		delete[] _NodeList;
		_NodeList = NewNodeList;
		_Allocated += _Increment;
	}

	// Allocate and load the node
	_NodeList[_NodeCount]            = new tagFileNode;
	_NodeList[_NodeCount]->Duplicate = false;
	_NodeList[_NodeCount]->FileHash  = new wstring(FileHash);
	_NodeList[_NodeCount]->FileDate  = new wstring(FileDate);
	_NodeList[_NodeCount]->FileTime  = new wstring(FileTime);
	_NodeList[_NodeCount]->FileSize  = new wstring(FileSize);
	_NodeList[_NodeCount]->FileName  = new wstring(FileName);
	_NodeCount++;
}

//=============================================================================
// SortAndCheck - Called after adding and processing all the nodes, sorts by
//                FileHash and then by FileName.  After sorting, marks each
//                duplicated file for ultimate deletion by the user.
//
//                Mode controls the sort: 0 means sort by hash and file and
//                then mark duplicates, 1 means sort by file alone, 2 means
//                sort by date and file, and 3 means sort by size and file.
//                If Mode > 0, then the duplicate checking is bypassed.
//=============================================================================

void HashedFiles::SortAndCheck(int SortMode)
{
	// Shell Sort
	BOOL swap;
	tagFileNode* TempNode;
	int i, j, diff = 0;
	for (j = _NodeCount / 2; j > 0; j /= 2)
	{
		swap = true;
		while (swap)
		{
			swap = false;
			for (i = 0; i + j < _NodeCount; ++i)
			{
				switch (SortMode)
				{
				case 0: // By FileHash, then by FileName
					diff =                HashCompare(_NodeList[i]->FileHash->c_str(), _NodeList[i + j]->FileHash->c_str());
					if (diff == 0) diff = FileCompare(_NodeList[i]->FileName->c_str(), _NodeList[i + j]->FileName->c_str());
					break;
				case 1: // By FileName alone
					diff =                FileCompare(_NodeList[i]->FileName->c_str(), _NodeList[i + j]->FileName->c_str());
					break;
				case 2: // By FileDate, then by FileTime, then by FileName
					diff =                DateCompare(_NodeList[i]->FileDate->c_str(), _NodeList[i + j]->FileDate->c_str());
					if (diff == 0) diff = TimeCompare(_NodeList[i]->FileTime->c_str(), _NodeList[i + j]->FileTime->c_str());
					if (diff == 0) diff = FileCompare(_NodeList[i]->FileName->c_str(), _NodeList[i + j]->FileName->c_str());
					break;
				case 3: // By FileSize, then by FileName
					diff =                SizeCompare(_NodeList[i]->FileSize->c_str(), _NodeList[i + j]->FileSize->c_str());
					if (diff == 0) diff = FileCompare(_NodeList[i]->FileName->c_str(), _NodeList[i + j]->FileName->c_str());
					break;
				}

				if (diff > 0)
				{
					swap = true;
					TempNode = _NodeList[i];
					_NodeList[i] = _NodeList[i + j];
					_NodeList[i + j] = TempNode;
				}
			}
		}
	}

	// Scan for duplicate hashes and mark them.
	if (SortMode == 0)
	{
		for (i = 1; i < _NodeCount; ++i)
		{
			if (_NodeList[i]->FileHash->compare(_NodeList[i - 1]->FileHash->c_str()) == 0)
				_NodeList[i]->Duplicate = true; else _NodeList[i]->Duplicate = false;
		}
	}
}

//=============================================================================
// HashCompare - Identical to wstring::compare.
//=============================================================================
int HashedFiles::HashCompare(const wstring& string1, const wstring& string2) const
{
	return string1.compare(string2);
}

//=============================================================================
// FileCompare - Similar to wstring::compare, but treats non-alphanumeric
// characters, i.e. in the filename, as '~' so the sort is in the right order.
// Also, this routine is case-insensitive.
//=============================================================================
int HashedFiles::FileCompare(const wstring& string1, const wstring& string2) const
{
	WCHAR char1, char2;
	for (UINT i = 0; i < min(string1.length(), string2.length()); ++i)
	{
		char1 = string1[i];
		char2 = string2[i];

		if (!iswalnum(char1)) char1 = WCHAR('~');
		if (!iswalnum(char2)) char2 = WCHAR('~');

		if (iswlower(char1)) char1 = towupper(char1);
		if (iswlower(char2)) char2 = towupper(char2);

		if (char1 < char2) return -1; // The first string is lexigraphically smaller.
		if (char1 > char2) return +1; // The first string is lexigraphically greater.
	}

	// Strings are lexigraphically identical
	if (string1.length() == string2.length()) return  0;

	// String lengths are different, so sort the shorter string first.
	if (string1.length() < string2.length()) return -1; else return +1;
}

//=============================================================================
// DateCompare - Similar to wstring::compare, but sorts
// dates that look like "mm/dd/yyyy" as "yyyymm/dd".
//=============================================================================
int HashedFiles::DateCompare(const wstring& string1, const wstring& string2) const
{
	int diff;
		diff = string1.substr(6,4).compare(string2.substr(6,4)); // sort yyyy first
	if (diff == 0)
		diff = string1.substr(0,5).compare(string2.substr(0,5)); // sort mm/dd second
	return diff;
}

//=============================================================================
// TimeCompare - Identical to wstring::compare.
//=============================================================================
int HashedFiles::TimeCompare(const wstring& string1, const wstring& string2) const
{
	return string1.compare(string2);
}

//=============================================================================
// SizeCompare - Identical to wstring::compare.
//=============================================================================
int HashedFiles::SizeCompare(const wstring& string1, const wstring& string2) const
{
	return string1.compare(string2);
}

//=============================================================================
// GetNode - Called after calling AddNode for each file along with SortAndCheck,
// to retrieve the sorted and marked FileHashes, FileDates, FileSizes, and FileNames.
//=============================================================================

BOOL HashedFiles::GetNode
(int Node, BOOL& Duplicate, wstring& FileHash, wstring& FileDate,
	wstring& FileTime, wstring& FileSize, wstring& FileName) const
{
	if (Node < 0 || Node > _NodeCount - 1) return false;
	Duplicate = _NodeList[Node]->Duplicate;
	FileHash  = _NodeList[Node]->FileHash->c_str();
	FileDate  = _NodeList[Node]->FileDate->c_str();
	FileTime  = _NodeList[Node]->FileTime->c_str();
	FileSize  = _NodeList[Node]->FileSize->c_str();
	FileName  = _NodeList[Node]->FileName->c_str();
	return true;
}

//=============================================================================
// GetNode - An overload of GetNode, which returns only the duplicate flag.
//=============================================================================

BOOL HashedFiles::GetNode(int Node, BOOL& Duplicate) const
{
	if (Node < 0 || Node > _NodeCount - 1) return false;
	Duplicate = _NodeList[Node]->Duplicate;
	return true;
}

//=============================================================================
// GetFile - Similar to GetNode, but returns only the FileName.
//=============================================================================

BOOL HashedFiles::GetFile(int Node, wstring& FileName) const
{
	if (Node < 0 || Node > _NodeCount - 1) return false;
	FileName = _NodeList[Node]->FileName->c_str();
	return true;
}

//=============================================================================
// GetNextFile - Similar to GetFile, gets the next file, and updates index.
//               Called from the worker thread.
//=============================================================================
BOOL HashedFiles::GetNextFile(wstring& FileName)
{
	if (_NextNode > _NodeCount - 1) return false;
	FileName = _NodeList[_NextNode++]->FileName->c_str();
	return true;

}

//=============================================================================
// SaveHash - Updates FileHash. Called after GetFile from the worker thread.
//            Updates statistics.
//=============================================================================

BOOL HashedFiles::SaveHash(int Node, wstring& FileHash)
{
	if (Node < 0 || Node > _NodeCount - 1) return false;
	*(_NodeList[Node]->FileHash) = FileHash;
	_NodesProcessed++;
	_BytesProcessed += atoi((const char *)(_NodeList[Node]->FileSize->c_str()));
	return true;
}

//=============================================================================
// Reset - Called by the destructor with Increment = 0. Optionally also
// called with increment > 0 to reset the class to the initial state.
//=============================================================================

void HashedFiles::Reset(int Increment)
{
	// Destructor or Init call - Delete everything.
	for (int i = 0; i < _NodeCount; ++i)
	{
		delete _NodeList[i]->FileHash;
		delete _NodeList[i]->FileDate;
		delete _NodeList[i]->FileTime;
		delete _NodeList[i]->FileSize;
		delete _NodeList[i]->FileName;
		delete _NodeList[i];
	}
	delete[] _NodeList;

	// Init call - Reset to the as-constructed state.
	if (Increment != 0)
	{
		_NodeList = new FileNode[Increment];
		_NodeCount = 0;
		_Allocated = _Increment = Increment;
		_NextNode = 0;
		_NodesProcessed = 0;
		_BytesProcessed = 0;
	}
}

//=============================================================================
// Save - Saves the class, along with three parameters
// and the selected directory, to a user specified file.
//=============================================================================
BOOL HashedFiles::Save(HWND hWnd, const int& iStartNode, const int& iSelectedFile,
	                   const int& iSortMode, const TCHAR* pszDirectoryName) const
{
	// Setup for the GetSaveFileName() call.
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	TCHAR szSaveFileName[MAX_PATH] = _T("MarkDuplicates.mdc");
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szSaveFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("MarkDuplicates Class Files (*.mdc)\0*.mdc\0All Files (*.*)\0*.*\0");
	ofn.Flags = OFN_OVERWRITEPROMPT;

	TCHAR szErrorMessage[MAX_ERROR_MESSAGE_LEN];

	// Retrieves the fully qualified file name that was selected.
	int LastAPICallLine = __LINE__ + 1;
	if (!GetSaveFileName(&ofn))
	{
		if (CommDlgExtendedError() != 0)
		{
			StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
				_T("API Error occurred in GetSaveFileName() at line %ld error code %ld"), LastAPICallLine, CommDlgExtendedError());
			MessageBeep(MB_ICONSTOP);
			MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		}
		return false;
	}

	// Create file for writing.
	LastAPICallLine = __LINE__ + 1;
	HANDLE hFile = CreateFile(szSaveFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
			_T("API Error occurred in CreateFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
		MessageBeep(MB_ICONSTOP);
		MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		return false;
	}

	wstring line;

	// Write the header line.
	line = _T("");
	TCHAR sz[16];
	_itow_s(iStartNode,    sz, 16, 10); line += sz; line += _T("|");
	_itow_s(iSelectedFile, sz, 16, 10); line += sz; line += _T("|");
	_itow_s(iSortMode,     sz, 16, 10); line += sz; line += _T("|");
	for (int i = 0; i < lstrlen(pszDirectoryName); ++i) line += *(&pszDirectoryName[i]);
	line += _T("\r\n");
	LastAPICallLine = __LINE__ + 1;
	if (!WriteFile(hFile, line.c_str(), (DWORD)line.length() * sizeof(TCHAR), NULL, NULL))
	{
		StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
			_T("API Error occurred in WriteFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
		MessageBeep(MB_ICONSTOP);
		MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		CloseHandle(hFile);
		return false;
	}

	// Write the detail lines.
	for (int i = 0; i < _NodeCount; ++i)
	{
		line = _T("");
		line += _NodeList[i]->FileHash->c_str(); line += _T("|");
		line += _NodeList[i]->FileDate->c_str(); line += _T("|");
		line += _NodeList[i]->FileTime->c_str(); line += _T("|");
		line += _NodeList[i]->FileSize->c_str(); line += _T("|");
		line += _NodeList[i]->Duplicate ? _T("X|") : _T("O|");
		line += _NodeList[i]->FileName->c_str(); line += _T("\r\n");
		LastAPICallLine = __LINE__ + 1;
		if (!WriteFile(hFile, line.c_str(), (DWORD)line.length() * sizeof(TCHAR), NULL, NULL))
		{
			StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
				_T("API Error occurred in WriteFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
			MessageBeep(MB_ICONSTOP);
			MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
			CloseHandle(hFile);
			return false;
		}
	}
	
	// Close the file.
	LastAPICallLine = __LINE__ + 1;
	if (!CloseHandle(hFile))
	{
		StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
			_T("API Error occurred in CloseHandle() at line %ld error code %ld"), LastAPICallLine, GetLastError());
		MessageBeep(MB_ICONSTOP);
		MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		return false;
	}

	return true;
}
//=============================================================================
// Load - Loads the class, the selected directory, along
// with three parameters, from a user specified file.
//=============================================================================
BOOL HashedFiles::Load(HWND hWnd, int& iStartNode, int& iSelectedFile, int& iSortMode, TCHAR* pszDirectoryName)
{
	(pszDirectoryName);
	// Setup for the GetOpenFileName() call.
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	TCHAR szOpenFileName[MAX_PATH] = _T("MarkDuplicates.mdc");
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szOpenFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("MarkDuplicates Class Files (*.mdc)\0*.mdc\0All Files (*.*)\0*.*\0");

	TCHAR szErrorMessage[MAX_ERROR_MESSAGE_LEN];

	// Retrieves the fully qualified file name that was selected.
	int LastAPICallLine = __LINE__ + 1;
	if (!GetOpenFileName(&ofn))
	{
		if (CommDlgExtendedError() != 0)
		{
			StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
				_T("API Error occurred in GetOpenFileName() at line %ld error code %ld"), LastAPICallLine, CommDlgExtendedError());
			MessageBeep(MB_ICONSTOP);
			MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		}
		return false;
	}

	// Create file for reading.
	LastAPICallLine = __LINE__ + 1;
	HANDLE hFile = CreateFile(szOpenFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
			_T("API Error occurred in CreateFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
		MessageBeep(MB_ICONSTOP);
		MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		return false;
	}

	wstring token;
	TCHAR chr;
	
	Reset();

	// Read and process the header line.
	for (int i = 0; i < 4; ++i)
	{
		token = _T("");
		do
		{
			// Read a character.
			LastAPICallLine = __LINE__ + 1;
			if (!ReadFile(hFile, &chr, sizeof(TCHAR), NULL, NULL))
			{
				StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
					_T("API Error occurred in ReadFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
				MessageBeep(MB_ICONSTOP);
				MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
				return false;
			}

			// Handle end-of-line.
			if (chr == L'\r')
			{
				LastAPICallLine = __LINE__ + 1;
				if (!ReadFile(hFile, &chr, sizeof(TCHAR), NULL, NULL)) // Skip over the '\r'
				{
					StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
						_T("API Error occurred in ReadFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
					MessageBeep(MB_ICONSTOP);
					MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
					return false;
				}
			}

			if (chr != L'|' && chr != L'\n') token += chr;
			else
			{
				switch (i)
				{
				case 0: iStartNode    = _wtoi(token.c_str()); break;
				case 1: iSelectedFile = _wtoi(token.c_str()); break;
				case 2: iSortMode     = _wtoi(token.c_str()); break;
				case 3: StringCchCopy(pszDirectoryName, MAX_PATH, token.c_str());                break;
				}
			}
		} while (chr != L'|' && chr != L'\n');
	}

	// Read and process the detail lines
	wstring Hash, Date, Time, Size, Dup, Name;
	BOOL bReadAhead = false;

	for (;;)
	{
		for (int i = 0; i < 6; ++i)
		{
			token = _T("");
			do
			{
				if (bReadAhead) bReadAhead = false;
				else
				{
					// Read a character.
					LastAPICallLine = __LINE__ + 1;
					if (!ReadFile(hFile, &chr, sizeof(TCHAR), NULL, NULL))
					{
						StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
							_T("API Error occurred in ReadFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
						MessageBeep(MB_ICONSTOP);
						MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
						return false;
					}
					
					// Handle end-of-line
					if (chr == L'\r')
					{
						LastAPICallLine = __LINE__ + 1;
						if (!ReadFile(hFile, &chr, sizeof(TCHAR), NULL, NULL)) // Skip over the '\r'
						{
							StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
								_T("API Error occurred in ReadFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
							MessageBeep(MB_ICONSTOP);
							MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
							return false;
						}
					}
				}

				if (chr != L'|' && chr != L'\n') token += chr;
				else
				{
					switch (i)
					{
					case 0: Hash = token; break;
					case 1: Date = token; break;
					case 2: Time = token; break;
					case 3: Size = token; break;
					case 4: Dup  = token; break;
					case 5: Name = token; break;
					}
				}
			} while (chr != L'|' && chr != L'\n');
		}
		
		// Insert the node.
		AddNode(Hash, Date, Time, Size, Name);
		BOOL bDup = Dup.compare(_T("X")) == 0 ? true : false;
		SetDuplicate(_NodeCount - 1, bDup);

		// Read the next character or EOF
		DWORD dwBytesRead;
		LastAPICallLine = __LINE__ + 1;
		if (!ReadFile(hFile, &chr, sizeof(TCHAR), &dwBytesRead, NULL))
		{
			StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
				_T("API Error occurred in ReadFile() at line %ld error code %ld"), LastAPICallLine, GetLastError());
			MessageBeep(MB_ICONSTOP);
			MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
			return false;
		}
		// Handle End-Of-File
		if (dwBytesRead == 0) break;

		bReadAhead = true; // Flag character already read and loop.
	}

	// Close the file.
	LastAPICallLine = __LINE__ + 1;
	if (!CloseHandle(hFile))
	{
		StringCchPrintf(szErrorMessage, MAX_ERROR_MESSAGE_LEN,
			_T("API Error occurred in CloseHandle() at line %ld error code %ld"), LastAPICallLine, GetLastError());
		MessageBeep(MB_ICONSTOP);
		MessageBox(hWnd, szErrorMessage, _T("HashedFiles.cpp"), MB_OK + MB_ICONSTOP);
		return false;
	}

	return true;
}
