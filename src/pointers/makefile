#
# makefile:
#       makefile for src/pointers directory.
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
OBJS = $(OUTPUTDIR)\amptrrc.obj \
       $(OUTPUTDIR)\aniedit2.obj \
       $(OUTPUTDIR)\cursor.obj \
       $(OUTPUTDIR)\dll.obj \
       $(OUTPUTDIR)\dllbin.obj \
#       $(OUTPUTDIR)\expire.obj \
       $(OUTPUTDIR)\mptranim.obj \
       $(OUTPUTDIR)\mptrcnr.obj \
       $(OUTPUTDIR)\mptredit.obj \
       $(OUTPUTDIR)\mptrfile.obj \
       $(OUTPUTDIR)\mptrhook.obj \
       $(OUTPUTDIR)\mptrldr.obj \
       $(OUTPUTDIR)\mptrlset.obj \
       $(OUTPUTDIR)\mptrpag1.obj \
       $(OUTPUTDIR)\mptrppl.obj \
       $(OUTPUTDIR)\mptrprop.obj \
       $(OUTPUTDIR)\mptrptr.obj \
       $(OUTPUTDIR)\mptrset.obj \
       $(OUTPUTDIR)\mptrutil.obj \
       $(OUTPUTDIR)\pointer.obj \
       $(OUTPUTDIR)\script.obj \
#$(OUTPUTDIR)\sominit.obj \
       $(OUTPUTDIR)\wpamptr.obj
       #$(OUTPUTDIR)\wpand.obj \
       #$(OUTPUTDIR)\wpoptr.obj \
#       $(OUTPUTDIR)\wpwani.obj \
#       $(OUTPUTDIR)\anwcur.obj \

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

# The "dep" target: run fastdep on the sources.
# "nmake dep" gets called from src\makefile if nmake dep
# is running on the main makefile.
dep:
    $(RUN_FASTDEP) *.c
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

.c.{$(OUTPUTDIR)}.obj:
        @echo $(MAKEDIR)\makefile: Compiling $(@B).c
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



