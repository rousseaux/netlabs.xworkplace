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

PROJECT_BASE_DIR = $(CVS_WORK_ROOT)\xworkplace

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

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in. $(XWP_OUTPUT_ROOT) is set by
# setup.in to point to \bin from the XWorkplace sources root,
# but this can be modified by setting an external environment
# variable. This approach has the advantage that
# 1) all build files are in a common dir tree and the entire
#    tree can be removed completely;
# 2) the build tree can be on a different physical drive for
#    speed.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)
!if [@echo       OUTPUTDIR is $(OUTPUTDIR)]
!endif

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

# The OBJS macro contains all the .OBJ files which need to be
# created from the files in this directory.
# These will be put into BIN\.
# If you add a new source, add the corresponding .OBJ file here.
OBJS = $(OUTPUTDIR)\ctr_engine.obj \
       $(OUTPUTDIR)\ctr_notebook.obj \
       $(OUTPUTDIR)\w_objbutton.obj \
       $(OUTPUTDIR)\w_pulse.obj

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
    $(OBJS)
    @echo ----- Leaving $(MAKEDIR)
!endif

callmainmake:
    @echo $(MAKEDIR)\makefile: Recursing to main makefile.
    @cd ..\..
    @nmake
    @echo $(MAKEDIR)\makefile: Returned from main makefile. Done.

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

.c.{$(OUTPUTDIR)}.obj:
        @echo $(MAKEDIR)\makefile: Compiling $(@B).c
!ifndef PRECH
# precompiled headers disabled:
        $(CC_DLL_MT) -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
        $(CC_DLL_MT) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

# main include path
INC = ..\..\include
# helpers include path
HLPINC = $(HELPERS_BASE)\include\helpers
# our own corresponding include path
MYOWNINC = $(INC)\xcenter

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

# XWorkplace "library" .OBJ files used by several classes.
# Source code is here in SRC\MAIN.

$(OUTPUTDIR)\ctr_engine.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\gpih.h $(HLPINC)\linklist.h \
        $(HLPINC)\prfh.h $(HLPINC)\winh.h $(HLPINC)\standards.h $(HLPINC)\stringh.h \
        $(HLPINC)\threads.h $(HLPINC)\timer.h $(HLPINC)\xstring.h \
    $(INC)\classes\xcenter.ih $(INC)\classes\xfobj.ih \
    $(INC)\bldlevel.h $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(INC)\shared\center.h $(INC)\xcenter\centerp.h \
    $(@B).c

$(OUTPUTDIR)\ctr_notebook.obj: \
    $(INC)\setup.h \
    $(HLPINC)\cnrh.h $(HLPINC)\except.h $(HLPINC)\linklist.h $(HLPINC)\prfh.h \
        $(HLPINC)\standards.h $(HLPINC)\stringh.h $(HLPINC)\winh.h $(HLPINC)\xstring.h \
    $(INC)\classes\xcenter.ih $(INC)\classes\xfobj.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(INC)\shared\center.h $(INC)\xcenter\centerp.h \
    $(@B).c

$(OUTPUTDIR)\w_objbutton.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\gpih.h $(HLPINC)\linklist.h $(HLPINC)\prfh.h \
        $(HLPINC)\winh.h $(HLPINC)\xstring.h \
    $(INC)\classes\xcenter.ih $(INC)\classes\xfldr.ih $(INC)\classes\xfobj.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\contentmenus.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\shared\center.h \
    $(INC)\filesys\object.h \
    $(INC)\startshut\shutdown.h \
    $(@B).c

$(OUTPUTDIR)\w_pulse.obj: \
    $(INC)\setup.h \
    $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\gpih.h $(HLPINC)\prfh.h $(HLPINC)\winh.h \
        $(HLPINC)\stringh.h $(HLPINC)\timer.h $(HLPINC)\xstring.h \
    $(INC)\shared\common.h \
    $(INC)\shared\center.h \
    $(@B).c


