#
# Makefile for XFolder National Language Support.
#
# A makefile contains information about how several source files
# relate to each other, especially which code modules must be updated
# when certain source files change.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
# This makefile assumes that certain toolkit tools are on the PATH.
# In addition, you must put the "unlock" utility from the "tools"
# directory on your PATH, or this will fail.
#
# Copyright (C) 1997-99 Ulrich M”ller.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, in version 2 as it comes in the COPYING
# file of the XFolder main distribution.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# Required changes for translating the NLS DLL: three lines below.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

PROJECT_BASE_DIR = $(MAKEDIR)\..\..

# include setup (compiler options etc.)
!include ..\..\setup.in

# This is the language code for the language that the NLS DLL
# will support. Change this to a different language code, and
# the makefile will support your langauge.
LANGUAGE = 001

#
# DO NOT CHANGE THE FOLLOWING
#

#
# Set up a few environment variables.
#

OUTPUTDIR = ..\..\bin

OBJS = $(OUTPUTDIR)\xfldr$(LANGUAGE).obj

XFLDHELP = $(XWPRUNNING)\help

#
# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize.
#

# V1.00: added .tmf
.SUFFIXES: .txt .c .obj .dll .rc .res .html .ipf .hlp .inf

all: $(XWPRUNNING)\bin\xfldr$(LANGUAGE).dll
    @echo ----- Leaving $(MAKEDIR)

#
# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
#

.c{$(OUTPUTDIR)}.obj:
        $(CC_DLL) -I$(INCLUDE) -c /Fo$(OUTPUTDIR)\$(@B).obj $<

#
# Now define inference rules for the different file types.
#

# V1.00: the following has been removed
# xfldr$(LANGUAGE).msg: $$(@B).txt
#          mkmsgf $*.txt $*.msg

$(OUTPUTDIR)\xfldr$(LANGUAGE).obj: $(@B).c

$(OUTPUTDIR)\xfldr$(LANGUAGE).res: $(@B).rc $$(@B).dlg *.bmp *.ico ..\..\include\dlgids.h
         $(RC) -r -x2 $(@B).rc $(OUTPUTDIR)\$(@B).res

$(OUTPUTDIR)\xfldr$(LANGUAGE).dll: $(@B).def $(OBJS) $(OUTPUTDIR)\$(@B).res ..\..\include\dlgids.h
         $(LINK_BASE) $(@B).def $(OBJS) /OUT:$(OUTPUTDIR)\$(@B).dll
         $(RC) -p -x2 $(OUTPUTDIR)\$(@B).res $(OUTPUTDIR)\$(@B).dll

# main target
$(XWPRUNNING)\bin\xfldr$(LANGUAGE).dll: $(OUTPUTDIR)\xfldr$(LANGUAGE).dll
         unlock $(XWPRUNNING)\bin\$(@B).dll
         cmd.exe /c del $(XWPRUNNING)\bin\$(@B).dll
         cmd.exe /c copy $(OUTPUTDIR)\$(@B).dll $(XWPRUNNING)\bin


