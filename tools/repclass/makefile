#
# makefile:
#       makefile for TOOLS\RECLASS directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    main makefile
#
#       Input:          files in this directory
#
#       Output:         repclass.exe, copied to modules directory.
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

.SUFFIXES: .c .obj .dll .idl .h .ih .rc .res

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
OUTPUTDIR = ..\..\bin\exe_st
MODULESDIR = ..\..\bin\modules

# create output directory
!if [@md ..\..\bin 2> NUL]
!endif
!if [@md $(OUTPUTDIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

# The main target:
# If we're called from the main makefile, MAINMAKERUNNING is defined,
# and we'll set $(OBJS) as our targets (which will go on).
# Otherwise, we call the main makefile, which will again call ourselves later.
all: $(MODULESDIR)\repclass.exe
    @echo ----- Leaving $(MAKEDIR)

# main include path
INC = ..\..\include
# helpers include path
HLPINC = $(HELPERS_BASE)\include\helpers
# our own corresponding include path
MYOWNINC = $(INC)\hook

# 1) files in this directory
.c.{$(OUTPUTDIR)}.obj:
        @ echo Compiling $(@B).c:
!ifndef PRECH
# precompiled headers disabled:
        $(CC_EXE_ST) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
        $(CC_EXE_ST) /fi"$(PRECH)\$(@B).pch" /si"$(PRECH)\$(@B).pch" -I$(INC) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

#
# Now define dependencies for all our .obj files by telling NMAKE
# which .OBJ file depends on what. For each .obj we define what files
# will enforce a recompilation when they've been changed.
#

# .OBJs for executables in this directory
$(OUTPUTDIR)\repclass.obj: $(@B).c

# "repclass" executable
$(MODULESDIR)\repclass.exe: $(OUTPUTDIR)\$(@B).obj makefile
          $(LINK) /OUT:$(MODULESDIR)\$(@B).exe /PMTYPE:VIO $(OUTPUTDIR)\$(@B).obj
