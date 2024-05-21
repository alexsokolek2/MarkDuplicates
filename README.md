MarkDuplicates.cpp : Defines the entry point for the application.

MarkDuplicates scans a user specified directory, gathering filenames,
write times, and sizes, along with the generated SHA-1 message digest.
This information is put into a class (a list of nodes). It is sorted
by message digest and then by file name. The result is that identical
files are grouped together. Duplicates are flagged and made available
or marking, which renames the files as "base.DELETE.ext". The user can
then look at the directory with explorer and select all of the marked
files for deletion. This solves the problem created when a new laptop
was acquired and OneDrive was accidentally told to upload multiple,
duplicate copies of the files. The user can override the duplicate
status of each file with X (for duplicate) and O (for non duplicate).
While looking at the list of files, the user can examine a file by
pressing Enter, Space or double clicking, using a Shell Open process.

In addition to this marking, the user can initiate a test sequence
which tests the SHA-1 implementation with the four tests described
in RFC-3174, along with a fifth test of my own, a large PDF file.
 
The user can change the font and color of the display, and that
will be saved to the registry. Initially, the columns of the display
do not line up, but changing to a fixed pitch font, such as Courier
will fix that. I like Courier Bold 12 Green best.

Saves and restores the window placement in the registry at
HKCU/Software/Alex Sokolek/Mark Duplicates/1.0.0.1/WindowPlacement.
 
Provision is made for saving and restoring the current node list,
along with the sort mode and scroll position and the selected file
so that review (which can be lengthy) can continue later.
 
Demonstrates using a class to wrap a set of C functions implementing
the SHA-1 Secure Message Digest algorithm described in RFC-3174.

Uses a thread pool of 12 threads to process the hashes.The machine
used for development and testing has 12 logical processors, hence
the choice of 12 threads.This will still work on a machine that
has fewer processors - It will just be slower - To take advantage
of a machine with more processors, see line 483, "int Threads = 12".
  
It is suggested that a backup copy of the directory in question be
made, in case something goes wrong.
 
Norton 360 trips up when Mark is performed. Apparantly renaming a
lot of files is considiered suspicious behavior. Make sure that the
executable is on the Excluded List. (I put the entire VS directory
source tree on the Excluded List.
 
Compilation requires that UNICODE be defined. Some of the choices
made in code, mainly wstring, do not support detecting UNICODE vs
non-UNICODE, so don't compile without UNICODE defined.

In case you get linker errors, be sure to include version.lib
in the linker input line.
 
Microsoft Visual Studio 2022 Community Edition 64 Bit 17.9.6

Alex Sokolek, Version 1.0.0.1, Copyright (c) March 29, 2024

Version 1.0.0.2, April 10, 2024, Corrected PeekMessage() error.

Version 1.0.0.3, April 24, 2024, Updated version for release.

Version 1.0.0.4, May 14, 2024, Fixed saving/loading of selected directory.

Version 1.0.0.5, May 19, 2024, Added support for a thread pool.

Version 1.0.0.6, May 21, 2024, Updated version for release. Comment changes only.
