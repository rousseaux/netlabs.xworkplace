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
OUTPUTDIR = $(BASE_OUTPUTDIR)\exe_mt
!if [@echo       OUTPUTDIR is $(OUTPUTDIR)]
!endif

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

# The OBJS macro contains all the .OBJ files which need to be
# created from the files in this directory.
# These will be put into BIN\.
# If you add a new source, add the corresponding .OBJ file here.
OBJS = \
$(OUTPUTDIR)\xwphook.obj \
# mousemove next, since it gets called most frequently
$(OUTPUTDIR)\hk_msmove.obj \
$(OUTPUTDIR)\hk_char.obj \
$(OUTPUTDIR)\hk_misc.obj \
$(OUTPUTDIR)\hk_scroll.obj

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
    $(XWPRUNNING)\bin\xwphook.dll
#    $(OBJS)
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
    $(CC_DLL_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
    $(CC_DLL_MT) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

# The .OBJ-from-sources dependencies are now automatically
# created by "nmake dep" into the .depend include file.
# V0.9.12 (2001-05-22) [umoeller]

!ifndef NOINCLUDEDEPEND
!include .depend
!endif

#
# Linking XWPHOOK.DLL
#
$(XWPRUNNING)\bin\xwphook.dll: $(MODULESDIR)\$(@B).dll
# no unlock, this is a hook        unlock $@
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!ifdef DYNAMIC_TRACE
        @echo $(MAKEDIR)\makefile [$@]: Creating TRACE files for $(@B).dll
        maptsf $(@B).map /MAJOR=253 /LOGSTACK=32 /LOGRETURN > $(@B).tsf
        trcust $(@B).tsf /I /L=bin\$(@B).dll /node /M=$(@B).map
        @echo $(MAKEDIR)\makefile [$@]: Done creating TRACE files for $(@B).dll
        cmd.exe /c copy $(@B).tdf $(XWPRUNNING)\bin
        cmd.exe /c del $(@B).tdf
        cmd.exe /c copy TRC00FD.TFF $(DYNAMIC_TRACE):\OS2\SYSTEM\TRACE
        cmd.exe /c del TRC00FD.TFF
!endif

# update DEF file if buildlevel has changed
xwphook.def: $(PROJECT_BASE_DIR)\include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ $(PROJECT_BASE_DIR)\include\bldlevel.h "$(XWPNAME) PM hook module"

$(MODULESDIR)\xwphook.dll: $(@B).def $(OBJS)
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ $(@B).def $(OBJS) $(PMPRINTF_LIB)
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

