///////////////////////////////////////////////////////////////////////////////
// HashedFiles.h
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "framework.h"

#define NODE_ALLOCATION_INCREMENT 100
#define MAX_ERROR_MESSAGE_LEN 100

class HashedFiles
{
private:
	typedef struct tagFileNode
	{
		BOOL     Duplicate;
		wstring* FileHash;
		wstring* FileDate;
		wstring* FileTime;
		wstring* FileSize;
		wstring* FileName;
	} *FileNode;
	FileNode*    _NodeList;
	int          _NodeCount;
	int          _Allocated;
	int          _Increment;
	int          _NextNode;
	int          _NodesProcessed;
	int          _BytesProcessed;
	int          HashCompare(const wstring& string1, const wstring& string2) const;
	int          FileCompare(const wstring& string1, const wstring& string2) const;
	int          DateCompare(const wstring& string1, const wstring& string2) const;
	int          TimeCompare(const wstring& string1, const wstring& string2) const;
	int          SizeCompare(const wstring& string1, const wstring& string2) const;
public:
	HashedFiles(int Increment = NODE_ALLOCATION_INCREMENT);
	~HashedFiles() { Reset(0); }
	void AddNode(const wstring& FileHash, const wstring& FileDate, const wstring& FileTime,
	             const wstring& FileSize, const wstring& FileName);
	void SortAndCheck(int Mode);
	int  GetNodeCount() const { return _NodeCount; }
	BOOL GetNode(int Node, BOOL& Duplicate, wstring& FileHash, wstring& FileDate,
	             wstring& FileTime, wstring& FileSize, wstring& FileName) const;
	void SetDuplicate(int Node, BOOL Duplicate) { _NodeList[Node]->Duplicate = Duplicate; }
	BOOL GetFile(int Node, wstring& FileName) const;
	BOOL GetNextFile(wstring& FileName);
	BOOL SaveHash(int Node, wstring& FileHash);
	BOOL GetNode(int Node, BOOL& Duplicate) const;
	void Reset(int Increment = NODE_ALLOCATION_INCREMENT);
	BOOL Save(HWND hWnd, const int& iStartNode, const int& iSelectedFile,
	          const int& iSortMode, const TCHAR* pszDirectoryName) const;
	BOOL Load(HWND hWnd, int& iStartNode, int& iSelectedFile, int& iSortMode, TCHAR* pszDirectoryName);
};