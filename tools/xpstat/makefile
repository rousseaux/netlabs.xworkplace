#
# Makefile for xpstat.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
#       Copyright (C) 1998-2000 Ulrich M”ller.
#       This file is part of the XWorkplace source package.
#       XWorkplace is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published
#       by the Free Software Foundation, in version 2 as it comes in the
#       "COPYING" file of the XWorkplace main distribution.
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
MODULESDIR = ..\..\bin\modules

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif


# FIXED MACROS
# ------------

.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

HEADERS = $(HLPINC)\eah.h $(HLPINC)\dosh.h $(HLPINC)\winh.h

ALL_OBJ1 = $(OUTPUTDIR)\xpstat.obj $(OUTPUTDIR)\procstat.obj $(OUTPUTDIR)\cnrh.obj \
           $(OUTPUTDIR)\comctl.obj $(OUTPUTDIR)\debug.obj $(OUTPUTDIR)\except.obj $(OUTPUTDIR)\gpih.obj \
           $(OUTPUTDIR)\linklist.obj $(OUTPUTDIR)\dosh.obj $(OUTPUTDIR)\stringh.obj \
           $(OUTPUTDIR)\winh.obj $(OUTPUTDIR)\memdebug.obj $(OUTPUTDIR)\textview.obj \
           $(OUTPUTDIR)\tmsgfile.obj

# TARGETS
# -------

all: $(XWPRUNNING)\bin\xpstat.exe
    @echo ----- Leaving $(MAKEDIR)

$(OUTPUTDIR)\xpstat.obj: $(@B).c $(@B).h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\gpih.h $(HLPINC)\linklist.h \
        $(HLPINC)\memdebug.h $(HLPINC)\procstat.h $(HLPINC)\dosh.h $(HLPINC)\stringh.h \
        $(HLPINC)\textview.h $(HLPINC)\winh.h \
    ..\..\include\dlgids.h
        $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\procstat.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\comctl.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\cnrh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\debug.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\dosh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\except.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\gpih.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\linklist.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\memdebug.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\stringh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\winh.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\textview.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(OUTPUTDIR)\tmsgfile.obj: $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
          $(CC_EXE) /Fo$(OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

# build targets
xpstat.def: ..\..\include\bldlevel.h
          cmd.exe /c BuildLevel.cmd $*.def ..\..\include\bldlevel.h "XWorkplace process status Utility"

$(OUTPUTDIR)\xpstat.res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\xpstat.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) $(HELPERS_BASE)\src\helpers\pmprintf.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe

# main target
$(XWPRUNNING)\bin\xpstat.exe: $(MODULESDIR)\$(@B).exe
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin


