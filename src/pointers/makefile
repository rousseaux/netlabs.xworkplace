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

PROJECT_BASE_DIR = $(MAKEDIR)\..\..

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
# files will be created in.
OUTPUTDIR = ..\..\bin

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
        $(CC_DLL) -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
        $(CC_DLL) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

# main include path
INC = ..\..\include
# helpers include path
HLPINC = $(HELPERS_BASE)\include\helpers
# our own corresponding include path
MYOWNINC = $(INC)\pointers

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

$(OUTPUTDIR)\amptrrc.obj: \
    $(INC)\setup.h \
    $(@B).c
#$(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\aniedit2.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\aniedit2.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\mptrfile.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrptr.h \
    ..\..\001\dll\r_amptr001.h \
    $(MYOWNINC)\mptrcnr.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\cursor.obj: \
    $(INC)\setup.h \
    $(HLPINC)\eah.h \
    $(MYOWNINC)\cursor.h \
    $(MYOWNINC)\info.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\eas.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\dll.obj: \
    $(INC)\setup.h \
    ..\..\001\dll\r_amptr001.h \
    $(MYOWNINC)\dll.h \
    $(MYOWNINC)\lx.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\bindata.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\dllbin.obj: template.dll
    ..\..\tools\xbin2obj -n template.dll -cCONST bindataDllTemplate $(OUTPUTDIR)\dllbin.obj

#$(OUTPUTDIR)\expire.obj: \
#    $(INC)\setup.h \
#    $(INC)\dlgids.h \
#    $(MYOWNINC)\expire.h \
#    $(MYOWNINC)\debug.h \
#    $(MYOWNINC)\r_amptreng.h \
#    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptranim.obj: \
    $(INC)\setup.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrhook.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrcnr.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrprop.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptredit.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptredit.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\pointer.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrfile.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrfile.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrhook.obj: \
    $(INC)\setup.h \
    $(MYOWNINC)\mptrhook.h \
    $(MYOWNINC)\debug.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrldr.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrldr.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrprop.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptrlset.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrlset.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h \
    $(INC)\classes\xfdataf.ih \
    $(MYOWNINC)\mptrlset.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrpag1.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrfile.h \
    $(MYOWNINC)\mptrpag1.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrprop.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptredit.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrppl.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\pointer.h \
    $(MYOWNINC)\cursor.h \
    $(MYOWNINC)\dll.h \
    $(MYOWNINC)\script.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrprop.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrprop.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrptr.obj: \
    $(INC)\setup.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\cursor.h \
    $(MYOWNINC)\pointer.h \
    $(MYOWNINC)\dll.h \
    $(MYOWNINC)\eas.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\fmacros.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrset.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrppl.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptranim.h \
    $(MYOWNINC)\wmuser.h \
    $(MYOWNINC)\pointer.h \
    $(MYOWNINC)\cursor.h \
    $(MYOWNINC)\dll.h \
    $(MYOWNINC)\script.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\mptrutil.obj: \
    $(INC)\setup.h \
    $(INC)\dlgids.h \
    $(MYOWNINC)\mptrcnr.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\mptrptr.h \
    ..\..\001\dll\r_amptr001.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\pointer.obj: \
    $(INC)\setup.h \
    $(HLPINC)\eah.h \
    $(MYOWNINC)\pointer.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\fmacros.h \
    $(MYOWNINC)\eas.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\script.obj: \
    $(INC)\setup.h \
    ..\..\001\dll\r_amptr001.h \
    $(MYOWNINC)\dll.h \
    $(MYOWNINC)\script.h \
    $(MYOWNINC)\mptrptr.h \
    $(MYOWNINC)\mptrset.h \
    $(MYOWNINC)\mptrutil.h \
    $(MYOWNINC)\macros.h \
    $(MYOWNINC)\debug.h \
    $(MYOWNINC)\bindata.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\wpamptr.obj: \
    $(INC)\setup.h \
    $(@B).c
#$(MYOWNINC)\$(@B).h


