#
# makefile:
#       makefile for src/filesys directory.
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
# files will be created in.
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
OBJS = $(OUTPUTDIR)\desktop.obj \
       $(OUTPUTDIR)\disk.obj \
       $(OUTPUTDIR)\fdrhotky.obj \
       $(OUTPUTDIR)\fdrmenus.obj \
       $(OUTPUTDIR)\fdrnotebooks.obj \
       $(OUTPUTDIR)\fdrsubclass.obj \
       $(OUTPUTDIR)\fhandles.obj \
       $(OUTPUTDIR)\filedlg.obj \
       $(OUTPUTDIR)\fileops.obj \
       $(OUTPUTDIR)\filesys.obj \
       $(OUTPUTDIR)\filetype.obj \
       $(OUTPUTDIR)\folder.obj \
       $(OUTPUTDIR)\fops_bottom.obj \
       $(OUTPUTDIR)\fops_top.obj \
       $(OUTPUTDIR)\object.obj \
       $(OUTPUTDIR)\program.obj \
       $(OUTPUTDIR)\refresh.obj \
       $(OUTPUTDIR)\statbars.obj \
       $(OUTPUTDIR)\trash.obj \
       $(OUTPUTDIR)\xthreads.obj

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
MYOWNINC = $(INC)\filesys

# Now define dependencies for all our .OBJ files by telling NMAKE
# which .OBJ file depends on what. For each .OBJ we define what files
# will enforce a recompilation when they've been changed.
# This uses the inference rules defined above.

# Note that include\setup.h is specified with _all_ files, so
# you can enforce a complete recompile of XWorkplace if you update
# that file (just save it once).

$(OUTPUTDIR)\desktop.obj: \
    $(INC)\setup.h \
    $(HLPINC)\animate.h $(HLPINC)\comctl.h $(HLPINC)\gpih.h $(HLPINC)\prfh.h \
        $(HLPINC)\shapewin.h $(HLPINC)\stringh.h $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfdesk.h \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
    $(INC)\filesys\fdrmenus.h $(INC)\filesys\xthreads.h \
    $(INC)\startshut\shutdown.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\disk.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\dosh.h $(HLPINC)\stringh.h $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfdisk.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(INC)\filesys\folder.h $(INC)\filesys\fdrmenus.h $(INC)\filesys\statbars.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\fdrhotky.obj: \
    $(INC)\setup.h \
    $(HLPINC)\linklist.h $(HLPINC)\winh.h $(HLPINC)\undoc.h \
    $(INC)\classes\xfldr.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(@B).c $(INC)\filesys\folder.h

$(OUTPUTDIR)\fdrmenus.obj: \
    $(INC)\setup.h \
    $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\except.h \
           $(HLPINC)\linklist.h $(HLPINC)\prfh.h $(HLPINC)\stringh.h $(HLPINC)\syssound.h \
           $(HLPINC)\threads.h $(HLPINC)\winh.h $(HLPINC)\undoc.h \
    $(INC)\dlgids.h \
    $(INC)\classes\xfobj.ih $(INC)\classes\xfldr.ih $(INC)\classes\xfdisk.ih \
    $(INC)\shared\common.h $(INC)\shared\contentmenus.h $(INC)\shared\notebook.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\config\partitions.h \
    $(INC)\filesys\folder.h $(INC)\filesys\object.h $(INC)\filesys\xthreads.h \
    $(INC)\startshut\shutdown.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\fdrnotebooks.obj: \
    $(INC)\setup.h \
    $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfldr.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
    $(INC)\filesys\xthreads.h \
    $(@B).c $(INC)\filesys\folder.h

$(OUTPUTDIR)\fdrsubclass.obj: \
    $(INC)\setup.h \
    $(HLPINC)\dosh.h $(HLPINC)\cnrh.h $(HLPINC)\except.h $(HLPINC)\linklist.h \
        $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfldr.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\contentmenus.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\filesys\fileops.h $(INC)\filesys\folder.h $(INC)\filesys\fdrmenus.h \
        $(INC)\filesys\statbars.h $(INC)\filesys\xthreads.h \
    $(@B).c $(INC)\filesys\folder.h

$(OUTPUTDIR)\fhandles.obj: \
    $(INC)\setup.h \
    $(HLPINC)\except.h $(HLPINC)\wphandle.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\filedlg.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h \
            $(HLPINC)\prfh.h  $(HLPINC)\stringh.h $(HLPINC)\threads.h \
            $(HLPINC)\winh.h $(HLPINC)\undoc.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfobj.ih $(INC)\classes\xfdisk.h \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\filesys\trash.h $(INC)\filesys\folder.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\fileops.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h \
            $(HLPINC)\prfh.h  $(HLPINC)\stringh.h $(HLPINC)\threads.h \
            $(HLPINC)\winh.h $(HLPINC)\undoc.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfobj.ih $(INC)\classes\xtrash.h $(INC)\classes\xtrashobj.h \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h $(INC)\shared\xsetup.h \
    $(INC)\filesys\trash.h $(INC)\filesys\xthreads.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\filesys.obj: \
    $(INC)\setup.h \
    $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\eah.h $(HLPINC)\except.h $(HLPINC)\prfh.h \
        $(HLPINC)\stringh.h $(HLPINC)\threads.h $(HLPINC)\winh.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfpgmf.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\notebook.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\filetype.obj: \
    $(INC)\setup.h \
    $(HLPINC)\cnrh.h $(HLPINC)\comctl.h $(HLPINC)\dosh.h $(HLPINC)\linklist.h \
            $(HLPINC)\prfh.h $(HLPINC)\standards.h $(HLPINC)\stringh.h \
            $(HLPINC)\winh.h $(HLPINC)\tree.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfdataf.ih $(INC)\classes\xfwps.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\cnrsort.h $(INC)\shared\common.h $(INC)\shared\notebook.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\folder.obj: \
    $(INC)\setup.h \
    $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\eah.h $(HLPINC)\except.h \
        $(HLPINC)\gpih.h $(HLPINC)\linklist.h $(HLPINC)\prfh.h $(HLPINC)\stringh.h \
        $(HLPINC)\syssound.h $(HLPINC)\threads.h $(HLPINC)\winh.h $(HLPINC)\undoc.h \
        $(HLPINC)\wphandle.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfldr.ih $(INC)\classes\xfobj.ih $(INC)\classes\xfdisk.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\cnrsort.h $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(INC)\filesys\fileops.h $(INC)\filesys\filesys.h $(INC)\filesys\fdrmenus.h \
        $(INC)\filesys\statbars.h $(INC)\filesys\xthreads.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\fops_bottom.obj: \
    $(INC)\setup.h \
    $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h $(HLPINC)\threads.h \
    $(INC)\classes\xfobj.ih $(INC)\classes\xfont.ih $(INC)\classes\xfontfile.ih \
        $(INC)\classes\xfontobj.ih $(INC)\classes\xtrash.ih $(INC)\classes\xtrashobj.ih \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\config\fonts.h \
    $(INC)\filesys\trash.h $(INC)\filesys\xthreads.h \
    $(@B).c $(MYOWNINC)\fileops.h

$(OUTPUTDIR)\fops_top.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\linklist.h $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfobj.ih $(INC)\classes\xtrash.h $(INC)\classes\xtrashobj.h \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(@B).c $(MYOWNINC)\fileops.h

$(OUTPUTDIR)\object.obj: \
    $(INC)\setup.h \
    $(HLPINC)\comctl.h $(HLPINC)\cnrh.h $(HLPINC)\dosh.h $(HLPINC)\except.h \
        $(HLPINC)\linklist.h $(HLPINC)\stringh.h $(HLPINC)\winh.h $(HLPINC)\wphandle.h \
        $(HLPINC)\xstring.h $(HLPINC)\undoc.h \
    $(INC)\classes\xfobj.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\cnrsort.h $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
    $(INC)\hook\xwphook.h \
    $(INC)\filesys\filesys.h $(INC)\filesys\folder.h \
    $(INC)\config\hookintf.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\program.obj: \
    $(INC)\setup.h \
    $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h $(HLPINC)\prfh.h \
        $(HLPINC)\stringh.h $(HLPINC)\threads.h $(HLPINC)\winh.h $(HLPINC)\xstring.h \
    $(INC)\classes\xfdataf.ih $(INC)\classes\xfobj.ih \
    $(INC)\dlgids.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h \
    $(INC)\filesys\object.h $(INC)\filesys\program.h \
    $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\refresh.obj: \
     $(INC)\setup.h \
     $(HLPINC)\dosh.h $(HLPINC)\except.h $(HLPINC)\linklist.h $(HLPINC)\threads.h \
     $(INC)\classes\xfldr.ih \
     $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
     $(INC)\filesys\folder.h \
     $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\statbars.obj: \
     $(INC)\setup.h \
     $(HLPINC)\dosh.h $(HLPINC)\linklist.h $(HLPINC)\prfh.h $(HLPINC)\stringh.h \
        $(HLPINC)\winh.h $(HLPINC)\xstring.h \
     $(INC)\classes\xfobj.ih \
     $(INC)\dlgids.h \
     $(INC)\shared\classes.h $(INC)\shared\common.h $(INC)\shared\notebook.h $(INC)\shared\wpsh.h \
     $(INC)\filesys\xthreads.h \
     $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\trash.obj: \
     $(INC)\setup.h \
     $(HLPINC)\dosh.h $(HLPINC)\linklist.h $(HLPINC)\except.h $(HLPINC)\linklist.h \
            $(HLPINC)\stringh.h $(HLPINC)\threads.h $(HLPINC)\tree.h $(HLPINC)\undoc.h $(HLPINC)\xstring.h \
     $(INC)\classes\xtrash.ih $(INC)\classes\xtrashobj.ih $(INC)\classes\xfobj.h \
     $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\notebook.h \
            $(INC)\shared\wpsh.h \
     $(INC)\filesys\fileops.h $(INC)\filesys\folder.h $(INC)\filesys\xthreads.h   \
     $(@B).c $(MYOWNINC)\$(@B).h

$(OUTPUTDIR)\xthreads.obj: \
    $(INC)\setup.h \
    $(HLPINC)\animate.h $(HLPINC)\comctl.h \
            $(HLPINC)\dosh.h $(HLPINC)\eah.h $(HLPINC)\except.h $(HLPINC)\linklist.h \
            $(HLPINC)\gpih.h $(HLPINC)\prfh.h $(HLPINC)\shapewin.h $(HLPINC)\stringh.h \
            $(HLPINC)\syssound.h $(HLPINC)\threads.h $(HLPINC)\winh.h \
    $(INC)\classes\xfldr.h $(INC)\classes\xtrash.h $(INC)\classes\xtrashobj.h \
    $(INC)\shared\common.h $(INC)\shared\kernel.h $(INC)\shared\wpsh.h \
    $(INC)\dlgids.h \
    $(INC)\hook\xwphook.h \
    $(INC)\filesys\fileops.h $(INC)\filesys\folder.h \
        $(INC)\filesys\xthreads.h $(INC)\filesys\trash.h \
    $(INC)\config\hookintf.h \
    $(@B).c $(MYOWNINC)\$(@B).h


