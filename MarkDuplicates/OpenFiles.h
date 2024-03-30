///////////////////////////////////////////////////////////////////////////////
// OpenFiles.h
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "framework.h"

#define OPEN_FILES_NODE_ALLOCATION_INCREMENT 25

class OpenFiles
{
private:
	typedef struct tagFileNode
	{
		int      YTop;
		int      YBottom;
		wstring* FileName;
		int      Index;
	} *FileNode;
	FileNode*    _NodeList;
	int          _NodeCount;
	int          _Allocated;
	int          _Increment;
public:
	OpenFiles(int Increment = OPEN_FILES_NODE_ALLOCATION_INCREMENT);
	~OpenFiles() { Reset(0); }
	void AddFile(int YTop, int YBottom, const wstring& FileName, int Index);
	int GetFileCount() { return _NodeCount; }
	BOOL GetFile(int Y, wstring& FileName, int& Index);
	void Reset(int Increment = OPEN_FILES_NODE_ALLOCATION_INCREMENT);
};