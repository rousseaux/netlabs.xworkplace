#
# Makefile for xwpsec32.sys.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
#       Copyright (C) 1998-2001 Ulrich M”ller.
#       This file is part of the XWorkplace source package.
#       XWorkplace is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published
#       by the Free Software Foundation, in version 2 as it comes in the
#       "COPYING" file of the XWorkplace main distribution.
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
!include ..\..\setup.in

!ifndef DDKBASE
!error DDKBASE is not defined.
!endif

!ifndef SESBASE
!error SESBASE is not defined.
!endif

MYDIR = $(MAKEDIR)

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in. $(XWP_OUTPUT_ROOT) is set by
# setup.in to point to \bin from the XWorkplace sources root,
# but this can be modified by setting an external environment
# variable. This approach has the advantage that
# 1) all build files are in a common dir tree and the entire
#    tree can be removed completely;
# 2) the build tree can be on a different physical drive for
#    speed.
MAIN_OUTPUTDIR = $(XWP_OUTPUT_ROOT)
OUTPUTDIR=$(MAIN_OUTPUTDIR)\xwpsec32.sys
MODULESDIR=$(MAIN_OUTPUTDIR)\modules

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

# FIXED MACROS
# ------------

MYDIR = $(MAKEDIR)

INC = ..\..\include

MYINC = $(INC)\xwpsec32.sys

CC        = icc -q
CFLAGS    = -Ge- -Gs- -Gr+ -Rn -Ss+ -O -G5

LD        = icc

AS        = alp
ASFLAGS   = -Mb -Li

CDEFINES   =
# -DDYNAMIC_DEVHELP -DMWDD32_VER=$(MWDD32_VERSION)
ADEFINES   =
#-D:DYNAMIC_DEVHELP
CINCLUDES  = /I$(PROJECT_INCLUDE)

DRIVERNAME=xwpsec32

AINCLUDES  = -Fdi:$(DDKBASE)/inc -Fdi:../../include/xwpsec32.sys

AOBJS = \
    $(OUTPUTDIR)\sec32_pre_init_base.obj $(OUTPUTDIR)\sec32_devhlp32.obj

COBJS =   \
#    $(OUTPUTDIR)\audit.obj                                   \
    $(OUTPUTDIR)\callb_close.obj                               \
    $(OUTPUTDIR)\callb_delete.obj                              \
    $(OUTPUTDIR)\callb_dirs.obj                                \
    $(OUTPUTDIR)\callb_exec.obj                                \
    $(OUTPUTDIR)\callb_find.obj                                \
    $(OUTPUTDIR)\callb_move.obj                                \
    $(OUTPUTDIR)\callb_open.obj                                \
#    $(OUTPUTDIR)\sec32_idc.obj                               \
    $(OUTPUTDIR)\sec32_strategy.obj                          \
    $(OUTPUTDIR)\sec32_data.obj                              \
    $(OUTPUTDIR)\sec32_callbacks.obj                         \
    $(OUTPUTDIR)\strat_init_base.obj                         \
    $(OUTPUTDIR)\strat_init_complete.obj                     \
    $(OUTPUTDIR)\strat_ioctl.obj                             \
    $(OUTPUTDIR)\strat_open.obj                              \
    $(OUTPUTDIR)\strat_close.obj                             \
    $(OUTPUTDIR)\strat_invalid.obj                           \
    $(OUTPUTDIR)\strat_shutdown.obj                          \
    $(OUTPUTDIR)\util_daemon.obj                             \
#    $(OUTPUTDIR)\util_log.obj                                \
    $(OUTPUTDIR)\util_misc.obj                               \
    $(OUTPUTDIR)\util_sem.obj

# TARGETS
# -------

all: $(MODULESDIR)\$(DRIVERNAME).sys
    @echo ----- Leaving $(MAKEDIR)

# The "dep" target: run fastdep on the sources.
# "nmake dep" gets called from src\makefile if nmake dep
# is running on the main makefile.
dep:
    $(RUN_FASTDEP) *.c
    @echo ----- Leaving $(MAKEDIR)

.c{$(OUTPUTDIR)}.obj:
        @echo $<
        @echo CINCLUDES: $(CINCLUDES)
        @$(CC) -c $(CFLAGS) $(CDEFINES) -Fa$(@:.obj=.s) -Fo$@ $(CINCLUDES) $<

.asm{$(OUTPUTDIR)}.obj:
        @echo $<
        @$(AS) $(ASFLAGS) $(ADEFINES) $(AINCLUDES) $< -Fo:$@ -Fl:$(@:.obj=.lst)

# linking

$(DRIVERNAME).def: ..\..\include\bldlevel.h makefile
    $(RUN_BLDLEVEL) $(DRIVERNAME).def ..\..\include\bldlevel.h "$(XWPNAME) Security device driver"

$(MODULESDIR)\$(DRIVERNAME).sys: \
            $(AOBJS)             \
            $(COBJS)             \
            $(OUTPUTDIR)\sec32_start.obj     \
            $(OUTPUTDIR)\sec32_end.obj       \
            $(DRIVERNAME).def    \
            makefile
        ilink @<<
/OUT:$(MODULESDIR)\$(@B).sys
/map:$(MODULESDIR)\$(@B).map
/EXEPACK:2 /NOD /ALIGNMENT:4
$(OUTPUTDIR)\sec32_start.obj
$(AOBJS)
$(COBJS)
$(OUTPUTDIR)\sec32_end.obj
$(DDKBASE)\lib\os2386p.lib
$(DRIVERNAME).def
<<
!ifdef XWP_OUTPUT_ROOT_DRIVE
        @$(XWP_OUTPUT_ROOT_DRIVE)
!endif
        @cd $(MODULESDIR)
        mapsym $(DRIVERNAME).map
!ifdef CVS_WORK_ROOT_DRIVE
        @$(CVS_WORK_ROOT_DRIVE)
!endif
        @cd $(MYDIR)
        -copy $@             E:\os2
        -copy $(@:.sys=.sym) E:\os2
        -copy $@             F:\os2\boot
        -copy $(@:.sys=.sym) F:\os2\boot


# The .OBJ-from-sources dependencies are now automatically
# created by "nmake dep" into the .depend include file.
# V0.9.12 (2001-05-22) [umoeller]

!ifndef NOINCLUDEDEPEND
!include .depend
!endif



