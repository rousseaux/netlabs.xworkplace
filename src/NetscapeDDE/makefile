#
# Makefile for NETSCDDE.EXE.
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

PROJECT_BASE_DIR = $(CVS_WORK_ROOT)\xworkplace

# include setup (compiler options etc.)
!include ..\..\setup.in

OUTPUTDIR = ..\..\bin\exe_mt
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

HEADERS = netscdde.h

ALL_OBJ1 = $(OUTPUTDIR)\netscdde.obj

# TARGETS
# -------

all: $(XWPRUNNING)\bin\netscdde.exe
    @echo ----- Leaving $(MAKEDIR)

$(OUTPUTDIR)\netscdde.obj: netscdde.c $(HEADERS)
          $(CC_EXE_ST) /Fo$(OUTPUTDIR)\$(@B).obj netscdde.c

# build targets
netscdde.def: ..\..\include\bldlevel.h
          cmd.exe /c BuildLevel.cmd $*.def ..\..\include\bldlevel.h "XWorkplace Netscape DDE Interface"

$(OUTPUTDIR)\netscdde.res: netscdde.rc netscdde.h netscdde.dlg
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\netscdde.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) $(HELPERS_BASE)\src\helpers\pmprintf.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe

# main target
$(XWPRUNNING)\bin\netscdde.exe: $(MODULESDIR)\$(@B).exe
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin


