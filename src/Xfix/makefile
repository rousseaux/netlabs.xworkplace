#
# makefile:
#       makefile for TOOLS\xfix directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    main makefile
#
#       Input:          files in this directory
#
#       Output:         xfix.exe, copied to parent directory (TOOLS\).
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
.SUFFIXES: .cpp .obj .h

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in. $(XWP_OUTPUT_ROOT) is set by
# setup.in to point to \bin from the XWorkplace sources root,
# but this can be modified by setting an external environment
# variable. This approach has the advantage that
# 1) all build files are in a common dir tree and the entire
#    tree can be removed completely;
# 2) the build tree can be on a different physical drive for
#    speed.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)\exe_mt
MODULESDIR = $(XWP_OUTPUT_ROOT)\modules

!if [@md $(OUTPUTDIR) 2> NUL]
!endif

APPNAME = xfix

# The OBJS macro contains all the .OBJ files which need to be
# created from the files in this directory.
# These will be put into BIN\.
OBJS = $(OUTPUTDIR)\$(APPNAME).obj $(OUTPUTDIR)\helpers.lib $(HELPERS_BASE)\src\helpers\pmprintf.lib

# The main target:
# If we're called from the main makefile, MAINMAKERUNNING is defined,
# and we'll set $(OBJS) as our targets (which will go on).
# Otherwise, we call the main makefile, which will again call ourselves later.
all: \
    $(XWPRUNNING)\bin\$(APPNAME).exe
    @echo ----- Leaving $(MAKEDIR)

# The "dep" target: run fastdep on the sources.
# "nmake dep" gets called from src\makefile if nmake dep
# is running on the main makefile.
dep:
    $(RUN_FASTDEP) *.cpp
    @echo ----- Leaving $(MAKEDIR)

# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
# The ugly {} brackets are some awkward syntax for specifying
# files in other directories.

# Special macros used here: $(@B) is the current target w/out ext.

# -- compile C files to .OBJ files, using the CC macro above.
#    The output will be placed in the directory specified by
#    the OUTPUTDIR variable (set above).

.cpp.{$(OUTPUTDIR)}.obj:
        @echo $(MAKEDIR)\makefile [$@]: Compiling $(@B).cpp
!ifndef PRECH
# precompiled headers disabled:
        $(CC_EXE_MT) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).cpp
!else
        $(CC_EXE_MT) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" /Fo$(OUTPUTDIR)\$(@B).obj $(@B).cpp
!endif

# build targets
$(APPNAME).def: ..\..\include\bldlevel.h makefile
          $(RUN_BLDLEVEL) $*.def ..\..\include\bldlevel.h "WPS handles fixer"

$(OUTPUTDIR)\$(APPNAME).res: $(@B).rc *.ico
          $(RC) -r $(@B).rc $(OUTPUTDIR)\$(@B).res

$(MODULESDIR)\$(APPNAME).exe: $(OBJS) $(@B).def $(OUTPUTDIR)\$(@B).res makefile
          $(LINK) /out:$(MODULESDIR)\$(@B).exe $(OBJS) $(@B).def
          $(RC) $(OUTPUTDIR)\$(@B).res $(MODULESDIR)\$(@B).exe
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
          cd $(MODULESDIR)
          mapsym /n $(@B).map > NUL
          cmd.exe /c copy $(@B).sym $(XWPRUNNING)\bin
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
          cd $(MAKEDIR)

# main target
$(XWPRUNNING)\bin\$(APPNAME).exe: $(MODULESDIR)\$(@B).exe
          $(RUN_UNLOCK) $(XWPRUNNING)\bin\$(@B).exe
          cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
          cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
#          cmd.exe /c copy $(MODULESDIR)\$(@B).exe E:\os2

# The .OBJ-from-sources dependencies are now automatically
# created by "nmake dep" into the .depend include file.
# V0.9.12 (2001-05-22) [umoeller]

!ifndef NOINCLUDEDEPEND
!include .depend
!endif



