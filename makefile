
#
# makefile:
#       This is XWorkplace's main makefile.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    nowhere, this is the main makefile.
#                       This recurses into the subdirectories.
#
#       Input:          specify the target(s) to be made, which can be:
#
#                       --  "dep": create dependencies for all makefiles.
#                           Run this first before the regular "all"
#                           target.
#
#                       --  "all" (default): build XFLDR.DLL, XWPHOOK.DLL,
#                           XWPDAEMN.EXE, plus the default plugin DLLs.
#
#                       --  "really_all": "all" plus external EXEs
#                           (Treesize, Netscape DDE, et al) plus NLS
#                           specified by the XWP_LANG_CODE variable,
#                           which defaults to "001" (setup.in).
#
#                       The following subtargets exist (which get called
#                       by the "all" or "really_all" targets):
#
#                       --  nls: compile $(XWP_LANG_CODE)\ directory
#                       --  tools: compile TOOLS\ directory
#                       --  idl: update SOM headers (include\classes\*)
#                       --  cpl_main: compile *.c files for DLLs, no link
#                       --  link: link bin\*.obj to DLLs and copy to XWorkplace
#                                 install directory
#
#                       Use "nmake -a [<targets>] to _re_build the targets,
#                       even if they are up to date.
#
#                       Other special targets not used by "all" or "really_all"
#                       (these must be specified explicitly on the nmake
#                       command line):
#
#                       --  dlgedit: invoke dialog editor on NLS DLL
#                       --  release: create/update release tree in directory
#                           specified by XWPRELEASE; this invokes "really_all"
#                           in turn.
#
#       Environment:    You MUST set a number of environment variables before
#                       compiling. See PROGREF.INF.
#
#       Output:         All XWorkplace Files code files. This calls the other
#                       makefiles in the subdirectories.
#
#                       Output files are first created in bin\ (which is
#                       created if it doesn't exist), then copied to
#                       $(XWPRUNNING), which must be defined externally or
#                       thru "setup.in".
#
#       Edit "setup.in" to set up the make process (compilation flags etc.).
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
!include setup.in

# VARIABLES
# ---------

# objects.in defines the .OBJ files to be compiled.
!include objects.in

OUTPUTDIR = $(XWP_OUTPUT_ROOT)

# The OBJS macro contains all the .OBJ files which have been
# created from the files in MAIN\. The definitions below are
# from the objects.in include file.
OBJS = \
$(XFLDR_OBJS_CLASSES) \
$(XFLDR_OBJS_SHARED) \
$(XFLDR_OBJS_CONFIG) \
$(XFLDR_OBJS_FILESYS) \
$(XFLDR_OBJS_MEDIA) \
$(XFLDR_OBJS_STARTSHUT) \
$(XFLDR_OBJS_XCENTER)

OBJS_ANICLASSES = $(XWP_OUTPUT_ROOT)\anand.obj $(XWP_OUTPUT_ROOT)\anos2ptr.obj $(XWP_OUTPUT_ROOT)\anwani.obj $(XWP_OUTPUT_ROOT)\anwcur.obj
OBJS_ANICONVERT = $(XWP_OUTPUT_ROOT)\cursor.obj $(XWP_OUTPUT_ROOT)\pointer.obj $(XWP_OUTPUT_ROOT)\script.obj
#$(XWP_OUTPUT_ROOT)\expire.obj
OBJS_ANIDLL = $(XWP_OUTPUT_ROOT)\dll.obj $(XWP_OUTPUT_ROOT)\dllbin.obj
OBJS_ANIANI = $(XWP_OUTPUT_ROOT)\mptranim.obj $(XWP_OUTPUT_ROOT)\mptrcnr.obj $(XWP_OUTPUT_ROOT)\mptredit.obj $(XWP_OUTPUT_ROOT)\mptrlset.obj \
    $(XWP_OUTPUT_ROOT)\mptrpag1.obj $(XWP_OUTPUT_ROOT)\mptrppl.obj $(XWP_OUTPUT_ROOT)\mptrprop.obj $(XWP_OUTPUT_ROOT)\mptrptr.obj $(XWP_OUTPUT_ROOT)\mptrset.obj \
    $(XWP_OUTPUT_ROOT)\mptrutil.obj $(XWP_OUTPUT_ROOT)\mptrfile.obj $(XWP_OUTPUT_ROOT)\wpamptr.obj

!ifdef ANIMATED_MOUSE_POINTERS
ANIOBJS = $(OBJS_ANICLASSES) $(OBJS_ANICONVERT) $(OBJS_ANIANI) $(OBJS_ANIDLL)
!else
ANIOBJS =
!endif

# The HLPOBJS macro contains all the .OBJ files which have been
# created from the files in HELPERS\. You probably won't have to change this.
HLPOBJS = $(XWP_OUTPUT_ROOT)\helpers.lib

# The following macros contains the .OBJ files for the XCenter plugins.
DISKFREEOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_diskfree.obj $(PMPRINTF_LIB)
WINLISTOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_winlist.obj $(PMPRINTF_LIB)
MONITOROBJS = $(XWP_OUTPUT_ROOT)\widgets\w_monitors.obj $(PMPRINTF_LIB)
SENTINELOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_sentinel.obj $(PMPRINTF_LIB) libs\win32k.lib
HEALTHOBJS = $(XWP_OUTPUT_ROOT)\widgets\xwHealth.obj $(PMPRINTF_LIB)
SAMPLEOBJS = $(XWP_OUTPUT_ROOT)\widgets\____sample.obj $(PMPRINTF_LIB)

D_CDFSOBJS = $(XWP_OUTPUT_ROOT)\widgets\d_cdfs.obj $(PMPRINTF_LIB)

# objects for XDEBUG.DLL (debugging only)
DEBUG_OBJS = $(XWP_OUTPUT_ROOT)\xdebug.obj $(XWP_OUTPUT_ROOT)\xdebug_folder.obj

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize.
.SUFFIXES: .obj .dll .exe .h .rc .res

# The LIBS macro contains all the .LIB files, either from the compiler or
# others, which are needed for this project:
#   somtk       is the SOM toolkit lib
#   pmprintf    is for debugging
# The other OS/2 libraries are used by default.
LIBS = $(TKBASE)\som\lib\somtk.lib $(PMPRINTF_LIB)

# some variable strings to pass to sub-nmakes
SUBMAKE_PASS_STRING = "PROJECT_BASE_DIR=$(PROJECT_BASE_DIR)" "PROJECT_INCLUDE=$(PROJECT_INCLUDE)"

# store current directory so we can change back later
CURRENT_DIR = $(MAKEDIR)

# REGULAR MAIN TARGETS
# --------------------

# "all": default, does a regular compile, but leaves
# out the not-so-important stuff such as netscdee and treesize.
# Basically, this is for updating XFLDR.DLL and the hook/daemon
# for speed.
all:            compile_all link
    @echo ----- Leaving $(MAKEDIR)
    @echo Yo, done!

# "really_all" references "all" and compiles really everything.
# This must be used for the release version.
really_all:     compile_really_all tools link nls
    @echo ----- Leaving $(MAKEDIR)
    @echo Yo, done!

# "dep": create dependencies.
dep:
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir src (dep)
    @cd src
    $(MAKE) -nologo dep "SUBTARGET=dep" "RUNDEPONLY=1" "REALLYALL=1"
    @cd $(CURRENT_DIR)
    @echo $(MAKEDIR)\makefile [$@]: Going for src\helpers (DLL version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo dep "NOINCLUDEDEPEND=1" $(SUBMAKE_PASS_STRING)
    @cd $(CURRENT_DIR)
    @echo ----- Leaving $(MAKEDIR)
    @echo Yo, done!

# "COMPILE" PSEUDOTARGETS
# -----------------------

# The following are invoked via "all" or "really_all"
# to call src\makefile with SUBTARGET=all, which will
# in turn invoke nmake all to the src\ subdirectories.

# idl always gets invoked first to check whether
# src\classes\*.c needs to be refreshed from idl\*.idl.
idl:
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir idl
    @cd idl
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..

helpers:
# helpers:
# this branches over to the xwphelpers source tree,
# which is prepared for this. The helpers.lib file
# is created in $(XWP_OUTPUT_ROOT) then.
    @echo $(MAKEDIR)\makefile [$@]: Going for src\helpers (DLL version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(XWP_OUTPUT_ROOT)" "CC_HELPERS=$(CC_HELPERS_DLL)"
# according to VAC++ user guide, we need to use /ge+ for libs
# even if the lib will be linked to a DLL
    @cd $(CURRENT_DIR)

helpers_exe_mt:
# helpers_exe_mt:
# same as the above, but this builds a multithread lib for EXEs
# in $(XWP_OUTPUT_ROOT)\exe_mt\ instead.
    @echo $(MAKEDIR)\makefile [$@]: Going for src\helpers (EXE MT version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(XWP_OUTPUT_ROOT)\exe_mt" "CC_HELPERS=$(CC_HELPERS_EXE_MT)"
    @cd $(CURRENT_DIR)

# compile_all: compile main (without treesize etc.)
compile_all: idl helpers helpers_exe_mt
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir src
    @cd src
    $(MAKE) -nologo "SUBTARGET=all"
    @cd $(CURRENT_DIR)

# compile_really_all: compile really_all
compile_really_all: idl helpers helpers_exe_mt
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir src (REALLY_ALL)
    @cd src
    $(MAKE) -nologo "SUBTARGET=all" "REALLYALL=1"
    @cd $(CURRENT_DIR)

tools:
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir tools
    @cd tools
    $(MAKE) -nologo "SUBTARGET=all" "MAINMAKERUNNING=YES"
    @cd ..

xwpsecurity:
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir src\xwpsec_ring0
    @cd src\xwpsec_ring0
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir src\XWPShell
    @cd src\XWPShell
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

nls:
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir $(XWP_LANG_CODE)\dll
    @cd $(XWP_LANG_CODE)\dll
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir $(XWP_LANG_CODE)\inf.$(XWP_LANG_CODE)
    @cd inf.$(XWP_LANG_CODE)
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir $(XWP_LANG_CODE)\misc
    @cd misc
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile [$@]: Going for subdir $(XWP_LANG_CODE)\xwphelp2
    @cd xwphelp2
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..


# LINKER PSEUDOTARGETS
# --------------------

link: $(XWPRUNNING)\bin\xfldr.dll \
      $(XWPRUNNING)\bin\xwpres.dll \
      $(XWPRUNNING)\plugins\xcenter\diskfree.dll \
      $(XWPRUNNING)\plugins\xcenter\monitors.dll \
      $(XWPRUNNING)\plugins\xcenter\winlist.dll \
      $(XWPRUNNING)\plugins\xcenter\sentinel.dll \
      $(XWPRUNNING)\plugins\xcenter\xwHealth.dll \
      $(XWPRUNNING)\plugins\xcenter\sample.dll \
      $(XWPRUNNING)\plugins\drvdlgs\d_cdfs.dll \
#      $(XWPRUNNING)\bin\xwphook.dll \
#      $(XWPRUNNING)\bin\xwpdaemn.exe
#      $(XWPRUNNING)\bin\xwpfonts.fon
#      $(XWPRUNNING)\bin\xdebug.dll

#
# generic inference rules for copying and stuff
#

{$(MODULESDIR)}.dll{$(XWPRUNNING)\plugins\xcenter}.dll:
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

{$(MODULESDIR)}.dll{$(XWPRUNNING)\plugins\drvdlgs}.dll:
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\drvdlgs
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\drvdlgs

# Finally, define rules for linking the target DLLs and EXEs
# This uses the $OBJS and $HLPOBJS macros defined at the top.
#
# The actual targets are the DLLs and EXEs in the XWorkplace
# installation directory. We create the target in bin\ first
# and then copy it thereto.

#
# Linking XFLDR.DLL
#
$(XWPRUNNING)\bin\xfldr.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!ifdef DYNAMIC_TRACE
        @echo $(MAKEDIR)\makefile [$@]: Creating TRACE files for $(@B).dll
        maptsf $(@B).map /MAJOR=255 /LOGSTACK=32 /LOGRETURN > $(@B).tsf
        trcust $(@B).tsf /I /L=bin\$(@B).dll /node /M=$(@B).map
        @echo $(MAKEDIR)\makefile [$@]: Done creating TRACE files for $(@B).dll
        cmd.exe /c copy $(@B).tdf $(XWPRUNNING)\bin
        cmd.exe /c del $(@B).tdf
        cmd.exe /c copy TRC00FF.TFF $(DYNAMIC_TRACE):\OS2\SYSTEM\TRACE
        cmd.exe /c del TRC00FF.TFF
!endif

MODDEFFILE = \
!ifdef XWPLITE
src\shared\xwp_lite.def
!else
src\shared\xwp.def
!endif

# update DEF file if buildlevel has changed
src\shared\xwp.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) main module"

$(MODULESDIR)\xfldr.dll: $(OBJS) $(HLPOBJS) $(ANIOBJS) $(MODDEFFILE) objects.in
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) @<<$(TEMP)\XFLDR.LNK
/OUT:$@ $(MODDEFFILE) $(OBJS) $(HLPOBJS) $(ANIOBJS) $(LIBS)
<<KEEP
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)
        cmd.exe /c tools\raisebld.cmd include\build.h

#
# Linking XWPRES.DLL
#

$(XWPRUNNING)\bin\xwpres.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
src\shared\xwpres.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) resources module"

$(MODULESDIR)\xwpres.dll: src\shared\xwpres.def $(XWP_OUTPUT_ROOT)\xwpres.res $(XWP_OUTPUT_ROOT)\dummyfont.obj
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK_ALWAYSPACK) /OUT:$@ src\shared\xwpres.def $(XWP_OUTPUT_ROOT)\dummyfont.obj
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        $(RC) ..\xwpres.res $(@B).dll
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

# XCENTER PLUGINS LINKER PSEUDOTARGETS
# ------------------------------------

#
# Linking WINLIST.DLL
#

# update DEF file if buildlevel has changed
src\widgets\winlist.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) window-list plugin DLL"

$(MODULESDIR)\winlist.dll: $(WINLISTOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def $(WINLISTOBJS)
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking DISKFREE.DLL
#
$(XWPRUNNING)\plugins\xcenter\diskfree.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\w_diskfree.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) diskfree plugin DLL"

$(MODULESDIR)\diskfree.dll: $(DISKFREEOBJS) src\widgets\w_diskfree.def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\w_diskfree.def @<<
$(DISKFREEOBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking MONITORS.DLL
#
$(XWPRUNNING)\plugins\xcenter\monitors.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\monitors.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) monitors plugin DLL"

$(MODULESDIR)\monitors.dll: $(MONITOROBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<
$(MONITOROBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking sentinel.DLL
#
$(XWPRUNNING)\plugins\xcenter\sentinel.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\sentinel.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) memory sentinel plugin DLL"

$(MODULESDIR)\sentinel.dll: $(SENTINELOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def $(SENTINELOBJS)
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking xwHealth.DLL
#
$(XWPRUNNING)\plugins\xcenter\xwHealth.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\xwHealth.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) xwHealth plugin DLL"

$(MODULESDIR)\xwHealth.dll: $(HEALTHOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<
$(HEALTHOBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking SAMPLE.DLL
#
$(XWPRUNNING)\plugins\xcenter\sample.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\sample.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) sample plugin DLL"

$(MODULESDIR)\sample.dll: $(SAMPLEOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<
$(SAMPLEOBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

# DRIVER PLUGINS LINKER PSEUDOTARGETS
# -----------------------------------
#
# Linking D_CDFS.DLL
#

# update DEF file if buildlevel has changed
src\widgets\d_cdfs.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) CDFS.IFS driver plugin DLL"

$(MODULESDIR)\d_cdfs.dll: $(D_CDFSOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile [$@]: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<
$(D_CDFSOBJS)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking XWPFONTS.FON
#
$(XWPRUNNING)\bin\xwpfonts.fon: $(MODULESDIR)\$(@B).fon
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).fon $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
src\shared\xwpfonts.def: include\bldlevel.h makefile
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "$(XWPNAME) bitmap fonts"

$(MODULESDIR)\xwpfonts.fon: $(XWP_OUTPUT_ROOT)\$(@B).res
        @echo $(MAKEDIR)\makefile [$@]: Linking $(MODULESDIR)\$(@B).fon
        $(LINK) /OUT:$(MODULESDIR)\$(@B).dll src\shared\$(@B).def
        @cd $(MODULESDIR)
# rename manually because otherwise the linker warns
#        cmd.exe /c del $(@B).fon
        cmd.exe /c ren $(@B).dll $(@B).fon
        $(RC) ..\$(@B).res $(@B).fon
        @cd $(CURRENT_DIR)

#
# XDEBUG.DLL
#
$(XWPRUNNING)\bin\xdebug.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin

$(MODULESDIR)\xdebug.dll: src\shared\$(@B).def $(DEBUG_OBJS) $(HLPOBJS) $(XWP_OUTPUT_ROOT)\wpsh.obj
        $(LINK) /OUT:$(MODULESDIR)\$(@B).dll src\shared\$(@B).def $(DEBUG_OBJS) $(HLPOBJS) $(LIBS) $(XWP_OUTPUT_ROOT)\wpsh.obj

#
# Special target "dlgedit": this is not called by "all",
# but must be set on the NMAKE command line.
# Note that you need DLGEDIT.EXE from the Toolkit for this.
#

dlgedit:
# added (UM 99-10-24)
    @echo $(MAKEDIR)\makefile [$@]: Calling DLGEDIT.EXE
    @cd $(XWP_LANG_CODE)\dll
# rebuild RES file in bin
    @nmake -nologo all "MAINMAKERUNNING=YES"
# copy RES file to 001\dll so dlgedit finds it
    @cmd.exe /c copy $(PROJECT_OUTPUT_DIR)\xfldr$(XWP_LANG_CODE).res
    @cmd.exe /c copy ..\..\include\dlgids.h
# invoke DLGEDIT
    dlgedit xfldr$(XWP_LANG_CODE).res
# move newly created RES file back to \bin
    @cmd.exe /c copy xfldr$(XWP_LANG_CODE).res ..\bin
    @cmd.exe /c del xfldr$(XWP_LANG_CODE).res
    @cmd.exe /c del dlgids.h
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..\..

dlgedit049:
# added V0.9.1 (99-12-19) [umoeller]
    @echo $(MAKEDIR)\makefile [$@]: Calling DLGEDIT.EXE
    @cd 049_de\dll
# rebuild RES file in bin
    @nmake -nologo all "MAINMAKERUNNING=YES"
# copy RES file to frontend.res so dlgedit finds it
    @cmd.exe /c copy ..\..\bin\xfldr049.res
    @cmd.exe /c copy ..\..\include\dlgids.h
# invoke DLGEDIT
    dlgedit xfldr049.res
# move newly created RES file back to \bin
    @cmd.exe /c copy xfldr049.res ..\bin
    @cmd.exe /c del xfldr049.res
    @cmd.exe /c del dlgids.h
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..\..

#
# Special target "release": this is not called by "all",
# but must be set on the NMAKE command line.
#

release: really_all
# 1) main dir
!ifndef XWPRELEASE
!error XWPRELEASE must be set before calling "make release". Terminating.
!endif
# nuke old directories
# create directories
!if [@md $(XWPRELEASE) 2> NUL]
!endif
!ifndef XWPLITE
!if [@md $(XWPRELEASE_HEALTH) 2> NUL]
!endif
!endif
!if [@md $(XWPRELEASE_MAIN) 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS) 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLSDOC) 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAP) 2> NUL]
!endif
    @echo $(MAKEDIR)\makefile [$@]: Now copying files to $(XWPRELEASE).
!ifndef XWPLITE
    $(COPY) release\COPYING $(XWPRELEASE_MAIN)
    $(COPY) release\install.cmd $(XWPRELEASE_MAIN)
    $(COPY) release\file_id.diz $(XWPRELEASE_MAIN)
    $(COPY) release\install.ico $(XWPRELEASE_MAIN)
    $(COPY) $(XWP_LANG_CODE)\readme $(XWPRELEASE_NLSDOC)
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).inf $(XWPRELEASE_NLSDOC)
    $(COPY) BUGS $(XWPRELEASE_NLSDOC)
    $(COPY) FEATURES $(XWPRELEASE_NLSDOC)
    $(COPY) cvs.txt $(XWPRELEASE_MAIN)
!else
    $(COPY) _private\status.txt $(XWPRELEASE_MAIN)
!endif
#
# 2) bin
#    a) kernel
!if [@md $(XWPRELEASE_MAIN)\bin 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS)\bin 2> NUL]
!endif
!ifndef XWPLITE
    $(COPY) release\bin\alwssort.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\bm-lvm.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\bootmgr.cmd $(XWPRELEASE_MAIN)\bin
!endif
    $(COPY) release\bin\defdetls.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\deficon.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\deftree.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\iconorm.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\icosmall.cmd $(XWPRELEASE_MAIN)\bin
!ifndef XWPLITE
    $(COPY) release\bin\newobj.cmd $(XWPRELEASE_MAIN)\bin
!endif
    $(COPY) release\bin\packtree.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\showall.cmd $(XWPRELEASE_MAIN)\bin
!ifndef XWPLITE
    $(COPY) release\bin\xhelp.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\xshutdwn.cmd $(XWPRELEASE_MAIN)\bin
    $(COPY) release\bin\xshutdwn.ico
!endif
#    $(COPY) release\bin\icons.dll
!ifndef XWPLITE
    $(COPY) release\bin\files.txt
!endif
    $(COPY) $(MODULESDIR)\xfldr.dll $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xfldr.sym $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwpres.dll $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwphook.dll $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwphook.sym $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\netscdde.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\treesize.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xfix.exe     $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwpdaemn.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xfldr.sym $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwpdaemn.sym $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwphook.sym $(XWPRELEASE_MAIN)\bin
!ifndef XWPLITE
    $(COPY) $(MODULESDIR)\repclass.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\wpsreset.exe $(XWPRELEASE_MAIN)\bin
!endif
#    b) NLS
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).dll $(XWPRELEASE_NLS)\bin
#    $(COPY) $(XWP_LANG_CODE)\misc\*.sgs $(XWPRELEASE_NLS)\bin
#    b) mapfiles
    $(COPY) $(MODULESDIR)\*.map $(XWPRELEASE_MAP)
#
# 3) bootlogo
!ifndef XWPLITE
!if [@md $(XWPRELEASE_MAIN)\bootlogo 2> NUL]
!endif
    $(COPY) release\bootlogo\* $(XWPRELEASE_MAIN)\bootlogo
!endif
#
# 4) help
!if [@md $(XWPRELEASE_NLS)\help 2> NUL]
!endif
#    $(COPY) $(XWP_LANG_CODE)\misc\xfldr$(XWP_LANG_CODE).tmf $(XWPRELEASE_NLS)\help
    $(COPY) $(XWPRUNNING)\help\xfldr$(XWP_LANG_CODE).tmf $(XWPRELEASE_NLS)\help
!ifndef XWPLITE
    $(COPY) $(XWP_LANG_CODE)\misc\drvrs$(XWP_LANG_CODE).txt $(XWPRELEASE_NLS)\help
    $(COPY) $(XWP_LANG_CODE)\misc\xfcls$(XWP_LANG_CODE).txt $(XWPRELEASE_NLS)\help
!endif
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).hlp $(XWPRELEASE_NLS)\help
!ifndef XWPLITE
# 5) themes
#!if [@md $(XWPRELEASE_MAIN)\icons 2> NUL]
#!endif
#    $(COPY) release\icons\* $(XWPRELEASE_MAIN)\icons
#!endif
!if [@md $(XWPRELEASE_MAIN)\themes 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\themes\warp3 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\themes\warp4 2> NUL]
!endif
    $(COPY) release\themes\warp3\* $(XWPRELEASE_MAIN)\themes\warp3
    $(COPY) release\themes\warp4\* $(XWPRELEASE_MAIN)\themes\warp4
!endif
# 6) install
!if [@md $(XWPRELEASE_MAIN)\install 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS)\install 2> NUL]
!endif
#    $(COPY) release\install\xwpusers.acc $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\deinst.cmd $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\delobjs.cmd $(XWPRELEASE_MAIN)\install
!ifdef XWPLITE
    $(COPY) release\install\freshini_lite.cmd $(XWPRELEASE_MAIN)\install\freshini.cmd
!else
    $(COPY) release\install\freshini.cmd $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\od.cmd $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\soundoff.cmd $(XWPRELEASE_MAIN)\install
#   $(COPY) release\install\test.cmd $(XWPRELEASE_MAIN)\install
#    $(COPY) release\install\xfolder.ico $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\xwp.ico $(XWPRELEASE_MAIN)\install
    $(COPY) release\install\xwp_o.ico $(XWPRELEASE_MAIN)\install
!endif
#    $(COPY) release\install\xwpusers.xml $(XWPRELEASE_MAIN)\install
!ifndef XWPLITE
    $(COPY) $(XWP_LANG_CODE)\misc\crobj001.cmd $(XWPRELEASE_NLS)\install\crobj001.cmd
!else
    $(COPY) $(XWP_LANG_CODE)\misc\crobj001_lite.cmd $(XWPRELEASE_NLS)\install\crobj001.cmd
!endif
!ifndef XWPLITE
    $(COPY) $(XWP_LANG_CODE)\misc\instl001.cmd $(XWPRELEASE_NLS)\install\instl001.cmd
!else
    $(COPY) $(XWP_LANG_CODE)\misc\instl001_lite.cmd $(XWPRELEASE_NLS)\install\instl001.cmd
!endif
!ifndef XWPLITE
    $(COPY) $(XWP_LANG_CODE)\misc\sound001.cmd $(XWPRELEASE_NLS)\install
    $(COPY) $(XWP_LANG_CODE)\misc\*.msg $(XWPRELEASE_NLS)\install
!endif
# 7) wav
!if [@md $(XWPRELEASE_MAIN)\wav 2> NUL]
!endif
    $(COPY) release\wav\* $(XWPRELEASE_MAIN)\wav
    @echo $(MAKEDIR)\makefile [$@]: Done copying files.
# 8) plugins
!if [@md $(XWPRELEASE_MAIN)\plugins 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\plugins\xcenter 2> NUL]
!endif
!ifndef XWPLITE
!if [@md $(XWPRELEASE_MAIN)\plugins\drvdlgs 2> NUL]
!endif
!endif
!ifndef XWPLITE
!if [@md $(XWPRELEASE_HEALTH)\plugins 2> NUL]
!endif
!if [@md $(XWPRELEASE_HEALTH)\plugins\xcenter 2> NUL]
!endif
!endif
    $(COPY) $(MODULESDIR)\diskfree.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\diskfree.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\monitors.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\monitors.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\sentinel.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\sentinel.sym $(XWPRELEASE_MAIN)\plugins\xcenter
!ifndef XWPLITE
    $(COPY) $(MODULESDIR)\d_cdfs.dll $(XWPRELEASE_MAIN)\plugins\drvdlgs
    $(COPY) $(MODULESDIR)\d_cdfs.sym $(XWPRELEASE_MAIN)\plugins\drvdlgs
    $(COPY) $(MODULESDIR)\xwHealth.dll $(XWPRELEASE_HEALTH)\plugins\xcenter
    $(COPY) $(MODULESDIR)\xwHealth.sym $(XWPRELEASE_HEALTH)\plugins\xcenter
!endif
# 9) toolkit
!if [@md $(XWPRELEASE_MAIN)\toolkit 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\toolkit\shared 2> NUL]
!endif
!ifndef XWPLITE
!if [@md $(XWPRELEASE_MAIN)\toolkit\config 2> NUL]
!endif
!endif
    $(COPY) src\widgets\miniwdgt.c $(XWPRELEASE_MAIN)\toolkit
    $(COPY) include\shared\center.h $(XWPRELEASE_MAIN)\toolkit\shared
!ifndef XWPLITE
    $(COPY) include\config\drivdlgs.h $(XWPRELEASE_MAIN)\toolkit\config
!endif
    @echo $(MAKEDIR)\makefile [$@]: Done copying files.
# now go shrink executables V0.9.13 (2001-06-17) [umoeller]
!ifndef XWP_DEBUG
!ifdef LXLITEPATH
    $(LXLITEPATH)\lxlite \
$(XWPRELEASE_MAIN)\bin\*.dll \
$(XWPRELEASE_MAIN)\bin\*.exe \
$(XWPRELEASE_NLS)\bin\*.dll
    $(LXLITEPATH)\lxlite \
$(XWPRELEASE_MAIN)\plugins\xcenter\*.dll \
!ifndef XWPLITE
$(XWPRELEASE_HEALTH)\plugins\xcenter\*.dll
!endif
!endif
!endif

#
# Special target "warpin": this is not called by "all",
# but must be set on the NMAKE command line.
# Will work only on my private system.
#

warpin: release
    @echo $(MAKEDIR)\makefile [$@]: Building WPI from $(XWPRELEASE).
    cd $(XWPRELEASE)
    cd ..
    warpin_001.cmd
    cd $(CURRENT_DIR)

#
# Special target "transfer": this is not called by "all",
# but must be set on the NMAKE command line.
# Will work only on my private system.
#

transfer: warpin
    @echo $(MAKEDIR)\makefile [$@]: Transferring WPI from $(XWPRELEASE).
    cd $(XWPRELEASE)
    cd ..
    sendfile laptop xwp-temp.wpi D:\install\xwp-transferred.wpi
    cd $(CURRENT_DIR)

