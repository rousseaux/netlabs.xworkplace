#
# makefile:
#       makefile for src/main directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    main makefile
#
#       Input:          ./*.c
#
#       Output:         ../bin/*.obj
#
#       Edit "setup.in" to set up the make process.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
!include ..\..\setup.in

# FIXED MACROS
# ------------
#
# You probably need not change the following.
#

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize in inference
# rules.

.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
BASE_OUTPUTDIR = ..\..\bin
EXE_OUTPUTDIR = $(BASE_OUTPUTDIR)\exes

# create output directory
!if [@md $(BASE_OUTPUTDIR) 2> NUL]
!endif
!if [@md $(EXE_OUTPUTDIR) 2> NUL]
!endif

# The OBJS macro contains all the .OBJ files which need to be
# created from the files in this directory.
# These will be put into BIN\.
OBJS = $(EXE_OUTPUTDIR)\xwpdaemn.obj \
       $(EXE_OUTPUTDIR)\pgmg_control.obj \
       $(EXE_OUTPUTDIR)\pgmg_move.obj \
       $(EXE_OUTPUTDIR)\pgmg_settings.obj \
       $(EXE_OUTPUTDIR)\pgmg_winscan.obj \
       $(EXE_OUTPUTDIR)\threads.obj \
       $(BASE_OUTPUTDIR)\xwphook.obj

# The main target:
# If we're called from the main makefile, MAINMAKERUNNING is defined,
# and we'll set $(OBJS) as our targets (which will go on).
# Otherwise, we call the main makefile, which will again call ourselves later.
all:   \
!ifndef MAINMAKERUNNING
    callmainmake
    @echo ----- Leaving $(MAKEDIR)
!else
    $(OBJS) $(EXE_OUTPUTDIR)\xwpdaemn.res
    @echo ----- Leaving $(MAKEDIR)
!endif

callmainmake:
    @echo $(MAKEDIR)\makefile: Recursing to main makefile.
    @cd ..\..
    @nmake
    @echo $(MAKEDIR)\makefile: Returned from main makefile. Done.

# main include path
INC = ..\..\include
# helpers include path
HLPINC = $(HELPERS_BASE)\include\helpers
# our own corresponding include path
MYOWNINC = $(INC)\hook

# Now define dependencies for all our .OBJ files by telling NMAKE
# which .OBJ file depends on what. For each .OBJ we define what files
# will enforce a recompilation when they've been changed.
# This uses the inference rules defined above.

# Note that include\setup.h is specified with _all_ files, so
# you can enforce a complete recompile of XWorkplace if you update
# that file (just save it once).

# Here follow the .OBJs for the SOM/WPS classes. Each class needs to be
# recompiled when its SOM header files change (due to SOM recompilation)
# or when other headers (in MAIN or HELPERS) have changed.

$(EXE_OUTPUTDIR)\xwpdaemn.obj: \
        $(INC)\setup.h \
        $(INC)\shared\kernel.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h $(MYOWNINC)\xwpdaemn.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_EXE) -I$(INCLUDE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(EXE_OUTPUTDIR)\pgmg_control.obj: \
        $(INC)\setup.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h $(MYOWNINC)\xwpdaemn.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_EXE) -I$(INCLUDE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(EXE_OUTPUTDIR)\pgmg_move.obj: \
        $(INC)\setup.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h $(MYOWNINC)\xwpdaemn.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_EXE) -I$(INCLUDE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(EXE_OUTPUTDIR)\pgmg_settings.obj: \
        $(INC)\setup.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h $(MYOWNINC)\xwpdaemn.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_EXE) -I$(INCLUDE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(EXE_OUTPUTDIR)\pgmg_winscan.obj: \
        $(INC)\setup.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h $(MYOWNINC)\xwpdaemn.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_EXE) -I$(INCLUDE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(EXE_OUTPUTDIR)\threads.obj: \
        $(INC)\setup.h \
        $(HELPERS_BASE)\src\helpers\$(@B).c $(HEADERS)
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
    $(CC_EXE) /Fo$(EXE_OUTPUTDIR)\$(@B).obj $(HELPERS_BASE)\src\helpers\$(@B).c

$(EXE_OUTPUTDIR)\xwpdaemn.res: *.ico $(@B).rc
    $(RC) -r -i $(BASE_OUTPUTDIR) $(@B).rc $(EXE_OUTPUTDIR)\$(@B).res

$(BASE_OUTPUTDIR)\xwphook.obj: \
        $(INC)\setup.h \
        $(HLPINC)\undoc.h \
        $(INC)\hook\xwphook.h \
        $(@B).c $(MYOWNINC)\xwphook.h $(MYOWNINC)\hook_private.h
    @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
    $(CC_DLL) -I$(INCLUDE) /Fo$(BASE_OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_DLL) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" -I$(INCLUDE) /Fo$(BASE_OUTPUTDIR)\$(@B).obj $(@B).c
!endif








