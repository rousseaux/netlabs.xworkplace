
#
# makefile:
#       makefile for main directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    nowhere, maybe MAKE.CMD. Main makefile.
#                       This recurses into the subdirectories.
#
#       Input:          specify the target(s) to be made, which can be:
#
#                       --  "dep": create dependencies for all makefiles.
#                           Run this first before the regular "all"
#                           target.
#
#                       --  "all" (default): build XFLDR.DLL,
#                           XWPHOOK.DLL, XWPDAEMN.EXE.
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

# MODULESDIR is used for mapfiles and final module (DLL, EXE) output.
# PROJECT_OUTPUT_DIR has been set by setup.in based on the environment.
MODULESDIR=$(PROJECT_OUTPUT_DIR)\modules
!if [@echo ---^> MODULESDIR is $(MODULESDIR)]
!endif

!ifdef XWP_DEBUG
!if [@echo ---^> Debug build has been enabled.]
!endif
!else
!if [@echo ---^> Building release code (debugging disabled).]
!endif
!endif

# create output directory
!if [@md $(PROJECT_OUTPUT_DIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

# VARIABLES
# ---------

# The OBJS macro contains all the .OBJ files which have been
# created from the files in MAIN\.
OBJS = \
# code from classes\
$(XWP_OUTPUT_ROOT)\xcenter.obj \
$(XWP_OUTPUT_ROOT)\xclslist.obj \
$(XWP_OUTPUT_ROOT)\xfdataf.obj \
$(XWP_OUTPUT_ROOT)\xfdesk.obj \
$(XWP_OUTPUT_ROOT)\xfdisk.obj \
$(XWP_OUTPUT_ROOT)\xfldr.obj \
$(XWP_OUTPUT_ROOT)\xfont.obj \
$(XWP_OUTPUT_ROOT)\xfontfile.obj \
$(XWP_OUTPUT_ROOT)\xfontobj.obj \
$(XWP_OUTPUT_ROOT)\xfobj.obj \
$(XWP_OUTPUT_ROOT)\xfpgmf.obj \
$(XWP_OUTPUT_ROOT)\xfsys.obj \
$(XWP_OUTPUT_ROOT)\xfwps.obj \
$(XWP_OUTPUT_ROOT)\xfstart.obj \
$(XWP_OUTPUT_ROOT)\xmmcdplay.obj \
$(XWP_OUTPUT_ROOT)\xmmvolume.obj \
$(XWP_OUTPUT_ROOT)\xtrash.obj \
$(XWP_OUTPUT_ROOT)\xtrashobj.obj \
$(XWP_OUTPUT_ROOT)\xwpadmin.obj \
$(XWP_OUTPUT_ROOT)\xwpfsys.obj \
$(XWP_OUTPUT_ROOT)\xwpkeybd.obj \
$(XWP_OUTPUT_ROOT)\xwpmedia.obj \
$(XWP_OUTPUT_ROOT)\xwpmouse.obj \
$(XWP_OUTPUT_ROOT)\xwppgm.obj \
$(XWP_OUTPUT_ROOT)\xwpsetup.obj \
$(XWP_OUTPUT_ROOT)\xwpscreen.obj \
$(XWP_OUTPUT_ROOT)\xwpsound.obj \
$(XWP_OUTPUT_ROOT)\xwpstring.obj \
# code from shared \
$(XWP_OUTPUT_ROOT)\center.obj \
$(XWP_OUTPUT_ROOT)\classes.obj \
$(XWP_OUTPUT_ROOT)\cnrsort.obj \
$(XWP_OUTPUT_ROOT)\common.obj \
$(XWP_OUTPUT_ROOT)\contentmenus.obj \
$(XWP_OUTPUT_ROOT)\notebook.obj \
$(XWP_OUTPUT_ROOT)\kernel.obj \
$(XWP_OUTPUT_ROOT)\xsetup.obj \
$(XWP_OUTPUT_ROOT)\wpsh.obj \
# code from config\
$(XWP_OUTPUT_ROOT)\cfgsys.obj \
$(XWP_OUTPUT_ROOT)\classlst.obj \
$(XWP_OUTPUT_ROOT)\drivdlgs.obj \
$(XWP_OUTPUT_ROOT)\drivers.obj \
$(XWP_OUTPUT_ROOT)\fonts.obj \
$(XWP_OUTPUT_ROOT)\hookintf.obj \
$(XWP_OUTPUT_ROOT)\pagemage.obj \
#$(XWP_OUTPUT_ROOT)\partitions.obj \
$(XWP_OUTPUT_ROOT)\sound.obj \
# code from filesys\
$(XWP_OUTPUT_ROOT)\desktop.obj \
$(XWP_OUTPUT_ROOT)\disk.obj \
$(XWP_OUTPUT_ROOT)\fdrhotky.obj \
$(XWP_OUTPUT_ROOT)\fdrnotebooks.obj \
$(XWP_OUTPUT_ROOT)\fdrsort.obj \
$(XWP_OUTPUT_ROOT)\fdrsubclass.obj \
$(XWP_OUTPUT_ROOT)\fdrmenus.obj \
$(XWP_OUTPUT_ROOT)\fhandles.obj \
$(XWP_OUTPUT_ROOT)\filedlg.obj \
$(XWP_OUTPUT_ROOT)\fileops.obj \
$(XWP_OUTPUT_ROOT)\filesys.obj \
$(XWP_OUTPUT_ROOT)\filetype.obj \
$(XWP_OUTPUT_ROOT)\folder.obj \
$(XWP_OUTPUT_ROOT)\fops_bottom.obj \
$(XWP_OUTPUT_ROOT)\fops_top.obj \
$(XWP_OUTPUT_ROOT)\object.obj \
$(XWP_OUTPUT_ROOT)\program.obj \
$(XWP_OUTPUT_ROOT)\refresh.obj \
$(XWP_OUTPUT_ROOT)\statbars.obj \
$(XWP_OUTPUT_ROOT)\trash.obj \
$(XWP_OUTPUT_ROOT)\xthreads.obj \
# code from media\
$(XWP_OUTPUT_ROOT)\mmcdplay.obj \
$(XWP_OUTPUT_ROOT)\mmhelp.obj \
$(XWP_OUTPUT_ROOT)\mmthread.obj \
$(XWP_OUTPUT_ROOT)\mmvolume.obj \
# code from startshut\
$(XWP_OUTPUT_ROOT)\apm.obj \
$(XWP_OUTPUT_ROOT)\archives.obj \
$(XWP_OUTPUT_ROOT)\shutdown.obj \
$(XWP_OUTPUT_ROOT)\winlist.obj \
# code from xcenter\
$(XWP_OUTPUT_ROOT)\ctr_engine.obj \
$(XWP_OUTPUT_ROOT)\ctr_notebook.obj \
$(XWP_OUTPUT_ROOT)\w_objbutton.obj \
$(XWP_OUTPUT_ROOT)\w_pulse.obj

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

!ifdef XWP_DEBUG
PMPRINTF_LIB = $(HELPERS_BASE)\src\helpers\pmprintf.lib
!else
PMPRINTF_LIB =
!endif

# The following macros contains the .OBJ files for the XCenter plugins.
DISKFREEOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_diskfree.obj $(PMPRINTF_LIB)
WINLISTOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_winlist.obj $(PMPRINTF_LIB)
MONITOROBJS = $(XWP_OUTPUT_ROOT)\widgets\w_monitors.obj $(PMPRINTF_LIB)
SENTINELOBJS = $(XWP_OUTPUT_ROOT)\widgets\w_sentinel.obj $(PMPRINTF_LIB) libs\win32k.lib
HEALTHOBJS = $(XWP_OUTPUT_ROOT)\widgets\xwHealth.obj $(PMPRINTF_LIB)
SAMPLEOBJS = $(XWP_OUTPUT_ROOT)\widgets\____sample.obj $(PMPRINTF_LIB)

# The PGMGDMNOBJS macro contains all the PageMage .OBJ files,
# which go into XWPDAEMN.EXE.
PGMGDMNOBJS = \
$(XWP_OUTPUT_ROOT)\exe_mt\pgmg_control.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\pgmg_move.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\pgmg_settings.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\pgmg_winscan.obj

# The DMNOBJS macro contains all the .OBJ files for XWPDAEMN.EXE;
# this includes PGMGDMNOBJS.
DMNOBJS = \
$(XWP_OUTPUT_ROOT)\exe_mt\xwpdaemn.obj \
$(PGMGDMNOBJS) \
$(XWP_OUTPUT_ROOT)\exe_mt\debug.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\gpih.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\linklist.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\except.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\dosh.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\memdebug.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\shapewin.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\stringh.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\threads.obj \
$(XWP_OUTPUT_ROOT)\exe_mt\xstring.obj \
$(XWP_OUTPUT_ROOT)\xwphook.lib

# objects for XDEBUG.DLL (debugging only)
DEBUG_OBJS = $(XWP_OUTPUT_ROOT)\xdebug.obj $(XWP_OUTPUT_ROOT)\xdebug_folder.obj

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize in inference
# rules.

.SUFFIXES: .obj .dll .exe .h .rc .res

# The LIBS macro contains all the .LIB files, either from the compiler or
# others, which are needed for this project:
#   somtk       is the SOM toolkit lib
#   pmprintf    is for debugging
# The other OS/2 libraries are used by default.
LIBS = somtk.lib $(PMPRINTF_LIB)

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
all:            idl helpers helpers_exe_mt compile_all link
    @echo ----- Leaving $(MAKEDIR)
    @echo Yo, done!

# "really_all" references "all" and compiles really everything.
# This must be used for the release version.
really_all:     idl helpers helpers_exe_mt compile_really_all link nls
    @echo ----- Leaving $(MAKEDIR)
    @echo Yo, done!

# "dep": create dependencies.
dep:
    @echo $(MAKEDIR)\makefile: Going for subdir src (dep)
    @cd src
    $(MAKE) -nologo dep "SUBTARGET=dep" "RUNDEPONLY=1" "REALLYALL=1"
    @cd $(CURRENT_DIR)

# "COMPILE" PSEUDOTARGETS
# -----------------------

# The following are invoked via "all" or "really_all"
# to call src\makefile with SUBTARGET=all, which will
# in turn invoke nmake all to the src\ subdirectories.

# idl always gets invoked first to check whether
# src\classes\*.c needs to be refreshed from idl\*.idl.
idl:
    @echo $(MAKEDIR)\makefile: Going for subdir idl
    @cd idl
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..

helpers:
# helpers:
# this branches over to the xwphelpers source tree,
# which is prepared for this. The helpers.lib file
# is created in $(XWP_OUTPUT_ROOT) then.
    @echo $(MAKEDIR)\makefile: Going for src\helpers (DLL version)
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
    @echo $(MAKEDIR)\makefile: Going for src\helpers (EXE MT version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(XWP_OUTPUT_ROOT)\exe_mt" "CC_HELPERS=$(CC_HELPERS_EXE_MT)"
    @cd $(CURRENT_DIR)

# compile_all: compile main (without treesize etc.)
compile_all:
    @echo $(MAKEDIR)\makefile: Going for subdir src
    @cd src
    $(MAKE) -nologo "SUBTARGET=all"
    @cd $(CURRENT_DIR)

# compile_really_all: compile really_all
compile_really_all:
    @echo $(MAKEDIR)\makefile: Going for subdir src (REALLY_ALL)
    @cd src
    $(MAKE) -nologo "SUBTARGET=all" "REALLYALL=1"
    @cd $(CURRENT_DIR)

tools:
    @echo $(MAKEDIR)\makefile: Going for subdir tools
    @cd tools
    $(MAKE) -nologo "SUBTARGET=all" "MAINMAKERUNNING=YES"
    @cd ..

xwpsecurity:
    @echo $(MAKEDIR)\makefile: Going for subdir src\xwpsec_ring0
    @cd src\xwpsec_ring0
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..
    @echo $(MAKEDIR)\makefile: Going for subdir src\XWPShell
    @cd src\XWPShell
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

nls:
    @echo $(MAKEDIR)\makefile: Going for subdir $(XWP_LANG_CODE)\dll
    @cd $(XWP_LANG_CODE)\dll
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile: Going for subdir $(XWP_LANG_CODE)\inf.$(XWP_LANG_CODE)
    @cd inf.$(XWP_LANG_CODE)
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile: Going for subdir $(XWP_LANG_CODE)\misc
    @cd misc
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..
    @echo $(MAKEDIR)\makefile: Going for subdir $(XWP_LANG_CODE)\xwphelp
    @cd xwphelp
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
      $(XWPRUNNING)\bin\xwphook.dll \
      $(XWPRUNNING)\bin\xwpdaemn.exe
#      $(XWPRUNNING)\bin\xwpfonts.fon
#      $(XWPRUNNING)\bin\xdebug.dll

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
        @echo $(MAKEDIR)\makefile: Creating TRACE files for $(@B).dll
        maptsf $(@B).map /MAJOR=255 /LOGSTACK=32 /LOGRETURN > $(@B).tsf
        trcust $(@B).tsf /I /L=bin\$(@B).dll /node /M=$(@B).map
        @echo $(MAKEDIR)\makefile: Done creating TRACE files for $(@B).dll
        cmd.exe /c copy $(@B).tdf $(XWPRUNNING)\bin
        cmd.exe /c del $(@B).tdf
        cmd.exe /c copy TRC00FF.TFF $(DYNAMIC_TRACE):\OS2\SYSTEM\TRACE
        cmd.exe /c del TRC00FF.TFF
!endif

# update DEF file if buildlevel has changed
src\shared\xwp.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XWorkplace main WPS classes module"

$(MODULESDIR)\xfldr.dll: $(OBJS) $(HLPOBJS) $(ANIOBJS) src\shared\xwp.def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) @<<
/OUT:$@ src\shared\xwp.def $(OBJS) $(HLPOBJS) $(ANIOBJS) $(LIBS)
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
src\shared\xwpres.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XWorkplace resources module"

$(MODULESDIR)\xwpres.dll: $(XWP_OUTPUT_ROOT)\dummyfont.obj src\shared\xwpres.def $(XWP_OUTPUT_ROOT)\xwpres.res
        @echo $(MAKEDIR)\makefile: Linking $@
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

#
# Linking WINLIST.DLL
#
$(XWPRUNNING)\plugins\xcenter\winlist.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        $(RUN_UNLOCK) $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter

# update DEF file if buildlevel has changed
src\widgets\winlist.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter window-list plugin DLL"

$(MODULESDIR)\winlist.dll: $(WINLISTOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
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
src\widgets\w_diskfree.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter diskfree plugin DLL"

$(MODULESDIR)\diskfree.dll: $(DISKFREEOBJS) src\widgets\w_diskfree.def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\w_diskfree.def @<<link.tmp
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
src\widgets\monitors.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter monitors plugin DLL"

$(MODULESDIR)\monitors.dll: $(MONITOROBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<link.tmp
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
src\widgets\sentinel.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter Theseus 4 plugin DLL"

$(MODULESDIR)\sentinel.dll: $(SENTINELOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
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
src\widgets\xwHealth.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter xwHealth plugin DLL"

$(MODULESDIR)\xwHealth.dll: $(HEALTHOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<link.tmp
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
src\widgets\sample.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XCenter sample plugin DLL"

$(MODULESDIR)\sample.dll: $(SAMPLEOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<link.tmp
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

#
# Linking XWPDAEMN.EXE
#
$(XWPRUNNING)\bin\xwpdaemn.exe: $(MODULESDIR)\$(@B).exe
        cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
src\Daemon\xwpdaemn.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XWorkplace PM daemon"

# create import library from XWPHOOK.DLL
$(XWP_OUTPUT_ROOT)\xwphook.lib: $(MODULESDIR)\$(@B).dll src\hook\$(@B).def
        implib /nologo $(XWP_OUTPUT_ROOT)\$(@B).lib $(MODULESDIR)\$(@B).dll

$(MODULESDIR)\xwpdaemn.exe: src\Daemon\$(@B).def $(DMNOBJS) $(XWP_OUTPUT_ROOT)\exe_mt\$(@B).res
        @echo $(MAKEDIR)\makefile: Linking $(MODULESDIR)\$(@B).exe
        $(LINK) @<<
/OUT:$(MODULESDIR)\$(@B).exe src\Daemon\$(@B).def $(DMNOBJS) $(PMPRINTF_LIB)
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        $(RC) ..\exe_mt\$(@B).res $(@B).exe
        mapsym /n $(@B).map > NUL
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(CURRENT_DIR)

#
# Linking XWPHOOK.DLL
#
$(XWPRUNNING)\bin\xwphook.dll: $(MODULESDIR)\$(@B).dll
# no unlock, this is a hook        unlock $@
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!ifdef DYNAMIC_TRACE
        @echo $(MAKEDIR)\makefile: Creating TRACE files for $(@B).dll
        maptsf $(@B).map /MAJOR=253 /LOGSTACK=32 /LOGRETURN > $(@B).tsf
        trcust $(@B).tsf /I /L=bin\$(@B).dll /node /M=$(@B).map
        @echo $(MAKEDIR)\makefile: Done creating TRACE files for $(@B).dll
        cmd.exe /c copy $(@B).tdf $(XWPRUNNING)\bin
        cmd.exe /c del $(@B).tdf
        cmd.exe /c copy TRC00FD.TFF $(DYNAMIC_TRACE):\OS2\SYSTEM\TRACE
        cmd.exe /c del TRC00FD.TFF
!endif

# update DEF file if buildlevel has changed
src\hook\xwphook.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XWorkplace PM hook module"

$(MODULESDIR)\xwphook.dll: src\hook\$(@B).def $(XWP_OUTPUT_ROOT)\$(@B).obj
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\hook\$(@B).def $(XWP_OUTPUT_ROOT)\$(@B).obj $(PMPRINTF_LIB)
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
src\shared\xwpfonts.def: include\bldlevel.h
        $(RUN_BLDLEVEL) $@ include\bldlevel.h "XWorkplace bitmap fonts"

$(MODULESDIR)\xwpfonts.fon: $(XWP_OUTPUT_ROOT)\dummyfont.obj $(XWP_OUTPUT_ROOT)\$(@B).res
        @echo $(MAKEDIR)\makefile: Linking $(MODULESDIR)\$(@B).fon
        $(LINK) /OUT:$(MODULESDIR)\$(@B).dll src\shared\$(@B).def $(XWP_OUTPUT_ROOT)\dummyfont.obj
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
    @echo $(MAKEDIR)\makefile: Calling DLGEDIT.EXE
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
    @echo $(MAKEDIR)\makefile: Calling DLGEDIT.EXE
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
# create directories
!if [@md $(XWPRELEASE) 2> NUL]
!endif
!if [@md $(XWPRELEASE_HEALTH) 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN) 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS) 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLSDOC) 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAP) 2> NUL]
!endif
    @echo $(MAKEDIR)\makefile: Now copying files to $(XWPRELEASE).
    $(COPY) release\* $(XWPRELEASE_MAIN)
    $(COPY) $(XWP_LANG_CODE)\readme $(XWPRELEASE_NLSDOC)
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).inf $(XWPRELEASE_NLSDOC)
    $(COPY) BUGS $(XWPRELEASE_NLSDOC)
    $(COPY) FEATURES $(XWPRELEASE_NLSDOC)
    $(COPY) cvs.txt $(XWPRELEASE_MAIN)
#
# 2) bin
#    a) kernel
!if [@md $(XWPRELEASE_MAIN)\bin 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS)\bin 2> NUL]
!endif
    $(COPY) release\bin\* $(XWPRELEASE_MAIN)\bin
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
    $(COPY) $(MODULESDIR)\repclass.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\wpsreset.exe $(XWPRELEASE_MAIN)\bin
#    b) NLS
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).dll $(XWPRELEASE_NLS)\bin
#    $(COPY) $(XWP_LANG_CODE)\misc\*.sgs $(XWPRELEASE_NLS)\bin
#    b) mapfiles
    $(COPY) $(MODULESDIR)\*.map $(XWPRELEASE_MAP)
#
# 3) bootlogo
!if [@md $(XWPRELEASE_MAIN)\bootlogo 2> NUL]
!endif
    $(COPY) release\bootlogo\* $(XWPRELEASE_MAIN)\bootlogo
#
# 4) help
!if [@md $(XWPRELEASE_NLS)\help 2> NUL]
!endif
    $(COPY) $(XWP_LANG_CODE)\misc\xfldr$(XWP_LANG_CODE).tmf $(XWPRELEASE_NLS)\help
    $(COPY) $(XWP_LANG_CODE)\misc\drvrs$(XWP_LANG_CODE).txt $(XWPRELEASE_NLS)\help
    $(COPY) $(XWP_LANG_CODE)\misc\xfcls$(XWP_LANG_CODE).txt $(XWPRELEASE_NLS)\help
    $(COPY) $(MODULESDIR)\xfldr$(XWP_LANG_CODE).hlp $(XWPRELEASE_NLS)\help
# 5) icons
!if [@md $(XWPRELEASE_MAIN)\icons 2> NUL]
!endif
    $(COPY) release\icons\* $(XWPRELEASE_MAIN)\icons
# 6) install
!if [@md $(XWPRELEASE_MAIN)\install 2> NUL]
!endif
!if [@md $(XWPRELEASE_NLS)\install 2> NUL]
!endif
    $(COPY) release\install\* $(XWPRELEASE_MAIN)\install
    $(COPY) $(XWP_LANG_CODE)\misc\*.cmd $(XWPRELEASE_NLS)\install
    $(COPY) $(XWP_LANG_CODE)\misc\*.msg $(XWPRELEASE_NLS)\install
# 7) wav
!if [@md $(XWPRELEASE_MAIN)\wav 2> NUL]
!endif
    $(COPY) release\wav\* $(XWPRELEASE_MAIN)\wav
    @echo $(MAKEDIR)\makefile: Done copying files.
# 8) plugins
!if [@md $(XWPRELEASE_MAIN)\plugins 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\plugins\xcenter 2> NUL]
!endif
!if [@md $(XWPRELEASE_HEALTH)\plugins 2> NUL]
!endif
!if [@md $(XWPRELEASE_HEALTH)\plugins\xcenter 2> NUL]
!endif
    $(COPY) $(MODULESDIR)\diskfree.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\diskfree.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\monitors.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\monitors.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\sentinel.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\sentinel.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\xwHealth.dll $(XWPRELEASE_HEALTH)\plugins\xcenter
    $(COPY) $(MODULESDIR)\xwHealth.sym $(XWPRELEASE_HEALTH)\plugins\xcenter
    @echo $(MAKEDIR)\makefile: Done copying files.
# 9) toolkit
!if [@md $(XWPRELEASE_MAIN)\toolkit 2> NUL]
!endif
!if [@md $(XWPRELEASE_MAIN)\toolkit\shared 2> NUL]
!endif
    $(COPY) src\widgets\miniwdgt.c $(XWPRELEASE_MAIN)\toolkit
    $(COPY) include\shared\center.h $(XWPRELEASE_MAIN)\toolkit\shared

#
# Special target "warpin": this is not called by "all",
# but must be set on the NMAKE command line.
# Will work only on my private system.
#

warpin: release
    @echo $(MAKEDIR)\makefile: Building WPI from $(XWPRELEASE).
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
    @echo $(MAKEDIR)\makefile: Transferring WPI from $(XWPRELEASE).
    cd $(XWPRELEASE)
    cd ..
    sendfile p75 xwp-temp.wpi C:\_wpi\xwp-transferred.wpi
    cd $(CURRENT_DIR)

