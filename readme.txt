
XWorkplace Source README
(W) Ulrich M”ller, July 2, 1999
Last updated January 26, 2001, Ulrich M”ller


0. CONTENTS OF THIS FILE
========================

    1. LICENSE, COPYRIGHT, DISCLAIMER
    2. INTRODUCTION
    3. COMPILATION/INSTALLATION
    4. GETTING STARTED
    5. KNOWN LIMITATIONS
    6. CONTRIBUTORS


1. LICENSE, COPYRIGHT, DISCLAIMER
=================================

    Copyright (C) 1997-2001 Ulrich M”ller
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

    Plenty of programming information is contained in the PROGREF.INF
    file in the main directory of the source tree.

    In order to get to know XWorkplace's structure, you might first
    want to read the "XWorkplace Internals" section in the XWorkplace
    User Guide, which comes with the binary distribution already.

    The README for the latest binary release is in the "001\readme"
    file.


3. COMPILATION/INSTALLATION
===========================

    See PROGREF.INF for detailed instructions.

    If you only want to translate XWorkplace, you need no compiler.
    Otherwise, compiling requires IBM VAC++ 3.0, fixpak 8 installed,
    plus the OS/2 Developer's Toolkit for Warp 3. There's no way
    around that, so EMX won't work.

    The SRC\HELPERS and INCLUDE\HELPERS directories are no longer
    present in this source tree. Starting with V0.9.6, these have
    been exported to a separate CVS repository at Netlabs called
    "xwphelpers". See PROGREF.INF.


    Working With Netlabs CVS
    ------------------------

    Please see "cvs.txt" in this directory.


4. GETTING STARTED
==================

    See "001\readme" for this, which is the readme that comes with
    the binary release.

    For instructions how to use CVS, see the separate cvs.txt file.


5. KNOWN LIMITATIONS
====================

    See "001\readme" for this, which is the readme that comes with
    the binary release.


6. CONTRIBUTORS
===============

    Newer contributions (since about V0.9.1) are marked in xdoc
    style. The following author acronyms are used in the code:

        umoeller            Ulrich M”ller (djmutex@xworkplace.org)

        dk                  Dmitry Kubov <Dmitry@north.cs.msu.su>
        jsmall              John Small (jsmall@lynxus.com)
        lafaix              Martin Lafaix (lafaix@ibm.net)
        pr                  Paul Ratcliffe (paul@orac.clara.co.uk)

