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
#       Output:         ..\bin\exe_mt\*.obj
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
# defining file extensions that NMAKE will recognize.
.SUFFIXES: .c .obj .dll .h .ih .rc .res

HLPINC = $(HELPERS_BASE)\include\helpers

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
!if [@echo       BASE_OUTPUTDIR is $(BASE_OUTPUTDIR)]
!endif
OUTPUTDIR = $(BASE_OUTPUTDIR)\exe_mt

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

# The PGMGDMNOBJS macro contains all the XPager .OBJ files,
# which go into XWPDAEMN.EXE.
PGMGDMNOBJS = \
!ifndef XWPLITE
$(OUTPUTDIR)\pg_control.obj \
$(OUTPUTDIR)\pg_move.obj \
$(OUTPUTDIR)\pg_settings.obj \
$(OUTPUTDIR)\pg_winscan.obj
!endif

# The DMNOBJS macro contains all the .OBJ files for XWPDAEMN.EXE;
# this includes PGMGDMNOBJS.
OBJS = \
$(OUTPUTDIR)\xwpdaemn.obj \
$(OUTPUTDIR)\drivemonitor.obj \
$(PGMGDMNOBJS)

HLPOBJS = \
$(OUTPUTDIR)\debug.obj \
$(OUTPUTDIR)\gpih.obj \
$(OUTPUTDIR)\linklist.obj \
$(OUTPUTDIR)\except.obj \
$(OUTPUTDIR)\dosh.obj \
$(OUTPUTDIR)\memdebug.obj \
$(OUTPUTDIR)\regexp.obj \
$(OUTPUTDIR)\shapewin.obj \
$(OUTPUTDIR)\stringh.obj \
$(OUTPUTDIR)\threads.obj \
!ifdef XWP_DEBUG
$(OUTPUTDIR)\nls.obj \
$(OUTPUTDIR)\prfh.obj \
$(OUTPUTDIR)\tree.obj \
!endif
$(OUTPUTDIR)\xstring.obj \
$(XWP_OUTPUT_ROOT)\xwphook.lib

# extra LINKOBJS macro because we want to link $(OBJS) and $(HLPOBJS),
# but never compile $(HLPOBJS) from here
LINKOBJS = $(OBJS) $(HLPOBJS) $(PMPRINTF_LIB)

# The main target:
# If we're called from the main makefile, MAINMAKERUNNING is defined,
# and we'll set $(OBJS) as our targets (which will go on).
# Otherwise, we call the main makefile, which will again call ourselves later.
all:   \
!ifndef MAINMAKERUNNING
# we're not being called from main makefile: start main makefile
    callmainmake
    @echo ----- Leaving $(MAKEDIR)
!else
#    $(OBJS) $(OUTPUTDIR)\xwpdaemn.res
    $(XWPRUNNING)\bin\xwpdaemn.exe
    @echo ----- Leaving $(MAKEDIR)
!endif

callmainmake:
    @echo $(MAKEDIR)\makefile [$@]: Recursing to main makefile.
    @cd ..\..
    @nmake
    @echo $(MAKEDIR)\makefile [$@]: Returned from main makefile. Done.

# The "dep" target: run fastdep on the sources.
# "nmake dep" gets called from src\makefile if nmake dep
# is running on the main makefile.
dep:
    $(RUN_FASTDEP) *.c
    @echo ----- Leaving $(MAKEDIR)

.c.{$(OUTPUTDIR)}.obj:
    @echo $(MAKEDIR)\makefile [$@]: Compiling $(@B).c
!ifndef PRECH
# precompiled headers disabled:
    $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_EXE_MT) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

$(OUTPUTDIR)\xwpdaemn.res: *.ico *.bmp $(@B).rc
    $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

# The .OBJ-from-sources dependencies are now automatically
# created by "nmake dep" into the .depend include file.
# V0.9.12 (2001-05-22) [umoeller]

!ifndef NOINCLUDEDEPEND
!include .depend
!endif

#
# Linking XWPDAEMN.EXE
#
$(XWPRUNNING)\bin\xwpdaemn.exe: $(MODULESDIR)\$(@B).exe
        cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
xwpdaemn.def: $(PROJECT_BASE_DIR)\include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ $(PROJECT_BASE_DIR)\include\bldlevel.h "$(XWPNAME) PM daemon"

# create import library from XWPHOOK.DLL
$(XWP_OUTPUT_ROOT)\xwphook.lib: $(MODULESDIR)\$(@B).dll
        implib /nologo $(XWP_OUTPUT_ROOT)\$(@B).lib $(MODULESDIR)\$(@B).dll

$(MODULESDIR)\xwpdaemn.exe: $(@B).def $(LINKOBJS) $(OUTPUTDIR)\$(@B).res makefile
        @echo $(MAKEDIR)\makefile [$@]: Linking $(MODULESDIR)\$(@B).exe
        $(LINK) @<<
/OUT:$(MODULESDIR)\$(@B).exe $(@B).def $(LINKOBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        $(RC) ..\exe_mt\$(@B).res $(@B).exe
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)


