XWorkplace 0.9.6 README
(W) Ulrich M”ller, July 2, 1999
Last updated October 7, 2000, Ulrich M”ller


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

    I strongly recommend to install the most current XWorkplace
    binary release before compiling this thing, since the makefiles
    were really designed to automatically update an existing
    installation.

    If you only want to translate XWorkplace, you need no compiler.
    See PROGREF.INF for instructions.

    Otherwise, compiling requires IBM VAC++ 3.0, fixpak 8 installed,
    plus the OS/2 Developer's Toolkit for Warp 3. There's no way
    around that, so EMX won't work. See PROGREF.INF for details.

    The SRC\HELPERS and INCLUDE\HELPERS directories are no longer
    present in this source tree. Instead, the code from the WarpIN
    project is used, which also resides on the Netlabs CVS source
    tree. As a result, you also need to check out the WarpIN source
    code before compiling XWorkplace.

    For all details, please see PROGREF.INF in the main directory.


    Getting Sources from Netlabs CVS
    --------------------------------

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
    compression during transfers. This greatly speeds up things.


4. GETTING STARTED
==================

    See "001\readme" for this.


5. KNOWN LIMITATIONS
====================

    See "001\readme" for this.


