Tools readme:

In order for the makefiles to work, copy these utilities
somewhere on your PATH.

---> Note: strrpl and html2ipf have bugs fixed with V0.84
     and V0.9.0.
     Please use the newer versions.
     html2ipf has been moved to 001/tools because it's
     really language-dependent.

Tools description:

--  BuildLevel.cmd (C) Roman Stangl has been added with V0.9.0
    and is used to write build  level information into module
    definition files.
    See EDM/2 5/06 for details.

--  copydbg.exe strips the debug code out of any executable (EXE
    or DLL) into a .DBG file with the same file stem. This is
    from the EXCEPT3 package available at hobbes. Source code is
    in tools\copydebug.

--  strrpl.exe has been written by me. It is for fast string search and
    replace in files and useful for updating the INF HTML sources to
    V0.83. This has been updated and bugfixed with V0.9.0.
    Start the program to have it explain itself.

--  unlock.exe is used by the XFolder makefiles to unlock the XFolder
    DLL currently locked by the WPS. This is taken from an old version
    of the lxLite package.
    This has been written by Andrew Pavel Zabolotny.

Ulrich M”ller

