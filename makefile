
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
#                       --  "all" (default): build XFLDR.DLL,
#                           XWPHOOK.DLL, XWPDAEMN.EXE.
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

# PROJECT_BASE_DIR is used by "setup.in"
# subdirectories to identify the root of the source
# tree. This is passed to the sub-makefiles.
PROJECT_BASE_DIR = $(CVS_WORK_ROOT)\xworkplace

# MODULESDIR is used for mapfiles and final module (DLL, EXE) output.
MODULESDIR=bin\modules

# create output directory
!if [@md $(bin) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

# include setup (compiler options etc.)
!include setup.in

# VARIABLES
# ---------

# The OBJS macro contains all the .OBJ files which have been
# created from the files in MAIN\.
OBJS = \
# code from classes\
    bin\xcenter.obj bin\xfobj.obj bin\xfldr.obj bin\xfdesk.obj bin\xfsys.obj bin\xfwps.obj \
    bin\xfdisk.obj bin\xfdataf.obj bin\xfpgmf.obj bin\xfstart.obj bin\xmmvolume.obj \
    bin\xclslist.obj bin\xwpsound.obj bin\xtrash.obj bin\xtrashobj.obj bin\xwpfsys.obj \
    bin\xwpkeybd.obj bin\xwpmedia.obj bin\xwpmouse.obj bin\xwpsetup.obj bin\xwpscreen.obj \
    bin\xwpstring.obj \
# code from shared \
    bin\center.obj bin\classes.obj bin\cnrsort.obj bin\common.obj bin\contentmenus.obj \
    bin\notebook.obj bin\kernel.obj bin\xsetup.obj bin\wpsh.obj \
# code from config\
    bin\cfgsys.obj bin\classlst.obj bin\drivdlgs.obj bin\drivers.obj bin\hookintf.obj \
    bin\pagemage.obj bin\partitions.obj bin\sound.obj \
# code from filesys\
    bin\disk.obj bin\fdrhotky.obj bin\fdrnotebooks.obj bin\fdrsubclass.obj bin\fdrmenus.obj \
    bin\fhandles.obj bin\fileops.obj bin\filesys.obj bin\fops_bottom.obj bin\fops_top.obj \
    bin\filetype.obj \
    bin\folder.obj bin\object.obj bin\desktop.obj \
    bin\program.obj bin\statbars.obj bin\trash.obj bin\xthreads.obj \
# code from media\
    bin\mmhelp.obj bin\mmthread.obj bin\mmvolume.obj \
# code from startshut\
    bin\apm.obj bin\archives.obj bin\shutdown.obj bin\winlist.obj \
# code from xcenter\
    bin\ctr_engine.obj bin\ctr_notebook.obj bin\w_objbutton.obj bin\w_pulse.obj

OBJS_ANICLASSES = bin\anand.obj bin\anos2ptr.obj bin\anwani.obj bin\anwcur.obj
OBJS_ANICONVERT = bin\cursor.obj bin\pointer.obj bin\script.obj
#bin\expire.obj
OBJS_ANIDLL = bin\dll.obj bin\dllbin.obj
OBJS_ANIANI = bin\mptranim.obj bin\mptrcnr.obj bin\mptredit.obj bin\mptrlset.obj \
    bin\mptrpag1.obj bin\mptrppl.obj bin\mptrprop.obj bin\mptrptr.obj bin\mptrset.obj \
    bin\mptrutil.obj bin\mptrfile.obj bin\wpamptr.obj

!ifdef ANIMATED_MOUSE_POINTERS
ANIOBJS = $(OBJS_ANICLASSES) $(OBJS_ANICONVERT) $(OBJS_ANIANI) $(OBJS_ANIDLL)
!else
ANIOBJS =
!endif

# The HLPOBJS macro contains all the .OBJ files which have been
# created from the files in HELPERS\. You probably won't have to change this.
HLPOBJS = bin\helpers.lib

# The following macros contains the .OBJ files for the XCenter plugins.
WINLISTOBJS = bin\widgets\w_winlist.obj
MONITOROBJS = bin\widgets\w_monitors.obj

!ifdef PAGEMAGE
PGMGDMNOBJS = bin\exe_mt\pgmg_control.obj bin\exe_mt\pgmg_move.obj bin\exe_mt\pgmg_settings.obj \
    bin\exe_mt\pgmg_winscan.obj
!else
PGMGDMNOBJS =
!endif

# The DMNOBJS macro contains all the .OBJ files for XWPDAEMN.EXE.
DMNOBJS = \
bin\exe_mt\xwpdaemn.obj \
$(PGMGDMNOBJS) \
bin\exe_mt\debug.obj bin\exe_mt\except.obj bin\exe_mt\dosh.obj bin\exe_mt\memdebug.obj \
bin\exe_mt\stringh.obj bin\exe_mt\threads.obj bin\exe_mt\xstring.obj \
bin\xwphook.lib

# objects for XDEBUG.DLL (debugging only)
DEBUG_OBJS = bin\xdebug.obj bin\xdebug_folder.obj

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
PMPRINTF_LIB = $(HELPERS_BASE)\src\helpers\pmprintf.lib
LIBS = somtk.lib $(PMPRINTF_LIB)

# some variable strings to pass to sub-nmakes
SUBMAKE_PASS_STRING = "PROJECT_BASE_DIR=$(PROJECT_BASE_DIR)" "PROJECT_INCLUDE=$(PROJECT_INCLUDE)"

# store current directory so we can change back later
CURRENT_DIR = $(MAKEDIR)


# PSEUDOTARGETS
# -------------

all: idl cpl_main link
    @echo ----- Leaving $(MAKEDIR)

# "really_all" references "all".
really_all: all treesize netscdde xshutdwn tools nls
    @echo ----- Leaving $(MAKEDIR)

# If you add a subdirectory to SRC\, add a target to
# "cpl_main" also to have automatic recompiles.
cpl_main: helpers helpers_exe_mt classes config filesys media widgets xcenter \
!ifdef ANIMATED_MOUSE_POINTERS
pointers \
!endif
!ifdef XWPSECURITY
xwpsecurity \
!endif
shared startshut hook
#animouse

# COMPILER PSEUDOTARGETS
# ----------------------

tools:
    @echo $(MAKEDIR)\makefile: Going for subdir tools
    @cd tools
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..

idl:
    @echo $(MAKEDIR)\makefile: Going for subdir idl
    @cd idl
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..

classes:
    @echo $(MAKEDIR)\makefile: Going for subdir src\classes
    @cd src\classes
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

config:
    @echo $(MAKEDIR)\makefile: Going for subdir src\config
    @cd src\config
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

filesys:
    @echo $(MAKEDIR)\makefile: Going for subdir src\filesys
    @cd src\filesys
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

media:
# V0.9.3 (2000-04-25) [umoeller]
    @echo $(MAKEDIR)\makefile: Going for subdir src\media
    @cd src\media
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

widgets:
# V0.9.7 (2000-12-02) [umoeller]
    @echo $(MAKEDIR)\makefile: Going for subdir src\widgets
    @cd src\widgets
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

xcenter:
# V0.9.7 (2000-12-02) [umoeller]
    @echo $(MAKEDIR)\makefile: Going for subdir src\xcenter
    @cd src\xcenter
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

helpers:
# helpers:
# this branches over to the WarpIN source tree,
# which is prepared for this. The helpers.lib file
# is created in the .\bin directory and can be used
# with both EXE's and DLL's (VAC++ user guide says).
    @echo $(MAKEDIR)\makefile: Going for src\helpers (DLL version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(PROJECT_BASE_DIR)\bin" "CC_HELPERS=$(CC_HELPERS_DLL)"
# according to VAC++ user guide, we need to use /ge+ for libs
# even if the lib will be linked to a DLL
    @cd $(CURRENT_DIR)

helpers_exe_mt:
# helpers_exe_mt:
# same as the above, but this builds a multithread lib for EXEs.
    @echo $(MAKEDIR)\makefile: Going for WarpIN subdir src\helpers (EXE MT version)
    @cd $(HELPERS_BASE)\src\helpers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING) \
"HELPERS_OUTPUT_DIR=$(PROJECT_BASE_DIR)\bin\exe_mt" "CC_HELPERS=$(CC_HELPERS_EXE_MT)"
    @cd $(CURRENT_DIR)

shared:
    @echo $(MAKEDIR)\makefile: Going for subdir src\shared
    @cd src\shared
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

startshut:
    @echo $(MAKEDIR)\makefile: Going for subdir src\startshut
    @cd src\startshut
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

hook:
    @echo $(MAKEDIR)\makefile: Going for subdir src\Daemon
    @cd src\Daemon
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @echo $(MAKEDIR)\makefile: Going for subdir src\hook
    @cd ..\hook
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

treesize:
    @echo $(MAKEDIR)\makefile: Going for subdir src\treesize
    @cd src\treesize
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

netscdde:
    @echo $(MAKEDIR)\makefile: Going for subdir src\NetscapeDDE
    @cd src\NetscapeDDE
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

pointers:
    @echo $(MAKEDIR)\makefile: Going for subdir src\pointers
    @cd src\pointers
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

xshutdwn:
    @echo $(MAKEDIR)\makefile: Going for subdir src\xshutdwn
    @cd src\xshutdwn
    @nmake -nologo all "MAINMAKERUNNING=YES" $(SUBMAKE_PASS_STRING)
    @cd ..\..

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
      $(XWPRUNNING)\plugins\xcenter\monitors.dll \
      $(XWPRUNNING)\plugins\xcenter\winlist.dll \
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
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin
!ifndef DEBUG
# copy symbol file, which is only needed if debug code is disabled
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!endif
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
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XWorkplace main WPS classes module"

$(MODULESDIR)\xfldr.dll: $(OBJS) $(HLPOBJS) $(ANIOBJS) src\shared\xwp.def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\shared\xwp.def @<<link.tmp
$(OBJS) $(HLPOBJS) $(ANIOBJS) $(LIBS)
<<
        @cd $(MODULESDIR)
!ifndef DEBUG
# create symbol file, which is only needed if debug code is disabled
        mapsym /n $(@B).map > NUL
!endif
        @cd $(CURRENT_DIR)
        cmd.exe /c tools\raisebld.cmd include\build.h

#
# Linking XWPRES.DLL
#

$(XWPRUNNING)\bin\xwpres.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
src\shared\xwpres.def: include\bldlevel.h
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XWorkplace resources module"

$(MODULESDIR)\xwpres.dll: bin\dummyfont.obj src\shared\xwpres.def bin\xwpres.res
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK_ALWAYSPACK) /OUT:$@ src\shared\xwpres.def bin\dummyfont.obj
        @cd $(MODULESDIR)
        $(RC) ..\xwpres.res $(@B).dll
        @cd $(CURRENT_DIR)

#
# Linking WINLIST.DLL
#
$(XWPRUNNING)\plugins\xcenter\winlist.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
!ifndef DEBUG
# copy symbol file, which is only needed if debug code is disabled
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter
!endif

# update DEF file if buildlevel has changed
src\widgets\winlist.def: include\bldlevel.h
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XCenter window-list plugin DLL"

$(MODULESDIR)\winlist.dll: $(WINLISTOBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<link.tmp
$(WINLISTOBJS)
<<
        @cd $(MODULESDIR)
!ifndef DEBUG
# create symbol file, which is only needed if debug code is disabled
        mapsym /n $(@B).map > NUL
!endif
        @cd $(CURRENT_DIR)

#
# Linking MONITORS.DLL
#
$(XWPRUNNING)\plugins\xcenter\monitors.dll: $(MODULESDIR)\$(@B).dll
!ifdef XWP_UNLOCK_MODULES
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\plugins\xcenter
!ifndef DEBUG
# copy symbol file, which is only needed if debug code is disabled
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\plugins\xcenter
!endif

# update DEF file if buildlevel has changed
src\widgets\monitors.def: include\bldlevel.h
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XCenter monitors plugin DLL"

$(MODULESDIR)\monitors.dll: $(MONITOROBJS) src\widgets\$(@B).def
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\widgets\$(@B).def @<<link.tmp
$(MONITOROBJS)
<<
        @cd $(MODULESDIR)
!ifndef DEBUG
# create symbol file, which is only needed if debug code is disabled
        mapsym /n $(@B).map > NUL
!endif
        @cd $(CURRENT_DIR)


#
# Linking XWPDAEMN.EXE
#
$(XWPRUNNING)\bin\xwpdaemn.exe: $(MODULESDIR)\$(@B).exe
        cmd.exe /c copy $(MODULESDIR)\$(@B).exe $(XWPRUNNING)\bin
!ifndef DEBUG
# copy symbol file, which is only needed if debug code is disabled
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!endif

# update DEF file if buildlevel has changed
src\Daemon\xwpdaemn.def: include\bldlevel.h
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XWorkplace PM daemon"

# create import library from XWPHOOK.DLL
bin\xwphook.lib: $(MODULESDIR)\$(@B).dll src\hook\$(@B).def
        implib /nologo bin\$(@B).lib $(MODULESDIR)\$(@B).dll

$(MODULESDIR)\xwpdaemn.exe: src\Daemon\$(@B).def $(DMNOBJS) bin\exe_mt\$(@B).res
        @echo $(MAKEDIR)\makefile: Linking $(MODULESDIR)\$(@B).exe
        $(LINK) /OUT:$(MODULESDIR)\$(@B).exe src\Daemon\$(@B).def $(DMNOBJS) $(PMPRINTF_LIB)
        @cd $(MODULESDIR)
        $(RC) ..\exe_mt\$(@B).res $(@B).exe
!ifndef DEBUG
# create symbol file, which is only needed if debug code is disabled
        mapsym /n $(@B).map > NUL
!endif
        @cd $(CURRENT_DIR)

#
# Linking XWPHOOK.DLL
#
$(XWPRUNNING)\bin\xwphook.dll: $(MODULESDIR)\$(@B).dll
# no unlock, this is a hook        unlock $@
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin
!ifndef DEBUG
# copy symbol file, which is only needed if debug code is disabled
        cmd.exe /c copy $(MODULESDIR)\$(@B).sym $(XWPRUNNING)\bin
!endif
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
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XWorkplace PM hook module"

$(MODULESDIR)\xwphook.dll: src\hook\$(@B).def bin\$(@B).obj
        @echo $(MAKEDIR)\makefile: Linking $@
        $(LINK) /OUT:$@ src\hook\$(@B).def bin\$(@B).obj $(PMPRINTF_LIB)
        @cd $(MODULESDIR)
!ifndef DEBUG
# create symbol file, which is only needed if debug code is disabled
        mapsym /n $(@B).map > NUL
!endif
        @cd $(CURRENT_DIR)

#
# Linking XWPFONTS.FON
#
$(XWPRUNNING)\bin\xwpfonts.fon: $(MODULESDIR)\$(@B).fon
!ifdef XWP_UNLOCK_MODULES
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).fon $(XWPRUNNING)\bin

# update DEF file if buildlevel has changed
src\shared\xwpfonts.def: include\bldlevel.h
        cmd.exe /c BuildLevel.cmd $@ include\bldlevel.h "XWorkplace bitmap fonts"

$(MODULESDIR)\xwpfonts.fon: bin\dummyfont.obj bin\$(@B).res makefile
        @echo $(MAKEDIR)\makefile: Linking $(MODULESDIR)\$(@B).fon
        $(LINK) /OUT:$(MODULESDIR)\$(@B).dll src\shared\$(@B).def bin\dummyfont.obj
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
        unlock $@
!endif
        cmd.exe /c copy $(MODULESDIR)\$(@B).dll $(XWPRUNNING)\bin

$(MODULESDIR)\xdebug.dll: src\shared\$(@B).def $(DEBUG_OBJS) $(HLPOBJS) bin\wpsh.obj
        $(LINK) /OUT:$(MODULESDIR)\$(@B).dll src\shared\$(@B).def $(DEBUG_OBJS) $(HLPOBJS) $(LIBS) bin\wpsh.obj

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
# copy RES file to frontend.res so dlgedit finds it
    @cmd.exe /c copy ..\..\bin\xfldr$(XWP_LANG_CODE).res
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
    $(COPY) $(XWP_LANG_CODE)\inf.$(XWP_LANG_CODE)\xfldr$(XWP_LANG_CODE).inf $(XWPRELEASE_NLSDOC)
    $(COPY) BUGS $(XWPRELEASE_NLSDOC)
    $(COPY) FEATURES $(XWPRELEASE_NLSDOC)
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
    $(COPY) $(MODULESDIR)\xwphook.dll $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xwphook.sym $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\netscdde.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\treesize.exe $(XWPRELEASE_MAIN)\bin
    $(COPY) $(MODULESDIR)\xshutdwn.exe $(XWPRELEASE_MAIN)\bin
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
    $(COPY) $(XWP_LANG_CODE)\xwphelp\xfldr$(XWP_LANG_CODE).hlp $(XWPRELEASE_NLS)\help
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
    $(COPY) $(MODULESDIR)\monitors.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\monitors.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.dll $(XWPRELEASE_MAIN)\plugins\xcenter
    $(COPY) $(MODULESDIR)\winlist.sym $(XWPRELEASE_MAIN)\plugins\xcenter
    @echo $(MAKEDIR)\makefile: Done copying files.


