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
OBJS = $(OUTPUTDIR)\xdebug.obj \
       $(OUTPUTDIR)\xclslist.obj \
       $(OUTPUTDIR)\xfdataf.obj \
       $(OUTPUTDIR)\xfdesk.obj \
       $(OUTPUTDIR)\xfdisk.obj \
       $(OUTPUTDIR)\xfldr.obj \
       $(OUTPUTDIR)\xfobj.obj \
       $(OUTPUTDIR)\xfpgmf.obj \
       $(OUTPUTDIR)\xfstart.obj \
       $(OUTPUTDIR)\xfsys.obj \
       $(OUTPUTDIR)\xfwps.obj \
       $(OUTPUTDIR)\xwpsound.obj \
       $(OUTPUTDIR)\xtrash.obj \
       $(OUTPUTDIR)\xwpkeybd.obj \
       $(OUTPUTDIR)\xwpmouse.obj \
       $(OUTPUTDIR)\xwpsetup.obj

# The main target:
# If we're called from the main makefile, MAINMAKERUNNING is defined,
# and we'll set $(OBJS) as our targets (which will go on).
# Otherwise, we call the main makefile, which will again call ourselves later.
all:   \
!ifndef MAINMAKERUNNING
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
        $(CC_DLL) -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
        $(CC_DLL) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" -I$(INCLUDE) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

# main include path
INC = ..\..\include
# helpers include path
HLPINC = $(HELPERS_BASE)\include\helpers
# our own corresponding include path
MYOWNINC = $(INC)\classes

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

$(OUTPUTDIR)\xclslist.obj: \
            $(INC)\setup.h \
            $(HLPINC)\winh.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\config\classlst.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xdebug.obj:           $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h \
                      $(INC)\setup.h \
                      $(INC)\shared\common.h $(HLPINC)\pmprintf.h

$(OUTPUTDIR)\xfdataf.obj: \
            $(INC)\setup.h \
            $(HLPINC)\linklist.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\filesys\filesys.h $(INC)\filesys\filetype.h $(INC)\filesys\menus.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xfdesk.obj: \
            $(INC)\setup.h \
            $(HLPINC)\dosh.h $(HLPINC)\winh.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\filesys\desktop.h $(INC)\filesys\xthreads.h \
            $(INC)\startshut\archives.h $(INC)\startshut\shutdown.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h \

$(OUTPUTDIR)\xfdisk.obj: \
            $(INC)\setup.h \
            $(HLPINC)\undoc.h \
            $(INC)\classes\xfldr.ih \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
            $(INC)\filesys\disk.h $(INC)\filesys\folder.h $(INC)\filesys\menus.h $(INC)\filesys\statbars.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h \

$(OUTPUTDIR)\xfldr.obj: \
            $(INC)\setup.h \
            $(HLPINC)\dosh.h $(HLPINC)\cnrh.h $(HLPINC)\except.h $(HLPINC)\winh.h $(HLPINC)\eah.h $(HLPINC)\linklist.h $(HLPINC)\stringh.h $(HLPINC)\prfh.h $(HLPINC)\wphandle.h \
            $(INC)\classes\xfdisk.h \
            $(INC)\dlgids.h \
            $(INC)\shared\cnrsort.h $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
            $(INC)\filesys\filesys.h $(INC)\filesys\folder.h $(INC)\filesys\menus.h $(INC)\filesys\statbars.h $(INC)\filesys\xthreads.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h \

$(OUTPUTDIR)\xfobj.obj: \
            $(INC)\setup.h \
            $(HLPINC)\cnrh.h $(HLPINC)\winh.h $(HLPINC)\undoc.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
            $(INC)\filesys\fileops.h $(INC)\filesys\object.h $(INC)\filesys\xthreads.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xfpgmf.obj: \
            $(INC)\setup.h \
            $(HLPINC)\dosh.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\filesys\filesys.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xfstart.obj: \
            $(INC)\setup.h \
            $(HLPINC)\winh.h \
            $(INC)\classes\xfpgmf.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\filesys\folder.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xfsys.obj: \
            $(INC)\setup.h \
            $(HLPINC)\dosh.h \
            $(INC)\classes\xfobj.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\config\cfgsys.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xfwps.obj: \
            $(INC)\setup.h \
            $(HLPINC)\prfh.h $(HLPINC)\stringh.h $(HLPINC)\winh.h \
            $(INC)\classes\xfldr.ih \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\filesys\filetype.h $(INC)\filesys\folder.h $(INC)\filesys\menus.h $(INC)\filesys\statbars.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xtrash.obj: \
            $(INC)\setup.h \
            $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h $(HLPINC)\winh.h $(HLPINC)\stringh.h $(HLPINC)\undoc.h \
            $(INC)\classes\xfobj.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
            $(INC)\filesys\xthreads.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xwpkeybd.obj: \
            $(INC)\setup.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\notebook.h \
            $(INC)\config\hookintf.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xwpmouse.obj: \
            $(INC)\setup.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\notebook.h \
            $(INC)\config\hookintf.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xwpsetup.obj: \
            $(INC)\setup.h \
            $(HLPINC)\comctl.h $(HLPINC)\winh.h $(HLPINC)\procstat.h $(HLPINC)\stringh.h $(HLPINC)\tmsgfile.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h $(INC)\shared\xsetup.h \
            $(INC)\filesys\xthreads.h \
            $(INC)\hook\xwphook.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h \

$(OUTPUTDIR)\xwpsound.obj: \
            $(INC)\setup.h \
            $(INC)\classes\xfobj.h \
            $(INC)\dlgids.h \
            $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\config\sound.h \
            $(@B).c $(MYOWNINC)\$(@B).ih $(MYOWNINC)\$(@B).h


