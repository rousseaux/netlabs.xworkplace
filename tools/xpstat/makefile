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

# include setup (compiler options etc.)
!include ..\..\setup.in

# FIXED MACROS
# ------------

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)\exe_mt
MODULESDIR = $(XWP_OUTPUT_ROOT)\modules

!if [@md $(OUTPUTDIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

HEADERS = $(HLPINC)\eah.h $(HLPINC)\dosh.h $(HLPINC)\winh.h

ALL_OBJ1 = $(OUTPUTDIR)\xpstat.obj $(OUTPUTDIR)\helpers.lib $(HELPERS_BASE)\src\helpers\pmprintf.lib

CURRENT_DIR = $(MAKEDIR)

# TARGETS
# -------

all: helpers_exe_mt $(XWPRUNNING)\bin\xpstat.exe
    @echo ----- Leaving $(MAKEDIR)

$(OUTPUTDIR)\xpstat.obj: $(@B).c $(@B).h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\gpih.h $(HLPINC)\linklist.h \
        $(HLPINC)\memdebug.h $(HLPINC)\procstat.h $(HLPINC)\dosh.h $(HLPINC)\stringh.h \
        $(HLPINC)\textview.h $(HLPINC)\winh.h \
    ..\..\include\dlgids.h
        $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c

helpers_exe_mt:
    @echo $(MAKEDIR)\makefile: Going for src\helpers (EXE MT version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(XWP_OUTPUT_ROOT)\exe_mt" "CC_HELPERS=$(CC_HELPERS_EXE_MT)"
    @cd $(CURRENT_DIR)

$(OUTPUTDIR)\helpers.lib: helpers_exe_mt

# build targets
xpstat.def: ..\..\include\bldlevel.h
    cmd.exe /c BuildLevel.cmd $*.def ..\..\include\bldlevel.h "XWorkplace process status utility"

$(OUTPUTDIR)\xpstat.res: $(@B).rc *.ico
    $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\xpstat.exe: $(ALL_OBJ1) $(@B).def $(OUTPUTDIR)\$(@B).res makefile
    $(LINK) /out:$(MODULESDIR)\$(@B).exe $(ALL_OBJ1) $(@B).def
    $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe

# main target
$(XWPRUNNING)\bin\xpstat.exe: $(MODULESDIR)\$(@B).exe
    cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin


