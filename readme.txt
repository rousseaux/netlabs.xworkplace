XWorkplace 0.9.0 README
(W) Ulrich M”ller, July 2, 1999
Last updated Jan 04, 2000, Ulrich M”ller


0. CONTENTS OF THIS FILE
========================

    1. LICENSE, COPYRIGHT, DISCLAIMER
    2. INTRODUCTION
    3. INSTALLATION
    4. GETTING STARTED
    5. KNOWN LIMITATIONS


1. LICENSE, COPYRIGHT, DISCLAIMER
=================================

    Copyright (C) 1998-99 Ulrich M”ller.

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

    The user guide is still largely outdated, sorry.
    See 001\inf.001\vers_2history.html, which has the current revision
    history though.
    After compilation, this can be found in XFLDR001.INF, as usual.

    Programming information is contained in the PROGREF.INF file in
    the main directory of the source tree.


3. INSTALLATION
===============

    To install this thing, you need to build it first. This requires
    IBM VAC++ 3.0, fixpak 8 installed.

    The SRC\HELPERS and INCLUDE\HELPERS directories are no longer
    present in this source tree. Instead, the code from the WarpIN
    project is used, which also resides on the Netlabs CVS source
    tree. As a result, you need to check out the WarpIN source code
    before compiling XWorkplace.

    Here comes a quick-start guide; for details, please refer to
    PROGREF.INF.

    1)  You will need to set up a few environment variables before
        compilation will work. You can change the defaults in SETUP.IN
        or define the variables externally, before calling NMAKE.
        The latter is recommended.

        The following environment variables are required:

        -- XWPRUNNING must point to a valid XWorkplace directory
           tree. The makefiles will copy all produced output files
           to the proper locations in that tree so that you can
           work on XWorkplace while it is installed. DLLs which
           are in use will be unlocked. See PROGREF.INF for details.

           You can point XWPRUNNING to an existing XFolder 0.85
           installation tree. In that case, copy the files from
           RELEASE to that tree, because some have been updated.

           Alternatively, specify the RELEASE subtree in the source
           tree directly.

           Specify the parent directory of BIN for this, where
           XFLDR.DLL will reside.

        -- HELPERS_BASE must point to the root of the WarpIN source
           tree. Otherwise XWorkplace cannot compile.

           Specify the parent directory of SRC\HELPERS and
           INCLUDE\HELPERS for this.

    2)  Put the .EXEs from \TOOLS somewhere on your PATH.

    3)  Execute MAKE.CMD.

    4)  Run RELEASE\INSTALL.CMD to install XWorkplace. This is only
        a slightly modified new version of the ole XFolder install
        script. Future versions will be installed using WarpIN.

    Optionally, you can replace the WPDataFile class with the DbgDataFile
    class in BIN\XDEBUG.DLL, which is created by MAKE.CMD. See
    SRC\CLASSES\XDEBUG.C for details. This is only for debugging WPS
    objects in detail though and probably not something for everyday work.


4. GETTING STARTED
==================

    XWorkplace will take over your existing XFolder 0.85 settings,
    if present. However, many more settings have arrived, so many
    might not be initialized.

    In any case, open the new "XWorkplace Setup" object first and
    go to the "Features" page. This is the most central place to
    enable XWorkplace features. Refer to the "Help" button there.


5. KNOWN LIMITATIONS
====================

    This version is still buggy. I have it running on my system for
    everyday work, but some things are a bit annoying, so be warned.

    1)  If "Extended associations" is enabled in XWPSetup, the new
        "File types" page in "Workplace Shell" is working, but XFldDataFile
        doesn't support the enhanced file type icons yet.
        You'll see the proper icons and associations in the "Open"
        submenu, but clicking on them will not open the proper
        association.

    2)  A few settings checkboxes do not work yet or not any longer.
        This doesn't hurt, but doesn't do anything either.

    3)  The new trash displays wrong info sometimes and cannot delete
        objects which have the read-only attribute on.

        Also, the trash can presently hangs the WPS when objects in
        the trash can are destroyed. This applies to both destroying
        single objects as well as emptying the trash can.





