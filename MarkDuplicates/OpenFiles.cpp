///////////////////////////////////////////////////////////////////////////////
// OpenFiles.cpp - Similar to HashedFiles, but stores only the FileName,
//                 Y position, and Index. Used to identify what file is
//                 selected when a mouse click occurs. Only used for one
//                 page full of files.
///////////////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "OpenFiles.h"

//=============================================================================
// Constructor - Initialize and allocate <increment> nodes.
//=============================================================================

OpenFiles::OpenFiles(int Increment)
{
	_NodeList = new FileNode[Increment];
	_NodeCount = 0;
	_Allocated = _Increment = Increment;
}

//=============================================================================
// AddFile - Allocate node if needed and save YTop, YBottom, FileName,
//           and Index.
//=============================================================================
void OpenFiles::AddFile(int YTop, int YBottom, const wstring& FileName, int Index)
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
	_NodeList[_NodeCount] = new tagFileNode;
	_NodeList[_NodeCount]->YTop = YTop;
	_NodeList[_NodeCount]->YBottom = YBottom;
	_NodeList[_NodeCount]->FileName = new wstring(FileName);
	_NodeList[_NodeCount]->Index = Index;
	_NodeCount++;
}

//=============================================================================
// GetFile - Called after calling AddFile, for each file, to retrieve
// the FileName and Index with Y position in [YTop, YBottom).
//=============================================================================

BOOL OpenFiles::GetFile(int Y, wstring& FileName, int& Index)
{
	for (int i = 0; i < _NodeCount; ++i)
	{
		if (Y >= _NodeList[i]->YTop && Y < _NodeList[i]->YBottom)
		{
			FileName = _NodeList[i]->FileName->c_str();
			Index = _NodeList[i]->Index;
			return true;
		}
	}
	return false;
}

//=============================================================================
// Reset - Called by the destructor with Increment = 0. Optionally also called
// with Increment > 0 to reset the class to the initial state.
//=============================================================================

void OpenFiles::Reset(int Increment)
{
	// Delete everything.
	for (int i = 0; i < _NodeCount; ++i)
	{
		delete _NodeList[i]->FileName;
		delete _NodeList[i];
	}
	delete[] _NodeList;

	// Reset to the as-constructed state.
	if (Increment != 0)
	{
		_NodeList = new FileNode[Increment];
		_NodeCount = 0;
		_Allocated = _Increment = Increment;
	}
}
