#
# Makefile for XWPSHELL.EXE.
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

MYDIR = $(MAKEDIR)

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

INC = ..\..\include

HLPINC = $(HELPERS_BASE)\include\helpers

HEADERS = $(HLPINC)\eah.h $(HLPINC)\dosh.h $(HLPINC)\linklist.h $(HLPINC)\memdebug.h $(HLPINC)\winh.h $(HLPINC)\xstring.h

ALL_OBJ1 = \
    $(OUTPUTDIR)\acldb.obj \
    $(OUTPUTDIR)\contexts.obj \
    $(OUTPUTDIR)\loggedon.obj \
    $(OUTPUTDIR)\userdb.obj \
    $(OUTPUTDIR)\subjects.obj \
    $(OUTPUTDIR)\xwpshell.obj \
    ..\..\bin\exe_mt\helpers.lib

# TARGETS
# -------

all: $(XWPRUNNING)\bin\xwpshell.exe
    @echo ----- Leaving $(MAKEDIR)

$(OUTPUTDIR)\acldb.obj: \
    $(HLPINC)\tree.h \
    $(INC)\security\xwpsecty.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\contexts.obj: \
    $(HLPINC)\tree.h \
    $(INC)\security\xwpsecty.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\loggedon.obj: \
    $(INC)\security\xwpsecty.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\userdb.obj: \
    $(INC)\security\xwpsecty.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\subjects.obj: \
    $(HLPINC)\tree.h \
    $(INC)\security\xwpsecty.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

$(OUTPUTDIR)\xwpshell.obj: \
    $(HLPINC)\procstat.h \
    $(INC)\security\ring0api.h $(INC)\security\xwpsecty.h xwpshell.h \
    ..\..\include\bldlevel.h \
    $(@B).c $(HEADERS)
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

# build targets
xwpshell.def: ..\..\include\bldlevel.h
          cmd.exe /c BuildLevel.cmd $*.def ..\..\include\bldlevel.h "XWorkplace Security shell starter"

$(OUTPUTDIR)\xwpshell.res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\xwpshell.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res makefile
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) $(HELPERS_BASE)\src\helpers\pmprintf.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe
          @cd $(MODULESDIR)
          mapsym xwpshell.map
          @cd $(MYDIR)

# main target
$(XWPRUNNING)\bin\xwpshell.exe: $(MODULESDIR)\$(@B).exe
          unlock $(XWPRUNNING)\bin\$(@B).exe
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
          cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe E:\os2


