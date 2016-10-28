#
# Makefile for XIMGVIEW.EXE.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
#       Copyright (C) 1998-2006 Ulrich M”ller.
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
!include ..\..\config.in
!include ..\..\make\setup.in

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)\exe_mt
MODULESDIR = $(XWP_OUTPUT_ROOT)\modules

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

# update DEF file
!if [@$(RUN_BLDLEVEL) $ximgview.def ..\..\include\bldlevel.h "$(XWPNAME) image viewer"]
!endif


# FIXED MACROS
# ------------

.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

HEADERS = ximgview.h $(HLPINC)\eah.h $(HLPINC)\dosh.h $(HLPINC)\linklist.h $(HLPINC)\memdebug.h $(HLPINC)\winh.h $(HLPINC)\xstring.h

ALL_OBJ1 = $(OUTPUTDIR)\ximgview.obj $(OUTPUTDIR)\helpers.lib $(HELPERS_BASE)\src\helpers\pmprintf.lib

MYDIR = $(MAKEDIR)

# TARGETS
# -------

all: helpers $(XWPRUNNING)\bin\ximgview.exe
    @echo ----- Leaving $(MAKEDIR)

helpers:
    @cd ..\..
    @$(MAKE) -nologo helpers
    cd $(MYDIR)

$(OUTPUTDIR)\ximgview.obj: $(@B).c $(HEADERS) ..\..\include\bldlevel.h
          $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

# build targets
$(OUTPUTDIR)\ximgview.res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\ximgview.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res makefile
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) mmpm2.lib $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe

# main target
$(XWPRUNNING)\bin\ximgview.exe: $(MODULESDIR)\ximgview.exe
          $(COPY) $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin


