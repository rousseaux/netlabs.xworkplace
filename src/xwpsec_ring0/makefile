#
# $Header$
#

# 32 bits OS/2 device driver and IFS support. Provides 32 bits kernel
# services (DevHelp) and utility functions to 32 bits OS/2 ring 0 code
# (device drivers and installable file system drivers).
# Copyright (C) 1995, 1996, 1997  Matthieu WILLM (willm@ibm.net)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#
# Path definitions
#

!include ..\..\setup.in

CC        = icc -q
CFLAGS    = -Ge- -Gs- -Gr+ -Rn -Ss+ -O -G5

LD        = icc

AS        = alp
ASFLAGS   = -Mb -Li

!ifndef DDKPATH
!error DDKPATH is not defined.
!endif

!ifndef SES_MAIN_DIR
!error SES_MAIN_DIR is not defined.
!endif

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
MAIN_OUTPUTDIR = ..\..\bin
OUTPUTDIR=$(MAIN_OUTPUTDIR)\xwpsec32.sys
MODULESDIR=$(MAIN_OUTPUTDIR)\modules

# create output directory
!if [@md $(MAIN_OUTPUTDIR) 2> NUL]
!endif
!if [@md $(OUTPUTDIR) 2> NUL]
!endif
!if [@md $(MODULESDIR) 2> NUL]
!endif

MYDIR = $(MAKEDIR)

INC = ..\..\include

MYINC = $(INC)\xwpsec32.sys

CDEFINES   =
# -DDYNAMIC_DEVHELP -DMWDD32_VER=$(MWDD32_VERSION)
ADEFINES   =
#-D:DYNAMIC_DEVHELP
CINCLUDES  = -I$(INCLUDE);$(PROJECT_INCLUDE);$(DDKPATH)\h;$(SES_MAIN_DIR)\h;

DRIVERNAME=xwpsec32

#    -I$(DDKPATH)\h \
#    -I$(DDKPATH)/src/dev/dasd/diskh \
#    -I/os2/security/dev/h \
#    -I/os2/security/dev/ssskpi \
#    -I. -I../../include -I../../include/os2

AINCLUDES  = -Fdi:$(DDKPATH)/inc -Fdi:../../include/xwpsec32.sys

.c{$(OUTPUTDIR)}.obj:
        @echo $<
        @echo CINCLUDES: $(CINCLUDES)
        @$(CC) -c $(CFLAGS) $(CDEFINES) -Fa$(@:.obj=.s) -Fo$@ $(CINCLUDES) $<

.asm{$(OUTPUTDIR)}.obj:
        @echo $<
        @$(AS) $(ASFLAGS) $(ADEFINES) $(AINCLUDES) $< -Fo:$@ -Fl:$(@:.obj=.lst)

all: $(MODULESDIR)\$(DRIVERNAME).sys

AOBJS = \
    $(OUTPUTDIR)\sec32_pre_init_base.obj $(OUTPUTDIR)\sec32_devhlp32.obj

COBJS =   \
    $(OUTPUTDIR)\audit.obj                                   \
    $(OUTPUTDIR)\callb_close.obj                               \
    $(OUTPUTDIR)\callb_delete.obj                              \
    $(OUTPUTDIR)\callb_dirs.obj                                \
    $(OUTPUTDIR)\callb_exec.obj                                \
    $(OUTPUTDIR)\callb_find.obj                                \
    $(OUTPUTDIR)\callb_move.obj                                \
    $(OUTPUTDIR)\callb_open.obj                                \
    $(OUTPUTDIR)\sec32_idc.obj                               \
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
    $(OUTPUTDIR)\util_log.obj                                \
    $(OUTPUTDIR)\util_misc.obj                               \
    $(OUTPUTDIR)\util_sem.obj

SHRHDR = $(MYINC)\devhlp32.h $(MYINC)\reqpkt32.h $(MYINC)\r0thunk.inc

$(OUTPUTDIR)\audit.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\callb_close.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_delete.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_dirs.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_exec.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_find.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_move.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\callb_open.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\sec32_data.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h

$(OUTPUTDIR)\sec32_devhlp32c.obj: $(@B).c $(SHRHDR)

$(OUTPUTDIR)\sec32_idc.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h

$(OUTPUTDIR)\sec32_strategy.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\sec32_callbacks.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_init_base.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_init_complete.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_ioctl.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\strat_open.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_close.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_invalid.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\strat_shutdown.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\util_daemon.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h \
        $(INC)\security\ring0api.h

$(OUTPUTDIR)\util_log.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\util_misc.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

$(OUTPUTDIR)\util_sem.obj: $(@B).c $(SHRHDR) \
        $(MYINC)\xwpsec_types.h $(MYINC)\xwpsec_callbacks.h

# linking

$(DRIVERNAME).def: ..\..\include\bldlevel.h makefile
    cmd.exe /c BuildLevel.cmd $(DRIVERNAME).def ..\..\include\bldlevel.h "XWorkplace Security device driver"

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
$(DDKPATH)\lib\os2386p.lib
$(DRIVERNAME).def
<<
        @cd $(MODULESDIR)
        mapsym $(DRIVERNAME).map
        @cd $(MYDIR)
        -copy $@             E:\os2
        -copy $(@:.sys=.sym) E:\os2
        -copy $@             F:\os2\boot
        -copy $(@:.sys=.sym) F:\os2\boot



#-Fe$@
