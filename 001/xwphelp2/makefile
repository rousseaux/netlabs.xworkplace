#
# Makefile for XWorkplace National Language Support.
#
# For use with IBM NMAKE, which is part of IBM C-Set/2 and VisualAge C++.
# This file will probably not work with other MAKE utilities, such as
# GNU make or DMAKE.
#
# Copyright (C) 1997-2002 Ulrich M”ller.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, in version 2 as it comes in the COPYING
# file of the XWorkplace main distribution.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# Required changes for translating the NLS DLL: three lines below.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

# include setup (compiler options etc.)
!include ..\..\setup.in

# OUTPUTDIR specifies the directory where all the output .OBJ
# files will be created in.
OUTPUTDIR = $(XWP_OUTPUT_ROOT)\modules
!if [@echo       OUTPUTDIR is $(OUTPUTDIR)]
!endif

# create output directory
!if [@md $(OUTPUTDIR) 2> NUL]
!endif

#
# Now set up a few environment variables.
#

# This is the language code for the language that the NLS DLL
# will support. Change this to a different language code, and
# the makefile will support your langauge.
LANGUAGE = 001

FEATURES_STRING =
!ifdef XWPLITE
FEATURES_STRING = -i..\..\include\features_lite.h
!endif

# Change this to the full path of the HTML2IPF utility.
# THIS HAS CHANGED WITH V0.84.
# now using h2i V0.9.13 (2001-06-23) [umoeller]
HTML2IPF = $(RUN_H2I) -s -i$(PROJECT_BASE_DIR)\include\shared\helppanels.h -i..\$(ENTITYDEFS) $(FEATURES_STRING)

# Change this to the directory where the output file (HLP)
# should be copied to.
!ifndef XWPRUNNING
!error The environment variable XWPRUNNING is not defined. Terminating.
!endif
COPYTO = $(XWPRUNNING)\help

# DO NOT CHANGE THE FOLLOWING

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize.
.SUFFIXES: .html .ipf .hlp .inf .gif

#
# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
#
{img}.gif{img}.bmp:
    -gbmsize $< $@,1.1

# The BMPS macro contains all the .BMP files which need to be
# created from the files in this directory.
# If you add a new .GIF, add the corresponding .BMP file here.
BMPS = \
    img\cascade.bmp \
    img\fonthint.bmp \
    img\nocascade.bmp \
    img\notrunc.bmp \
    img\properties.bmp \
    img\string.bmp \
    img\titleedit.bmp \
    img\trunc.bmp \
    img\xcenter_view.bmp \
    img\xc_pulse.bmp \
    img\xc_sentinel1.bmp \
    img\xc_sentinel2.bmp \
    img\xfsys.bmp \
    img\xfwps.bmp \
    img\xwpmedia.bmp \
    img\xwpscreen.bmp \
    img\xwsetup.bmp \
    img\trashcan.bmp

# main target
all: $(BMPS) $(COPYTO)\xfldr$(LANGUAGE).hlp
    @echo ----- Leaving $(MAKEDIR)


SRCHTML = *.html \
fsys\*.html \
glossary\*.html \
hook\*.html \
kernel\*.html \
media\*.html \
menus\*.html \
objects\*.html \
setup\*.html \
wps\*.html \
xcenter\*.html \
xshutdown\*.html

#
# Now define inference rules for the different file types.
#

IPFFILE = $(OUTPUTDIR)\hlp$(LANGUAGE).ipf

$(COPYTO)\xfldr$(LANGUAGE).hlp: $(OUTPUTDIR)\xfldr$(LANGUAGE).hlp
        cmd.exe /c copy $(OUTPUTDIR)\$(@B).hlp $(COPYTO)

$(IPFFILE): $(SRCHTML) makefile ..\$(ENTITYDEFS)
        $(HTML2IPF) xfldr$(LANGUAGE).html
        $(COPY) xfldr$(LANGUAGE).ipf $(IPFFILE)

$(OUTPUTDIR)\xfldr$(LANGUAGE).hlp: $(IPFFILE) img\*.bmp
        ipfc $(IPFFILE) $(OUTPUTDIR)\xfldr$(LANGUAGE).hlp



