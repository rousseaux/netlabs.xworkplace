#
# Makefile for XWorkplace National Language Support.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
# Copyright (C) 1997-2000 Ulrich M”ller.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, in version 2 as it comes in the COPYING
# file of the XWorkplace main distribution.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# Required changes for translating the NLS DLL: three lines below.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
!include ..\..\setup.in

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)
!if [@echo       OUTPUTDIR is $(OUTPUTDIR)]
!endif

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

#
# Now set up a few environment variables.
#

# This is the language code for the language that the NLS DLL
# will support. Change this to a different language code, and
# the makefile will support your langauge.
LANGUAGE = 001

# Change this to the full path of the HTML2IPF utility.
# THIS HAS CHANGED WITH V0.84.
HTML2IPF = ..\tools\html2ipf.cmd

# Change this to the directory where the output file (INF)
# should be copied to.
!ifndef XWPRUNNING
!error The environment variable XWPRUNNING is not defined. Terminating.
!endif
COPYTO = $(XWPRUNNING)

# DO NOT CHANGE THE FOLLOWING

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize.
.SUFFIXES: .html .ipf .hlp .inf

# main target
all: $(COPYTO)\xfldr$(LANGUAGE).inf
    @echo ----- Leaving $(MAKEDIR)

#
# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
#

#
# Now define inference rules for the different file types.
#

$(COPYTO)\xfldr$(LANGUAGE).inf: $(OUTPUTDIR)\xfldr$(LANGUAGE).inf
        cmd.exe /c copy $(OUTPUTDIR)\$(@B).inf $(COPYTO)

xfldr$(LANGUAGE).ipf: *.html *.gif makefile
        $(HTML2IPF) xfldr$(LANGUAGE).html

$(OUTPUTDIR)\xfldr$(LANGUAGE).inf: xfldr$(LANGUAGE).ipf
        ipfc xfldr001.ipf $(OUTPUTDIR)\xfldr$(LANGUAGE).inf


