# $Id$ 

#
# makefile:
#       makefile for TOOLS\WPSRESET directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    main makefile
#
#       Input:          files in this directory
#
#       Output:         wpsreset.exe, copied to parent directory (TOOLS\).
#
#       Edit "setup.in" to set up the make process.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
# !include ..\..\setup.in

OUTPUTDIR = ..\..\bin\exes

INCLUDE = $(INCLUDE);..\..\include

# First define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize in inference
# rules.

.SUFFIXES: .c .obj .dll .idl .h .ih .rc .res

# ICC environment variable for compiler options.
ICC     =
# /Wcnd- /Wgen- /Wcpy /Wobs- /Word- /Wcns+ /Wcnv-

# Compiler macro. This is used for each .OBJ file to be created.
CC      = icc /c /Ge /Gm- /Gd- /Re /O- /Oi- /ol- /q+ /Se /ss /Tn /w2 /Wcls+ /Wcpy+ /Wcmp+ /Wobs+ /Word+ /Wpro+ /Wrea+ /Wret+ /Wuni+ /Wuse+

# Some VisualAge C++ compiler options explained:
# /c:   compile only, no link (we'll call the linker explicitly)
# /fi+: precompile header files
# /gd-: link runtime statically
# /ge-: create DLL code
# /gi+: fast integer execution
# /Gl+: remove unreferenced functions (when comp.+link in 1 step)
# /gm+: multithread libraries
# /kc+: produce preprocessor warnings
# /o+:  optimization (inlining etc.)
# /oi-: no inlining (?)
# /ol+: use intermediate linker
# /q+:  suppress icc logo
# /Re : don't use subsystem libraries (!= Rn)
# /se:  all language extensions
# /si+: allow use of precompiled header files
# /ss:  allow double slashes comments in C too
# /ti+: debug code
# /tdp: compile everything as C++, even if it has a .C suffix
# /Tn:  add source line numbers to object files (for mapfile)
# /Wcls: class problems
# /Wcnd: conditional exprs problems (= / == etc.)
# /Wcmp: possible unsigned comparison redundancies
# /Wcns: operations involving constants
# /Wcnv: conversions
# /Wcpy: copy constructor problems
# /Wgen: generic debugging msgs
# /Wobs: obsolete features
# /Word: unspecified evaluation order
# /Wpar: list not-referenced parameters (annoying)
# /Wppc: list possible preprocessor problems (.h dependencies)
# /Wpro: warn if funcs have not been prototyped
# /Wrea: mark code that cannot be reached
# /Wret: check consistency of return levels
# /Wuni: uninitialized variables
# /Wuse: unused variables
# /w2:   produce error and warning messages, but no infos

LINK = ilink /map:$(OUTPUTDIR) /nologo /linenumbers /optfunc

# Main targets
all: $(OUTPUTDIR)\wpsreset.exe
    @echo ----- Leaving $(MAKEDIR)

# 1) files in this directory
.c.{$(OUTPUTDIR)}.obj:
        @ echo Compiling $(@B).c:
!ifndef PRECH
        $(CC) /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!else
        $(CC) /fi"$(PRECH)\$*.pch" /si"$(PRECH)\$*.pch" /Fo$(OUTPUTDIR)\$(@B).obj $(@B).c
!endif

#
# Now define dependencies for all our .obj files by telling NMAKE
# which .OBJ file depends on what. For each .obj we define what files
# will enforce a recompilation when they've been changed.
#

# .OBJs for executables in this directory
$(OUTPUTDIR)\wpsreset.obj: $(@B).c

# "wpsreset" executable: link and copy to parent (TOOLS) directory
$(OUTPUTDIR)\wpsreset.exe: $(OUTPUTDIR)\$(@B).obj makefile
          $(LINK) /OUT:$(OUTPUTDIR)\$(@B).exe /PMTYPE:VIO $(OUTPUTDIR)\$(@B).obj
          cmd.exe /c copy $(OUTPUTDIR)\$(@B).exe ..


