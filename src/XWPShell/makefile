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

# include setup (compiler options etc.)
!include ..\..\setup.in

MYDIR = $(MAKEDIR)

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in. $(XWP_OUTPUT_ROOT) is set by
# setup.in to point to \bin from the XWorkplace sources root,
# but this can be modified by setting an external environment
# variable. This approach has the advantage that
# 1) all build files are in a common dir tree and the entire
#    tree can be removed completely;
# 2) the build tree can be on a different physical drive for
#    speed.
BASE_OUTPUTDIR = $(XWP_OUTPUT_ROOT)
OUTPUTDIR = $(BASE_OUTPUTDIR)\exe_mt
MODULESDIR = $(BASE_OUTPUTDIR)\modules

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
    $(OUTPUTDIR)\helpers.lib

# TARGETS
# -------

all: $(XWPRUNNING)\bin\xwpshell.exe
    @echo ----- Leaving $(MAKEDIR)

# The "dep" target: run fastdep on the sources.
# "nmake dep" gets called from src\makefile if nmake dep
# is running on the main makefile.
dep:
    $(RUN_FASTDEP) *.c

# build targets
xwpshell.def: ..\..\include\bldlevel.h
          $(RUN_BLDLEVEL) $*.def ..\..\include\bldlevel.h "XWorkplace Security shell starter"

$(OUTPUTDIR)\xwpshell.res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\xwpshell.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res makefile
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) $(HELPERS_BASE)\src\helpers\pmprintf.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
          @cd $(MODULESDIR)
          mapsym xwpshell.map
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
          @cd $(MYDIR)

# main target
$(XWPRUNNING)\bin\xwpshell.exe: $(MODULESDIR)\$(@B).exe
          $(RUN_UNLOCK) $(XWPRUNNING)\bin\$(@B).exe
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
          cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe E:\os2

# The .OBJ-from-sources dependencies are now automatically
# created by "nmake dep" into the .depend include file.
# V0.9.12 (2001-05-22) [umoeller]

!ifndef NOINCLUDEDEPEND
!include .depend
!endif


