XWorkplace 0.9.4 README
(W) Ulrich M”ller, July 2, 1999
Last updated August 9, 2000, Ulrich M”ller


0. CONTENTS OF THIS FILE
========================

    1. LICENSE, COPYRIGHT, DISCLAIMER
    2. INTRODUCTION
    3. COMPILATION/INSTALLATION
    4. GETTING STARTED
    5. KNOWN LIMITATIONS


1. LICENSE, COPYRIGHT, DISCLAIMER
=================================

    Copyright (C) 1997-2000 Ulrich M”ller,
                            Christian Langanke,
                            and others (see the individual source files).

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as contained in
    the file COPYING in this distribution.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


2. INTRODUCTION
===============

    Welcome to the XWorkplace source code. As this thing is complex,
    I have tried to add plenty of documentation.

    In order to get to know XWorkplace's structure, you might first
    want to read the "XWorkplace Internals" section in the XWorkplace
    User Guide, which comes with the binary distribution already.

    The most current release information (for developer versions)
    is in the "001\readme" file.

    Programming information is contained in the PROGREF.INF file in
    the main directory of the source tree.


3. COMPILATION/INSTALLATION
===========================

    To install this thing, you need to build it first. This requires
    IBM VAC++ 3.0, fixpak 8 installed.

    The SRC\HELPERS and INCLUDE\HELPERS directories are no longer
    present in this source tree. Instead, the code from the WarpIN
    project is used, which also resides on the Netlabs CVS source
    tree. As a result, you also need to check out the WarpIN source
    code before compiling XWorkplace.

    First, do a "cvs login" with "readonly" as your password.

    Then, to check out the most current XWorkplace sources, create a
    subdirectory in your CVS root dir called "xworkplace".
    Then use:
        CVSROOT=:pserver:guest@www.netlabs.org:d:/netlabs.src/xworkplace
        USER=guest
    and do a "cvs checkout" from the "xworkplace" subdirectory.

    To check out the most current WarpIN sources, create a
    subdirectory in your CVS root dir called "warpin".
    Then use:
        CVSROOT=:pserver:guest@www.netlabs.org:d:/netlabs.src/warpin
        USER=guest
    and do a "cvs checkout" from the "warpin" subdirectory.

    Alternatively, use the Netlabs Open Source Archive Client (NOSAC).
    See http://www.netlabs.org/nosa for details.

    In any case, I strongly recommend to create a file in $(HOME)
    called ".cvsrc" and add "cvs -z9" in there to enable maximum
    compression during transfers.

    Here comes a quick-start guide; for details, please refer to
    PROGREF.INF.

    1)  You will need to set up a few environment variables before
        compilation will work. You can change the defaults in SETUP.IN
        or define the variables externally, before calling NMAKE.
        The latter is recommended.

        The following environment variables are required:

        -- XWPRUNNING must point to a valid XWorkplace installation
           tree. The makefiles will copy all produced output files
           to the proper locations in that tree so that you can
           work on XWorkplace while it is installed. DLLs which
           are in use will be unlocked.

           Do one of the following:

           -- Install the latest XWorkplace binary WarpIN archive.
              Set XWPRUNNING to the same directory which you
              specified as the target path to WarpIN.

           -- Alternatively, copy the entire "release" subtree to
              some new location and specify that location with
              XWPRUNNING to start.

              With XWPRUNNING, specify the parent directory of BIN,
              where XFLDR.DLL shall reside, like this:

                SET XWPRUNNING=J:\Tools\XWorkplace

        -- PROJECT_BASE_DIR must point to the root of the
           XWorkplace source tree. Otherwise the makefiles will
           complain.

           Specify the directory where this README.TXT file
           resides, like this:

                SET PROJECT_BASE_DIR=K:\cvs\xworkplace

        -- HELPERS_BASE must point to the root of the WarpIN
           source tree to be able to switch to the helpers code.
           Specify the parent directory of INCLUDE\HELPERS and
           SRC\HELPERS, like this:

                SET HELPERS_BASE=K:\cvs\warpin

        -- XWPRELEASE is used with "nmake release" to build a
           separate target tree which holds all files for a
           release version. That tree can then easily be "zipped"
           into a WarpIN archive (.WPI).

           This directory will hold a complete XWorkplace working
           set from where XWorkplace can be installed.
           That directory will be created by "nmake release"
           if if doesn't exist.

           Even if you don't plan to use "nmake release", you
           must set XWPRELEASE to something, or the makefiles
           will fail, like this:

                SET XWPRELEASE=dummy

        -- SET XWP_DEBUG=YES enables debug code. By contrast,
           if XWP_DEBUG is not defined, release code is produced.

    2)  Put the .EXE and .CMD files from \TOOLS somewhere on your
        PATH. These are used by the makefiles. Or add the tools
        directory to your path.

    3)  Execute "nmake" in the main source directory (where this
        README.TXT resides).

        If you don't have XWorkplace or XFolder installed yet,
        run "nmake release" instead. This will create a complete
        XWorkplace working set. (See XWPRELEASE above.)
        Run "install.cmd" in that directory to have XWorkplace
        installed then.

    Only if you want, you can replace the WPDataFile class with the DbgDataFile
    class in BIN\XDEBUG.DLL, which is created by the makefiles also. See
    SRC\CLASSES\XDEBUG.C for details. This is only for debugging WPS
    objects in detail though and probably not something for everyday work.


4. GETTING STARTED
==================

    See "001\readme" for this.


5. KNOWN LIMITATIONS
====================

    See "001\readme" for this.


