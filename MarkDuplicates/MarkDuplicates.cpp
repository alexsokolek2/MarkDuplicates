//======================================================================
// WARNING: If you can see this warning, then you are using the wrong
// version of the source. Please go back and do a git clone followed by
// a git checkout v1.0.0.4 to get the latest release version.
//======================================================================
// 
// MarkDuplicates.cpp : Defines the entry point for the application.
//
// MarkDuplicates scans a user specified directory, gathering filenames,
// write times, and sizes, along with the generated SHA-1 message digest.
// This information is put into a class (a list of nodes). It is sorted
// by message digest and then by file name. The result is that identical
// files are grouped together. Duplicates are flagged and made available
// for marking, which renames the files as "base.DELETE.ext". The user can
// then look at the directory with explorer and select all of the marked
// files for deletion. This solves the problem created when a new laptop
// was acquired and OneDrive was accidentally told to upload multiple,
// duplicate copies of the files. The user can override the duplicate
// status of each file with X (for duplicate) and O (for non duplicate).
// While looking at the list of files, the user can examine a file by
// pressing Enter or double clicking. This uses a Shell Open process.
//
// In addition to this marking, the user can initiate a test sequence
// which tests the SHA-1 implementation with the four tests described
// in RFC-3174, along with a fifth test of my own, a large PDF file.
// 
// The user can change the font and color of the display, and that
// will be saved to the registry. Initially, the columns of the display
// do not line up, but changing to a fixed pitch font, such as Courier
// will fix that. I like Courier Bold 12 Green best.
// 
// Saves and restores the window placement in the registry at
// HKCU/Software/Alex Sokolek/Mark Duplicates/1.0.0.1/WindowPlacement.
// 
// Provision is made for saving and restoring the current node list,
// along with the sort mode and scroll position and the selected file
// so that review (which can be lengthy) can continue later.
// 
// Demonstrates using a class to wrap a set of C functions implementing
// the SHA-1 Secure Message Digest algorithm described in RFC-3174.
// 
// Compilation requires that UNICODE be defined. Some of the choices
// made in code, mainly wstring, do not support detecting UNICODE vs
// non-UNICODE, so don't compile without UNICODE defined.
//
// In case you get linker errors, be sure to include version.lib
// in the linker input line.
// 
// Microsoft Visual Studio 2022 Community Edition 64 Bit 17.9.6
//
// Alex Sokolek, Version 1.0.0.1, Copyright (c) March 29, 2024
//
// Version 1.0.0.2, April 10, 2024, Corrected PeekMessage() error.
//
// Version 1.0.0.3, April 24, 2024, Updated version for release.
//
// Version 1.0.0.4, May 14, 2024, Fixed saving/loading of selected directory.

#include "framework.h"
#include "MarkDuplicates.h"

#include "ApplicationRegistry.h"
#include "sha1file.h"
#include "HashedFiles.h"
#include "OpenFiles.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
CHOOSEFONT sChooseFont;                         // The ChooseFont structure for the paint procedure
LOGFONT sLogFont;                               // The LogFont structure for the paint procedure
BOOL bChooseFont = false;                       // The result of calling ChooseFont
HFONT hFont = 0, hOldFont = 0;                  // Old and new fonts for the paint procedure
HashedFiles* pCHashedFiles;                     // Hashed Files class
//wstring DirectoryName;                          // Temp directory for hashed files
TCHAR szDirectoryName[MAX_PATH];                // Directory for hashed files
TCHAR szOldDirectoryName[MAX_PATH];             // Original current directory
BOOL bMarked = false;                           // Flag indicating that the files have already been marked
int iSortMode = 0;                              // Sort mode: Hash, Name, Date, Size
OpenFiles* pCOpenFiles;                         // The OpenFiles class, for double click open
wstring* pDblClickFile;                         // Needed by the OpenFiles class
int iSelectedFile;                              // The file in the window that is selected
int iNode;                                      // The node that is selected by single click
HWND hWndProgressBox;                           // The handle of the modeless progress dialog box
uint64_t BytesProcessed;                        // Total bytes processed

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI        FileHashWorkerThread(LPVOID lpParam);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    MDBoxProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_     HINSTANCE hInstance,
	                  _In_opt_ HINSTANCE hPrevInstance,
	                  _In_     TCHAR*    lpCmdLine,
	                  _In_     int       nCmdShow)
{
	// Setup to check for memory leaks. (Debug only.)
	#ifdef _DEBUG
	_CrtMemState sOld;
	_CrtMemState sNew;
	_CrtMemState sDiff;
	_CrtMemCheckpoint(&sOld); //take a snapshot
	#endif // _DEBUG

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MARKDUPLICATES, szWindowClass, MAX_LOADSTRING);

	// Allow only one instance of this application to run at the same time.
	// If not, find and activate the other window and then exit.
	HANDLE hMutex = CreateMutex(NULL, false, _T("{5A769DB2-14AC-4DA5-AEC7-881CA944045A}"));
	if (hMutex == NULL || hMutex == (HANDLE)ERROR_INVALID_HANDLE || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		MessageBox(NULL, _T("ERROR: Unable to create mutex!\n\nAnother instance is probably running."), szTitle, MB_OK | MB_ICONSTOP);
		HWND hWnd = FindWindow(NULL, szTitle);
		if (hWnd) PostMessage(hWnd, WM_ACTIVATE, WA_ACTIVE, NULL);

		return false;
	}

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MARKDUPLICATES));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	CloseHandle(hMutex);

	delete pCHashedFiles;
	delete pCOpenFiles;
	delete pDblClickFile;

	// Check for memory leaks. (Debug only.)
	#ifdef _DEBUG
	_CrtMemCheckpoint(&sNew); //take a snapshot 
	if (_CrtMemDifference(&sDiff, &sOld, &sNew)) // if there is a difference
	{
		MessageBeep(MB_ICONEXCLAMATION);
		MessageBox(NULL, _T("MEMORY LEAK(S) DETECTED!\n\nSee debug log."), szTitle, MB_OK | MB_ICONEXCLAMATION);
		OutputDebugString(L"-----------_CrtMemDumpStatistics ---------\n");
		_CrtMemDumpStatistics(&sDiff);
		OutputDebugString(L"-----------_CrtMemDumpAllObjectsSince ---------\n");
		_CrtMemDumpAllObjectsSince(&sOld);
		OutputDebugString(L"-----------_CrtDumpMemoryLeaks ---------\n");
		_CrtDumpMemoryLeaks();
	}
	#endif // _DEBUG

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MARKDUPLICATES));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MARKDUPLICATES);
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	pCHashedFiles = new HashedFiles;
	pCOpenFiles   = new OpenFiles;
	pDblClickFile = new wstring;
	iSelectedFile = 0;

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Initialize font structures for the command and paint procedures
	// (Declared global so already initialized to zeroes)
	sChooseFont.lStructSize = sizeof(sChooseFont);
	sChooseFont.hwndOwner = hWnd;
	sChooseFont.lpLogFont = &sLogFont;
	sChooseFont.Flags = CF_INITTOLOGFONTSTRUCT | CF_FIXEDPITCHONLY | CF_EFFECTS;

	// Initialize ApplicationRegistry class and load/restore window placement from the registry
	ApplicationRegistry* pAppReg = new ApplicationRegistry;
	if (pAppReg->Init(hWnd))
	{
		WINDOWPLACEMENT wp;
		if (pAppReg->LoadMemoryBlock(_T("WindowPlacement"), (LPBYTE)&wp, sizeof(wp)))
		{
			if (wp.flags == 0 && wp.showCmd == SW_MINIMIZE) wp.flags = WPF_SETMINPOSITION;
			SetWindowPlacement(hWnd, &wp);
		}
	}

	// Load/restore Choosefont and LogFont structures from the registry
	if (pAppReg->LoadMemoryBlock(_T("ChooseFont"), (LPBYTE)&sChooseFont, sizeof(sChooseFont)))
	{
		if (pAppReg->LoadMemoryBlock(_T("LogFont"), (LPBYTE)&sLogFont, sizeof(sLogFont)))
		{
			sChooseFont.hwndOwner = hWnd;
			sChooseFont.lpLogFont = &sLogFont;
			bChooseFont = true;
		}
	}
	delete pAppReg;

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND    - process the application menu
//  WM_MOUSEWHEEL - Process mouse scolling
//  WM_KEYDOWN    - Process keybouard scrolling and file launching
//  WM_VSCROLL    - Process scrollbar scrolling
//  WM_PAINT      - Paint the main window
//  WM_DESTROY    - post a quit message and return
//  WM_CREATE     - Create the modeless dialog box
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static UINT_PTR uiTimer;
	static int iStartNode = 0;
	HDC dc;
	TEXTMETRIC tm;
	RECT rect;
	SCROLLINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
	GetScrollInfo(hWnd, SB_VERT, &si);

	ApplicationRegistry* pAppReg = new ApplicationRegistry;
	pAppReg->Init(hWnd);

	switch (message)
	{
	case WM_INITDIALOG:
	{
		return true;
	}

	case WM_COMMAND:
		{
		sha1file SHAFile;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		// Declarations, initializations, and allocations for ID_FILE_TEST
		/////////////////////////////////////////////////////////////////////////////////////////////////
		int cbMessageDigest = SHAFile.GetMessageDigestLength() * 3 + 1;
		TCHAR* pszMessageDigest = new TCHAR[cbMessageDigest];

		int cbMessageSummary = SHAFile.GetMessageSummaryLength() + 1;
		TCHAR* pszMessageSummary = new TCHAR[cbMessageSummary];

		int cbMessageFinal = cbMessageDigest + cbMessageSummary + 1;
		TCHAR* pszMessageFinal = new TCHAR[cbMessageFinal];
		/////////////////////////////////////////////////////////////////////////////////////////////////

		int wmId = LOWORD(wParam);
			// Parse the menu selections:
		switch (wmId)
		{
		case ID_EDIT_FONT:
			if (ChooseFont(&sChooseFont))
			{
				if (pAppReg->SaveMemoryBlock(_T("ChooseFont"), (LPBYTE)&sChooseFont, sizeof(sChooseFont)))
				{
					bChooseFont = true;
					pAppReg->SaveMemoryBlock(_T("LogFont"), (LPBYTE)&sLogFont, sizeof(sLogFont));
					InvalidateRect(hWnd, NULL, true);
				}
			}
			break;

		case ID_FILE_TEST:
			{
				/////////////////////////////////////////////////////////////////////////////////////////////////
				// Run the four SHA1 test cases described in RFC 3174
				/////////////////////////////////////////////////////////////////////////////////////////////////
				SHAFile.Process(_T("Test1.dat"), 100, pszMessageDigest, pszMessageSummary);
				StringCchPrintf(pszMessageFinal, cbMessageFinal, _T("%s\n\n%s"), pszMessageDigest, pszMessageSummary);
				MessageBox(hWnd, pszMessageFinal, _T("Test1"), MB_OK);

				SHAFile.Process(_T("Test2.dat"), 100, pszMessageDigest, pszMessageSummary);
				StringCchPrintf(pszMessageFinal, cbMessageFinal, _T("%s\n\n%s"), pszMessageDigest, pszMessageSummary);
				MessageBox(hWnd, pszMessageFinal, _T("Test2"), MB_OK);

				SHAFile.Process(_T("Test3.dat"), 100, pszMessageDigest, pszMessageSummary);
				StringCchPrintf(pszMessageFinal, cbMessageFinal, _T("%s\n\n%s"), pszMessageDigest, pszMessageSummary);
				MessageBox(hWnd, pszMessageFinal, _T("Test3"), MB_OK);

				SHAFile.Process(_T("Test4.dat"), 100, pszMessageDigest, pszMessageSummary);
				StringCchPrintf(pszMessageFinal, cbMessageFinal, _T("%s\n\n%s"), pszMessageDigest, pszMessageSummary);
				MessageBox(hWnd, pszMessageFinal, _T("Test4"), MB_OK);
				/////////////////////////////
				// and run one test of my own
				/////////////////////////////
				SHAFile.Process(_T("Test5.dat"), 0, pszMessageDigest, NULL);
				MessageBox(hWnd, pszMessageDigest, _T("Test5"), MB_OK);
				break;
			}
		case ID_FILE_SCAN:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Select directory and then scan, hash, and sort.
			/////////////////////////////////////////////////////////////////////////////////////////////////
			{
				MessageBox(hWnd, _T("Navigate to the desired directory and double\n"
				                    "click on any file to process the directory."), szTitle, MB_OK);

				OPENFILENAME ofn;
				ZeroMemory(&ofn, sizeof(ofn));

				// Initially, _T(""), replaced by the path to scan via ofn.lpstrFile
				TCHAR* pszOpenFileName = new TCHAR[MAX_PATH];
				ZeroMemory(pszOpenFileName, MAX_PATH * sizeof(TCHAR));

				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = pszOpenFileName;
				ofn.nMaxFile = MAX_PATH;

				// Save original current directory.
				GetCurrentDirectory(MAX_PATH, szOldDirectoryName);

				int LastAPICallLine = __LINE__ + 1;
				if (!GetOpenFileName(&ofn)) // Retrieves the fully qualified file name that was selected.
				{
					if (CommDlgExtendedError() == 0) // Case of user pressed Cancel.
					{
						delete[] pszOpenFileName;
						break;
					}
					TCHAR sz[MAX_ERROR_MESSAGE_LEN];
					StringCchPrintf(sz, MAX_ERROR_MESSAGE_LEN,
						_T("API Error occurred at line %ld error code %ld"), LastAPICallLine, CommDlgExtendedError());
					MessageBox(hWnd, sz, _T("MarkDuplicates.cpp"), MB_OK + MB_ICONSTOP);
					break;
				}
				ofn.lpstrFile[ofn.nFileOffset] = TCHAR('\0'); // We only want the path, so eliminate the file name.
				WIN32_FIND_DATA Win32FindData;

				FILETIME LocalFileTime;
				SYSTEMTIME LocalSystemTime;

				#define FORMATTED_FILE_DATE_LEN 11
				TCHAR* pszFileDate = new TCHAR[FORMATTED_FILE_DATE_LEN];

				#define FORMATTED_FILE_TIME_LEN 6
				TCHAR* pszFileTime = new TCHAR[FORMATTED_FILE_TIME_LEN];

				#define FORMATTED_FILE_SIZE_LEN 21
				TCHAR* pszFileSize = new TCHAR[FORMATTED_FILE_SIZE_LEN];

				pCHashedFiles->Reset();

				// Save directory name in global array for use by other routines
				StringCchCopy(szDirectoryName, MAX_PATH, ofn.lpstrFile);
				SetCurrentDirectory(ofn.lpstrFile);

				// Count the files in the directory.
				int iTotalFiles = 0;
				HANDLE hFind = FindFirstFile(_T(".\\*.*"), &Win32FindData);
				if (hFind == INVALID_HANDLE_VALUE) break;
				do iTotalFiles += Win32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 0 : 1;
				while (FindNextFile(hFind, &Win32FindData) != 0);
				FindClose(hFind);

				// Setup to use the modeless dialog box to display progress.
				dc = GetDC(hWndProgressBox);
				TCHAR szFilesProcessed[100];
				RECT WindowRect;
				GetWindowRect(hWnd, &WindowRect);
				ShowWindow(hWndProgressBox, SW_SHOW);
				SetWindowPos(hWndProgressBox, HWND_NOTOPMOST, WindowRect.left+50, WindowRect.top+50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
				uint64_t FileSize;

				// Get the first file in the directory
				hFind = FindFirstFile(_T(".\\*.*"), &Win32FindData);
				if (hFind == INVALID_HANDLE_VALUE)
				{
					ShowWindow(hWndProgressBox, SW_HIDE);
					delete[] pszFileDate;
					delete[] pszFileTime;
					delete[] pszFileSize;
					delete[] pszOpenFileName;
					break;
				}

				BOOL bAbort = false;
				BytesProcessed = 0;

				do
				{
					// Ignore subdirectories.
					if (Win32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

					// Get the last write time and format the local time equivalent of it.
					FileTimeToLocalFileTime(&Win32FindData.ftLastWriteTime, &LocalFileTime);
					FileTimeToSystemTime(&LocalFileTime, &LocalSystemTime);
					StringCchPrintf(pszFileDate, FORMATTED_FILE_DATE_LEN, _T("%02d/%02d/%04d"),
					                LocalSystemTime.wMonth, LocalSystemTime.wDay, LocalSystemTime.wYear);
					StringCchPrintf(pszFileTime, FORMATTED_FILE_TIME_LEN, _T("%02d:%02d"),
					                LocalSystemTime.wHour, LocalSystemTime.wMinute);

					// Get the file size and format it.
					FileSize = (uint64_t)Win32FindData.nFileSizeHigh * ((uint64_t)MAXDWORD + 1) +
					           (uint64_t)Win32FindData.nFileSizeLow;
					StringCchPrintf(pszFileSize, FORMATTED_FILE_SIZE_LEN, _T("%9llu"), FileSize);
					BytesProcessed += FileSize;

					// Generate sha1 digest.
//					SHAFile.Process(Win32FindData.cFileName, 0, pszMessageDigest, NULL);

					// Add the file information to the HashedFiles class.
					pCHashedFiles->AddNode(_T(""), pszFileDate, pszFileTime, pszFileSize, Win32FindData.cFileName);

				} while (FindNextFile(hFind, &Win32FindData) != 0); // Process all files in the directory.
				FindClose(hFind);

				// Parameters for each thread.
				int Threads = 2;
				HANDLE hcsMutex = CreateMutex(NULL, true, _T("{B0DBEB02-3839-43DD-8D4C-D217B7F4EB9D}"));
				typedef struct ThreadProcParameters
				{
					HANDLE       hcsMutex;
					BOOL*        pbAbort;
					HashedFiles* pCHashedFiles;
				} THREADPROCPARAMETERS, *PTHREADPROCPARAMETERS;
				PTHREADPROCPARAMETERS* pThreadProcParameters = new PTHREADPROCPARAMETERS[Threads];
				HANDLE* phThreadArray = new HANDLE[Threads];

				// Initialize and instantiate the Worker Thread Pool
				for (int Thread = 0; Thread < Threads; ++Thread)
				{
					// Allocate the parameter structure for this thread.
					pThreadProcParameters[Thread] =
						(PTHREADPROCPARAMETERS)HeapAlloc(GetProcessHeap(),
							HEAP_ZERO_MEMORY, sizeof(THREADPROCPARAMETERS));
					if (pThreadProcParameters[Thread] == NULL) ExitProcess(2);

					// Initialize the parameters for this thread.
					pThreadProcParameters[Thread]->hcsMutex = hcsMutex;
					pThreadProcParameters[Thread]->pbAbort = &bAbort;
					pThreadProcParameters[Thread]->pCHashedFiles = pCHashedFiles;

					// Create and launch this thread, initially stalled waiting for the mutex.
					phThreadArray[Thread] = CreateThread
					(NULL, 0, FileHashWorkerThread, pThreadProcParameters[Thread], 0, NULL);
					if (phThreadArray[Thread] == NULL)
					{
						MessageBeep(MB_ICONEXCLAMATION);
						MessageBox(hWnd, _T("CreateThread"), szTitle, MB_OK | MB_ICONEXCLAMATION);
						ExitProcess(3);
					}
				}

				// Release the held threads.
				ReleaseMutex(hcsMutex);

				// Wait for all threads to terminate.
				for (;;)
				{
					// Wait for up to fifty milliseconds.
					if (WaitForMultipleObjects(Threads, phThreadArray, TRUE, 50) == WAIT_OBJECT_0) break;

					// Update the user about progress.
					int iPercent = (int)(pCHashedFiles->GetNodesProcessed() * 100.f /
						pCHashedFiles->GetNodeCount() + 0.5f);
					StringCchPrintf(szFilesProcessed, 100,
						_T("Files processed: %u     %d%% of %d     MBytes processed: %llu"),
						pCHashedFiles->GetNodeCount(), iPercent, iTotalFiles, BytesProcessed / 1024 / 1024);
					SetBkColor(dc, RGB(240, 240, 240));
					TextOut(dc, 16, 16, szFilesProcessed, lstrlen(szFilesProcessed));
					TextOut(dc, 16, 40, _T("Press ESC to abort."), 19);

					// Check for ESC pressed - Abort if so.
					MSG msg;
					if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) continue;
					if (msg.message != WM_KEYDOWN || msg.wParam != VK_ESCAPE) continue;
					bAbort = true;
					WaitForMultipleObjects(Threads, phThreadArray, true, INFINITE);
					pCHashedFiles->Reset();
					break;
				}

				// Delete the critical section mutex
				CloseHandle(hcsMutex);

				// Deallocate the thread arrays and structures.
				for (int Thread = 0; Thread < Threads; ++Thread)
				{
					CloseHandle(phThreadArray[Thread]);
					HeapFree(GetProcessHeap(), 0, pThreadProcParameters[Thread]);
				}
				delete[] phThreadArray;
				delete[] pThreadProcParameters;

				MessageBeep(MB_ICONASTERISK);
				ReleaseDC(hWnd, dc);

				// Sort by hash then file.
				iSortMode = 0;
				if (!bAbort) pCHashedFiles->SortAndCheck(iSortMode);
				
				// Setup initial view.
				bMarked = false;
				iStartNode = 0;
				iSelectedFile = 0;
				InvalidateRect(hWnd, NULL, true); // Generate paint message.

				delete[] pszFileDate;
				delete[] pszFileTime;
				delete[] pszFileSize;
				delete[] pszOpenFileName;

				// Restore original current directory.
				SetCurrentDirectory(szOldDirectoryName);

				// Set a 3 second timer to close the modeless dialog box.
				if (!bAbort) uiTimer = SetTimer(hWnd, 1, 3000, NULL);
				else         uiTimer = SetTimer(hWnd, 1, 30,   NULL);
			}
		break;

		case ID_FILE_MARK:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Mark duplicates by renaming them with ".DELETE" in the name before the extension
			/////////////////////////////////////////////////////////////////////////////////////////////////
		{
			if (bMarked)
			{
				MessageBox(hWnd, _T("Already marked. Rescan!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			if (pCHashedFiles->GetNodeCount() == 0)
			{
				MessageBox(hWnd, _T("Scan first, then mark!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			if (MessageBox(hWnd, _T("Are you sure you want to mark the duplicates for deletion?"),
				szTitle, MB_YESNO | MB_DEFBUTTON2) != IDYES) break;

			GetCurrentDirectory(MAX_PATH, szOldDirectoryName);
			SetCurrentDirectory(szDirectoryName);
			for (int i = 0; i < pCHashedFiles->GetNodeCount(); ++i)
			{
				BOOL dup;
				wstring hash, date, time, size, file;
				wstring line, base, ext, newfile;

				pCHashedFiles->GetNode(i, dup, hash, date, time, size, file); // Process each file
				if (!dup) continue; // Ignore non duplicates.

				size_t iDot = file.find_last_of(TCHAR('.')); // Find the extension
				if (iDot == wstring::npos) // case of no extension
				{
					newfile = file + _T(".DELETE"); // just add .DELETE to the end of the file name.
				}
				else // case of extension found
				{
					// Add .DELETE before the extension, i.e. "base.DELETE.ext"
					base = file.substr(0, iDot);
					ext = file.substr(iDot);
					newfile = base + _T(".DELETE") + ext;
				}

				// Rename file.
				MoveFile(file.c_str(), newfile.c_str());
			}
			bMarked = true;
			MessageBox(hWnd, _T("Mark completed. Rescan to see results."), szTitle, MB_OK);
			SetCurrentDirectory(szOldDirectoryName);
			break;
		}

		case ID_FILE_SAVE:
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// Saves the class.
		/////////////////////////////////////////////////////////////////////////////////////////////////

			if (pCHashedFiles->GetNodeCount() == 0) // Case of no files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then save!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			pCHashedFiles->Save(hWnd, iStartNode, iSelectedFile, iSortMode, szDirectoryName);
			break;

		case ID_FILE_LOAD:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Loads the class.
			/////////////////////////////////////////////////////////////////////////////////////////////////

			if (pCHashedFiles->GetNodeCount() > 0) // Case of some files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				if (MessageBox(hWnd, _T("Are you sure you want to load a saved class?"),
					szTitle, MB_YESNO | MB_DEFBUTTON2) != IDYES) break;
			}
			pCHashedFiles->Reset();
			pCHashedFiles->Load(hWnd, iStartNode, iSelectedFile, iSortMode, szDirectoryName);
			InvalidateRect(hWnd, NULL, true); // Generate paint message.
			break;

		case ID_SORT_BYHASH:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Re-sorts by file hash, then by file name. Changes spacing to double.
			/////////////////////////////////////////////////////////////////////////////////////////////////
		{
			if (pCHashedFiles->GetNodeCount() == 0) // Case of no files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then re-sort!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			iSortMode = 0; // Flag for painting and scrolling.
			pCHashedFiles->SortAndCheck(iSortMode);

			iStartNode = 0; // Start paint from the first entry in the list.
			InvalidateRect(hWnd, NULL, true); // Generate paint message.
			break;
		}

		case ID_SORT_BYFILE:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Re-sorts by file name alone. Changes spacing to single.
			/////////////////////////////////////////////////////////////////////////////////////////////////
		{
			if (pCHashedFiles->GetNodeCount() == 0) // Case of no files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then resort!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			iSortMode = 1; // Flag for painting and scrolling.
			pCHashedFiles->SortAndCheck(iSortMode);

			iStartNode = 0; // Start paint from the first entry in the list.
			InvalidateRect(hWnd, NULL, true); // Generate paint message.
			break;
		}

		case ID_SORT_BYDATE:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Re-sorts by file date, then by file time, then by file name. Changes spacing to single.
			/////////////////////////////////////////////////////////////////////////////////////////////////
		{
			if (pCHashedFiles->GetNodeCount() == 0) // Case of no files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then resort!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			iSortMode = 2; // Flag for painting and scrolling.
			pCHashedFiles->SortAndCheck(iSortMode);

			iStartNode = 0; // Start paint from the first entry in the list.
			InvalidateRect(hWnd, NULL, true); // Generate paint message.
			break;
		}

		case ID_SORT_BYSIZE:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			// Re-sorts by file size, then by file name. Changes spacing to single.
			/////////////////////////////////////////////////////////////////////////////////////////////////
		{
			if (pCHashedFiles->GetNodeCount() == 0) // Case of no files to process.
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then resort!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}
			iSortMode = 3; // Flag for painting and scrolling.
			pCHashedFiles->SortAndCheck(iSortMode);

			iStartNode = 0; // Start paint from the first entry in the list.
			InvalidateRect(hWnd, NULL, true); // Generate paint message.
			break;
		}

		case ID_EDIT_COPY: // (Plus accelerator <Ctrl>C) - Copies the selected FileName to the clipboard.
		{
			if (pCHashedFiles->GetNodeCount() == 0)
			{
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, _T("Scan first, then Copy!"), szTitle, MB_OK | MB_ICONEXCLAMATION);
				break;
			}

			pCHashedFiles->GetFile(iSelectedFile, *pDblClickFile);

			HGLOBAL hText = GlobalAlloc
				(GMEM_FIXED, lstrlen(pDblClickFile->c_str()) * sizeof(TCHAR) + sizeof(TCHAR));
			if (hText == NULL)
			{
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}
			wcscpy_s((TCHAR*)hText, wcslen(pDblClickFile->c_str()) + 1, pDblClickFile->c_str());

			if (!OpenClipboard(hWnd))
			{
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}
			if (!EmptyClipboard())
			{
				CloseClipboard();
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}
			if (!SetClipboardData(CF_UNICODETEXT, hText))
			{
				GlobalFree(hText);
				CloseClipboard();
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}
			CloseClipboard();
		}
		break;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		case IDM_ABOUT:
			/////////////////////////////////////////////////////////////////////////////////////////////////
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		delete[] pszMessageDigest;
		delete[] pszMessageSummary;
		delete[] pszMessageFinal;
	}
	break;

	case WM_MOUSEWHEEL: // Scroll down/up.
	
		// Get mouse wheel scroll increment, defaulting to 3
		UINT uScroll;
		if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &uScroll, 0)) uScroll = 3;
		
		// Adjust the Starting Node by the scroll amount
		iStartNode -= GET_WHEEL_DELTA_WPARAM(wParam) / 120 * uScroll;
		iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);

		dc = GetDC(hWnd);
		GetTextMetrics(dc, &tm);
		GetClientRect(hWnd, &rect);
		ReleaseDC(hWnd, dc);
		si.nMax = pCHashedFiles->GetNodeCount() - 1;
		si.nMin = 0;
		si.nPage = rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
		si.nPos = iStartNode;
		SetScrollInfo(hWnd, SB_VERT, &si, true);
		InvalidateRect(hWnd, NULL, true);
		break;

	// 5 second timer to close the progress box.
	case WM_TIMER:
		KillTimer(hWnd, uiTimer);
		ShowWindow(hWndProgressBox, SW_HIDE);
		break;

	// keyboard messages
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_DOWN: // Move the selection bar down one line and scroll if at bottom of page.

			if (iSelectedFile == pCHashedFiles->GetNodeCount() - 1) break; // Case of end of data.
			
			++iSelectedFile;
			if (iSelectedFile >= iStartNode + pCOpenFiles->GetFileCount() - 1) // Case of end of page.
			{
				// Scroll down one line.
				dc = GetDC(hWnd);
				GetTextMetrics(dc, &tm);
				GetClientRect(hWnd, &rect);
				ReleaseDC(hWnd, dc);
				++iStartNode;
				iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
				si.nMax = pCHashedFiles->GetNodeCount() - 1;
				si.nMin = 0;
				si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
				si.nPos = iStartNode;
				SetScrollInfo(hWnd, SB_VERT, &si, true);
			}
			
			InvalidateRect(hWnd, NULL, true); // Repaint.
			break;

		case VK_UP: // Move the selection bar up one line and scroll if at top.

			if (iSelectedFile == 0) break; // Case of beginning of data.
			--iSelectedFile;
			if (iSelectedFile < iStartNode) // Case of beginning of page.
			{
				// Scroll up one line.
				dc = GetDC(hWnd);
				GetTextMetrics(dc, &tm);
				GetClientRect(hWnd, &rect);
				ReleaseDC(hWnd, dc);
				--iStartNode;
				iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
				si.nMax = pCHashedFiles->GetNodeCount() - 1;
				si.nMin = 0;
				si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
				si.nPos = iStartNode;
				SetScrollInfo(hWnd, SB_VERT, &si, true);
			}
			InvalidateRect(hWnd, NULL, true); // Repaint.
			break;

		case VK_NEXT: // Scroll down one page. Selection bar goes to top.

			dc = GetDC(hWnd);
			GetTextMetrics(dc, &tm);
			GetClientRect(hWnd, &rect);
			ReleaseDC(hWnd, dc);
			iStartNode += rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1) - 2;
			iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
			iSelectedFile = iStartNode;
			si.nMax = pCHashedFiles->GetNodeCount() - 1;
			si.nMin = 0;
			si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			si.nPos = iStartNode;
			SetScrollInfo(hWnd, SB_VERT, &si, true);
			InvalidateRect(hWnd, NULL, true);
			break;

		case VK_PRIOR: // Scroll up one page. Selection bar goes to top.

			dc = GetDC(hWnd);
			GetTextMetrics(dc, &tm);
			GetClientRect(hWnd, &rect);
			ReleaseDC(hWnd, dc);
			iStartNode -= rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
			iSelectedFile = iStartNode;
			si.nMin = 0;
			si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			si.nPos = iStartNode;
			SetScrollInfo(hWnd, SB_VERT, &si, true);
			InvalidateRect(hWnd, NULL, true);
			break;
		
		case 'X': // X - Set Duplicate on the selected file.
			pCHashedFiles->SetDuplicate(iSelectedFile, true);
			InvalidateRect(hWnd, NULL, true);
			break;

		case 'O': // O - Clear Duplicate on the selected file.
			pCHashedFiles->SetDuplicate(iSelectedFile, false);
			InvalidateRect(hWnd, NULL, true);
			break;

		case VK_RETURN: // The selected file is executed with the shell open verb.
		case VK_SPACE:  // Space or Return will do it.
			if (pCHashedFiles->GetFile(iSelectedFile, *pDblClickFile))
			{
				GetCurrentDirectory(MAX_PATH, szOldDirectoryName);
				SetCurrentDirectory(szDirectoryName);
				HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				UNREFERENCED_PARAMETER(hr);
				HINSTANCE hi;
				hi = ShellExecute(hWnd, _T("open"), pDblClickFile->c_str(), NULL, NULL, SW_SHOWNORMAL);
				CoUninitialize();
				SetCurrentDirectory(szOldDirectoryName);
				if (hi <= (HINSTANCE)32)
				{
					LPVOID lpMsgBuf;
					LPVOID lpDisplayBuf;

					FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM |
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, (DWORD)(uint64_t)hi,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPTSTR)&lpMsgBuf, 0, NULL);

					lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
						(lstrlen((LPCTSTR)lpMsgBuf) + 64) * sizeof(TCHAR) +
						lstrlen((LPCTSTR)pDblClickFile));
					StringCchPrintf((LPTSTR)lpDisplayBuf,
						LocalSize(lpDisplayBuf) / sizeof(TCHAR),
						_T("Error: %d, %s\nEncountered while opening\nfile: %s!"),
						(int)(uint64_t)hi, (LPTSTR)lpMsgBuf, pDblClickFile->c_str());
					MessageBeep(MB_ICONEXCLAMATION);
					MessageBox(hWnd, (LPCTSTR)lpDisplayBuf, szTitle, MB_OK | MB_ICONEXCLAMATION);

					LocalFree(lpMsgBuf);
					LocalFree(lpDisplayBuf);
				}
			}
			break;

		case VK_CONTROL: //Don't beep for <Ctrl>C
			break;

		default: // Beep for any other keydown.
			MessageBeep(MB_ICONASTERISK);
			break;
		}
		break;
	
	// Scroll bar messages.

	case WM_VSCROLL:

		// Get all the vertical scroll bar information.
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
		GetScrollInfo(hWnd, SB_VERT, &si);

		switch (LOWORD(wParam))
		{

		case SB_LINEUP: // User clicked the top arrow. Move/scroll the selection bar up.

			if (iSelectedFile == 0) break;
			--iSelectedFile;
			if (iSelectedFile < iStartNode)
			{
				dc = GetDC(hWnd);
				GetTextMetrics(dc, &tm);
				GetClientRect(hWnd, &rect);
				ReleaseDC(hWnd, dc);
				--iStartNode;
				iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
				si.nMax = pCHashedFiles->GetNodeCount() - 1;
				si.nMin = 0;
				si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
				si.nPos = iStartNode;
				SetScrollInfo(hWnd, SB_VERT, &si, true);
			}
			InvalidateRect(hWnd, NULL, true);
			break;

		case SB_LINEDOWN: // User clicked the bottom arrow. Move/scroll the selection bar down.

			if (iSelectedFile == pCHashedFiles->GetNodeCount() - 1) break; // Case of end of data.

			++iSelectedFile;
			if (iSelectedFile >= iStartNode + pCOpenFiles->GetFileCount() - 1) // Case of end of page.
			{
				dc = GetDC(hWnd);
				GetTextMetrics(dc, &tm);
				GetClientRect(hWnd, &rect);
				ReleaseDC(hWnd, dc);
				++iStartNode;
				iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
				si.nMax = pCHashedFiles->GetNodeCount() - 1;
				si.nMin = 0;
				si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
				si.nPos = iStartNode;
				SetScrollInfo(hWnd, SB_VERT, &si, true);
			}
			InvalidateRect(hWnd, NULL, true);
			break;

		case SB_PAGEUP: // User clicked the scroll bar shaft above the scroll box.

			dc = GetDC(hWnd);
			GetTextMetrics(dc, &tm);
			GetClientRect(hWnd, &rect);
			ReleaseDC(hWnd, dc);
			iStartNode -= rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1) - 2;
			iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
			iSelectedFile = iStartNode;
			si.nMin = 0;
			si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			si.nPos = iStartNode;
			SetScrollInfo(hWnd, SB_VERT, &si, true);
			InvalidateRect(hWnd, NULL, true);
			break;

		case SB_PAGEDOWN: // User clicked the scroll bar shaft below the scroll box.

			dc = GetDC(hWnd);
			GetTextMetrics(dc, &tm);
			GetClientRect(hWnd, &rect);
			ReleaseDC(hWnd, dc);
			iStartNode += rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1) - 2;
			iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
			iSelectedFile = iStartNode;
			si.nMax = pCHashedFiles->GetNodeCount() - 1;
			si.nMin = 0;
			si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			si.nPos = iStartNode;
			SetScrollInfo(hWnd, SB_VERT, &si, true);
			InvalidateRect(hWnd, NULL, true);
			break;

		case SB_THUMBTRACK: // User dragged the scroll box.

			dc = GetDC(hWnd);
			GetTextMetrics(dc, &tm);
			GetClientRect(hWnd, &rect);
			ReleaseDC(hWnd, dc);
			iStartNode = si.nTrackPos;
			iStartNode = max(min(iStartNode, pCHashedFiles->GetNodeCount() - 1), 0);
			iSelectedFile = iStartNode;
			si.nMax = pCHashedFiles->GetNodeCount() - 1;
			si.nMin = 0;
			si.nPage = (rect.bottom - tm.tmHeight * 2) / tm.tmHeight / (iSortMode == 0 ? 2 : 1);
			si.nPos = iStartNode;
			SetScrollInfo(hWnd, SB_VERT, &si, true);
			InvalidateRect(hWnd, NULL, true);
			break;

		default:

			break;
		}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		HPEN hpenNew;
		HGDIOBJ hpenOld;

		GetClientRect(hWnd, &rect);

		if (bChooseFont) // A user specified font is in effect.
		{
			hOldFont = (HFONT)SelectObject(hdc, CreateFontIndirect(&sLogFont));
			SetTextColor(hdc, sChooseFont.rgbColors);
		}

		GetTextMetrics(hdc, &tm);

		int y = 10 + tm.tmHeight - tm.tmHeight * (iSortMode == 0 ? 2 : 1);
		BOOL dup;
		wstring hash, date, time, size, file, line;
		pCOpenFiles->Reset();

		iSelectedFile = max(iSelectedFile, iStartNode);

		// Write header line if nodes to process.
		if (pCHashedFiles->GetNodeCount() > 0)
		{
			wstring header;
			header += _T("SHA-1 Digest-----------------------------------------------   Date------   Time-   -----Size   D   ");
			header += _T("File Name------------------------------------------------------------------------------------------");
			SetBkColor(hdc, RGB(191, 255, 191));
			TextOut(hdc, 10, 10, header.c_str(), (int)header.length());

			// Draw a grey box around the header line
			hpenNew = CreatePen(PS_SOLID, 1, RGB(127, 127, 127));
			hpenOld = SelectObject(hdc, hpenNew);
			MoveToEx(hdc, 10,             10,             NULL);
			LineTo  (hdc, rect.right - 1, 10                  );
			LineTo  (hdc, rect.right - 1, 10 + tm.tmHeight - 1);
			LineTo  (hdc, 10,             10 + tm.tmHeight - 1);
			LineTo  (hdc, 10,             10                  );
			SelectObject(hdc, hpenOld);
			DeleteObject(hpenNew);
		}

		// Process nodes for displpay.
		for (int i = iStartNode; i < pCHashedFiles->GetNodeCount(); ++i)
		{
			pCHashedFiles->GetNode(i, dup, hash, date, time, size, file); // Get data for each file.
			
			line = hash + wstring(_T("   ")) +
				   date + wstring(_T("   ")) +
				   time + wstring(_T("   ")) +
				   size + wstring(_T("   ")) + wstring(dup ? _T("X   ") : _T("O   ")) + file;

			// Space or double space depending on the re-sort flag.
			y += dup ? tm.tmHeight : tm.tmHeight * (iSortMode  == 0 ? 2 : 1);
			
			if (y > rect.bottom - tm.tmHeight) break; // Stop painting if at the bottom of the client window.

			// Adjust selected file for scrolling down with the mouse wheel.
			if (y >= rect.bottom - tm.tmHeight - tm.tmHeight * (iSortMode == 0 ? 2 : 1)) iSelectedFile = min(iSelectedFile, i);

			// Highlight duplicates.
			SetBkColor(hdc, dup ? RGB(255, 191, 191) : RGB(255, 255, 255));
			
			// Write the line to the window.
			TextOut(hdc, 10, y, line.c_str(), (int)line.length());

			// Draw a grey box around the selected node.
			if (i == iSelectedFile)
			{
				hpenNew = CreatePen(PS_SOLID, 1, RGB(127, 127, 127));
				hpenOld = SelectObject(hdc, hpenNew);

				MoveToEx(hdc, 10,             y,                 NULL);
				LineTo  (hdc, rect.right - 1, y                  );
				LineTo  (hdc, rect.right - 1, y + tm.tmHeight - 1);
				LineTo  (hdc, 10,             y + tm.tmHeight - 1);
				LineTo  (hdc, 10,             y                  );

				SelectObject(hdc, hpenOld);
				DeleteObject(hpenNew);
			}

			// Save the Y limits and Index to the OpenFiles class.
			pCOpenFiles->AddFile(y, y + tm.tmHeight, file, i);
		}

		// Build the status bar if nodes processed.
		if (pCHashedFiles->GetNodeCount() > 0)
		{
			RECT rectFill;
			rectFill.left = 0;
			rectFill.right = rect.right;
			rectFill.top = rect.bottom - tm.tmHeight;
			rectFill.bottom = rect.bottom;

			LOGBRUSH LogBrush;
			LogBrush.lbColor = RGB(0, 0, 255);
			LogBrush.lbStyle = BS_SOLID;
			LogBrush.lbHatch = NULL;
			HBRUSH hbrNew = CreateBrushIndirect(&LogBrush);

			FillRect(hdc, &rectFill, hbrNew);

			int iDups = 0;
			for (int i = 0; i < pCHashedFiles->GetNodeCount(); ++i)
			{
				pCHashedFiles->GetNode(i, dup);
				iDups += dup ? 1 : 0;
			}
			const TCHAR* pszSortByText[] = {
				_T("SHA-1 Digest, then by File Name"  ),
				_T("File Name, alone"                 ),
				_T("File Date/Time, then by File Name"),
				_T("File Size, then by File Name"     )
			};

			#define	MAX_STATUS_LEN (MAX_PATH + 181 + 4 + 4 + 4 + 33 + 1)
			TCHAR* pszStatus = new TCHAR[MAX_STATUS_LEN];
			StringCchPrintf(pszStatus, MAX_STATUS_LEN,

				_T("Directory: %s     Files: %d     MBytes: %llu     Duplicates: %d     Sorted by: %s"),
				//  12345678901  234567890123  4567890123456    78901234567890123  4567890123456789

				szDirectoryName, pCHashedFiles->GetNodeCount(), BytesProcessed/1024/1024,
				iDups, pszSortByText[iSortMode]);

			// Draw the status bar.
			SetBkColor(hdc, RGB(0, 0, 255));
			COLORREF rgbOld = GetTextColor(hdc);
			SetTextColor(hdc, RGB(255, 255, 255));
			TextOut(hdc, 10, rect.bottom - tm.tmHeight, pszStatus, lstrlen(pszStatus));

			// Draw a grey box around the status bar.
			hpenNew = CreatePen(PS_SOLID, 1, RGB(127, 127, 127));
			hpenOld = SelectObject(hdc, hpenNew);
			MoveToEx(hdc, 10,             rect.bottom - tm.tmHeight, NULL);
			LineTo  (hdc, rect.right - 1, rect.bottom - tm.tmHeight      );
			LineTo  (hdc, rect.right - 1, rect.bottom - 1                );
			LineTo  (hdc, 10,             rect.bottom - 1                );
			LineTo  (hdc, 10,             rect.bottom - tm.tmHeight      );
			SelectObject(hdc, hpenOld);
			DeleteObject(hpenNew);
			SetTextColor(hdc, rgbOld);
			delete[] pszStatus;
		}

		// Show or hide the scroll bar depending on the number of files to display.
		ShowScrollBar(hWnd, SB_VERT,
			pCHashedFiles->GetNodeCount() < rect.bottom / tm.tmHeight / (iSortMode == 0 ? 2 : 1) ? false : true);

		if (bChooseFont) SelectObject(hdc, hOldFont);  // Restore original font in the DC.

		EndPaint(hWnd, &ps);
	}
	break;

	// Select node.
	case WM_LBUTTONDOWN:
		if (pCOpenFiles->GetFile(GET_Y_LPARAM(lParam), *pDblClickFile, iNode))
		{
			iSelectedFile = iNode;
			InvalidateRect(hWnd, NULL, true);
			break;
		}

	// Invert Duplicate.
	case WM_RBUTTONDOWN:
		BOOL Duplicate;
		if (pCOpenFiles->GetFile(GET_Y_LPARAM(lParam), *pDblClickFile, iNode))
		{
			pCHashedFiles->GetNode(iNode, Duplicate);
			pCHashedFiles->SetDuplicate(iNode, !Duplicate);
			InvalidateRect(hWnd, NULL, true);
			break;
		}

	// Launch file.
	case WM_LBUTTONDBLCLK:
		if (pCOpenFiles->GetFile(GET_Y_LPARAM(lParam), *pDblClickFile, iNode))
		{
			GetCurrentDirectory(MAX_PATH, szOldDirectoryName);
			SetCurrentDirectory(szDirectoryName);
			HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			(hr); // Ignored
			HINSTANCE hi;
			hi = ShellExecute(hWnd, _T("open"), pDblClickFile->c_str(), NULL, NULL, SW_SHOWNORMAL);
			CoUninitialize();
			SetCurrentDirectory(szOldDirectoryName);
			if (hi <= (HINSTANCE)32)
			{
				LPVOID lpMsgBuf;
				LPVOID lpDisplayBuf;

				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				              FORMAT_MESSAGE_FROM_SYSTEM |
				              FORMAT_MESSAGE_IGNORE_INSERTS,
				              NULL, (DWORD)(uint64_t)hi,
				              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				              (LPTSTR)&lpMsgBuf, 0, NULL);

				lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
					(lstrlen((LPCTSTR)lpMsgBuf) + 64) * sizeof(TCHAR) +
					lstrlen((LPCTSTR)pDblClickFile));
				StringCchPrintf((LPTSTR)lpDisplayBuf,
					LocalSize(lpDisplayBuf) / sizeof(TCHAR),
					_T("Error: %d, %s\nEncountered while opening\nfile: %s!"),
					(int)(uint64_t)hi, (LPTSTR)lpMsgBuf, pDblClickFile->c_str());
				MessageBeep(MB_ICONEXCLAMATION);
				MessageBox(hWnd, (LPCTSTR)lpDisplayBuf, szTitle, MB_OK | MB_ICONEXCLAMATION);

				LocalFree(lpMsgBuf);
				LocalFree(lpDisplayBuf);
			}
		}
		break;

	case WM_DESTROY:
		// Save window placement to the registry
		 {
			WINDOWPLACEMENT wp;
			ZeroMemory(&wp, sizeof(wp));
			wp.length = sizeof(wp);
			GetWindowPlacement(hWnd, &wp);
			if (pAppReg->Init(hWnd)) // Only executes the first time. See Init().
				pAppReg->SaveMemoryBlock(_T("WindowPlacement"), (LPBYTE)&wp, sizeof(wp));
		}
		
		DestroyWindow(hWndProgressBox);
		PostQuitMessage(0);
		break;

	case WM_CREATE:
		// Create the modeless dialog box - We will display it later in the process procedure
		hWndProgressBox = CreateDialog(hInst, MAKEINTRESOURCE(IDD_PROGRESS_BOX), hWnd, (DLGPROC)MDBoxProc);
		if (hWndProgressBox == NULL)
		{
			MessageBeep(MB_ICONEXCLAMATION);
			MessageBox(hWnd, _T("CreateDialog returned NULL"),
			                 _T("Warning!"), MB_OK | MB_ICONEXCLAMATION);
		}
		break;

	default:
		delete pAppReg;
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	delete pAppReg;

	return 0;
}



// Worker thread for processing the hashes of the files
DWORD WINAPI FileHashWorkerThread(LPVOID lpParam)
{
	// This is a copy of the structure from the command procedure.
	// The address of this structure is passed with lParam.
	typedef struct ThreadProcParameters
	{
		HANDLE       hcsMutex;
		BOOL* pbAbort;
		HashedFiles* pcsHashedFiles;
	} THREADPROCPARAMETERS, * PTHREADPROCPARAMETERS;
	PTHREADPROCPARAMETERS P;
	P = (PTHREADPROCPARAMETERS)lpParam;

	int Node;
	wstring FileName, FileHash;
	sha1file Sha1File;

	// Loop until no more work to do.
	for (;;)
	{
		if (*(P->pbAbort)) return 0; // Case of user pressed ESCAPE

		// Retrieve the next FileName from the NodeList.
		WaitForSingleObject(P->hcsMutex, INFINITE);
			BOOL bWorkToDo = P->pcsHashedFiles->GetNextFile(Node, FileName);
		ReleaseMutex(P->hcsMutex);
		if (!bWorkToDo) return 0;
		
		// Generate hash and save.
		Sha1File.Process(FileName.c_str(), 0, (TCHAR*)(FileHash.c_str()), NULL);
		P->pcsHashedFiles->SaveHash(Node, FileHash); // TEST
	}
}



// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for modeless dialog box.
INT_PTR CALLBACK MDBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(hDlg);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	switch (message)
	{
	case WM_INITDIALOG:
		return true;
	case WM_COMMAND:
		return true;
	}
	return false;
}
