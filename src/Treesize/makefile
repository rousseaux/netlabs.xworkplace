# $Id$ 

#
# Makefile for TREESIZE.EXE.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
#       Copyright (C) 1998-99 Ulrich M”ller.
#       This file is part of the XFolder source package.
#       XFolder is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published
#       by the Free Software Foundation, in version 2 as it comes in the
#       "COPYING" file of the XFolder main distribution.
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

PROJECT_BASE_DIR = $(MAKEDIR)\..\..

# include setup (compiler options etc.)
!include ..\..\setup.in

OUTPUTDIR = ..\..\bin\exes

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif


# FIXED MACROS
# ------------

.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

HEADERS = treesize.h $(HLPINC)\eah.h $(HLPINC)\dosh.h $(HLPINC)\winh.h

ALL_OBJ1 = $(OUTPUTDIR)\treesize.obj $(OUTPUTDIR)\eah.obj $(OUTPUTDIR)\cnrh.obj \
           $(OUTPUTDIR)\gpih.obj $(OUTPUTDIR)\dosh.obj $(OUTPUTDIR)\stringh.obj \
           $(OUTPUTDIR)\winh.obj

# TARGETS
# -------

all: $(XWPRUNNING)\bin\treesize.exe
    @echo ----- Leaving $(MAKEDIR)

$(OUTPUTDIR)\treesize.obj: $(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\eah.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\cnrh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\dosh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\gpih.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\stringh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\winh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

# build targets
$(OUTPUTDIR)\treesize.def: ..\..\include\bldlevel.h
          cmd.exe /c BuildLevel.cmd $*.def ..\..\include\bldlevel.h "XWorkplace Treesize Utility"

$(OUTPUTDIR)\treesize.res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(OUTPUTDIR)\treesize.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res
          $(LINK) /out:$(OUTPUTDIR)\$(@B).exe $(ALL_OBJ1) $(HELPERS_BASE)\src\helpers\pmprintf.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(OUTPUTDIR)\$(@B).exe

# main target
$(XWPRUNNING)\bin\treesize.exe: $(OUTPUTDIR)\$(@B).exe
          cmd.exe /c copy $(OUTPUTDIR)\$(@B).exe $(XWPRUNNING)\bin


