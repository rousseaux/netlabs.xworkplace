#
# makefile:
#       makefile for IDL directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       Called from:    main makefile
#
#       Input:          ./*.IDL
#
#       Output:         ../include/classes/*.h
#                       ../include/classes/*.ih
#                       ../src/classes/*.c
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

!include ..\setup.in

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize in inference
# rules.
.SUFFIXES: .idl .h .ih .def

# OUTPUTDIR specifies the directory where we will put the
# files created by this makefile.
OUTPUTDIR = $(PROJECT_BASE_DIR)\include\classes

# Some SOM variables. You won't have to change these.
SOMTEMP = $(TEMP)
!if [set SOMTMP=$(TEMP)] || \
    [set SMTMP=$(SOMTEMP)] || \
    [set SMEMIT=ih;h;c] || \
    [set SMADDSTAR=1] || \
    [set SMNOTC=1] || \
    [set SMINCLUDE=$(TKBASE)\idl;$(TKBASE)\h;$(TKBASE)\SOM\include;.]
!endif

!if [@echo    SMINCLUDE is $(SMINCLUDE)]
!endif

# SMTMP=$(SOMTEMP)
# SMEMIT=ih;h;c
# SMADDSTAR=1
# SMNOTC=1
# SMINCLUDE=$(TKBASE)\idl;$(TKBASE)\h;$(TKBASE)\SOM\include;.

# Specify the main target.

all: $(OUTPUTDIR)\anand.h \
     $(OUTPUTDIR)\anos2ptr.h \
     $(OUTPUTDIR)\anwani.h \
     $(OUTPUTDIR)\anwcur.h \
     $(OUTPUTDIR)\xcenter.h \
     $(OUTPUTDIR)\xclslist.h \
     $(OUTPUTDIR)\xdebug.h \
     $(OUTPUTDIR)\xdebug_folder.h \
     $(OUTPUTDIR)\xfdataf.h \
     $(OUTPUTDIR)\xfdesk.h \
     $(OUTPUTDIR)\xfdisk.h \
     $(OUTPUTDIR)\xfldr.h \
     $(OUTPUTDIR)\xfobj.h \
     $(OUTPUTDIR)\xfont.h \
     $(OUTPUTDIR)\xfontfile.h \
     $(OUTPUTDIR)\xfontobj.h \
     $(OUTPUTDIR)\xwppgmf.h \
     $(OUTPUTDIR)\xfstart.h \
     $(OUTPUTDIR)\xfsys.h \
     $(OUTPUTDIR)\xfwps.h \
     $(OUTPUTDIR)\xtrash.h \
     $(OUTPUTDIR)\xtrashobj.h \
     $(OUTPUTDIR)\xmmcdplay.h \
     $(OUTPUTDIR)\xmmvolume.h \
     $(OUTPUTDIR)\xwpadmin.h \
     $(OUTPUTDIR)\xwpfsys.h \
     $(OUTPUTDIR)\xwpkeybd.h \
     $(OUTPUTDIR)\xwpmedia.h \
     $(OUTPUTDIR)\xwpmouse.h \
     $(OUTPUTDIR)\xwpnetwork.h \
     $(OUTPUTDIR)\xwpnetsrv.h \
     $(OUTPUTDIR)\xwppgm.h \
     $(OUTPUTDIR)\xwpsetup.h \
     $(OUTPUTDIR)\xwpscreen.h \
     $(OUTPUTDIR)\xwpshadow.h \
     $(OUTPUTDIR)\xwpsound.h \
     $(OUTPUTDIR)\xwpstring.h \
     $(OUTPUTDIR)\xwpvcard.h
    @echo ----- Leaving $(MAKEDIR)

# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
# The ugly {} brackets are some awkward syntax for specifying
# files in other directories.

RUNSC = sc -p -v -r -maddstar -mnoint -S128000 -C128000
#-I $(TKBASE)\SOM\include -I . -I $(TKBASE)\idl -I $(TKBASE)\h

# -- emit .IH, .DEF files when .IDL has changed
.idl{$(OUTPUTDIR)}.h:
# emit H, IH (include\classes):
        $(RUNSC) -d$(OUTPUTDIR) -sh;ih $(@B).idl
# emit C (src\classes):
        $(RUNSC) -d$(PROJECT_BASE_DIR)\src\classes -sc $(@B).idl
# emit DEF (this dir):
        $(RUNSC) -s def $(@B).idl
# cleanup IH: this removes the ugly #pragma's for VAC++ 3.0
        $(RUN_STRRPL) $(OUTPUTDIR)\$(@B).ih "#pragma checkout(suspend)" " "
        $(RUN_STRRPL) $(OUTPUTDIR)\$(@B).ih "#pragma checkout(resume)" " "

# Now define dependencies to recreate the .H, .IH etc. files
# using the SOM inference rule above.

# Special macros used here: $(@B) is the current target w/out ext.

$(OUTPUTDIR)\anand.h:            $(@B).idl

$(OUTPUTDIR)\anos2ptr.h:         $(@B).idl

$(OUTPUTDIR)\anwani.h:           $(@B).idl

$(OUTPUTDIR)\anwcur.h:           $(@B).idl

$(OUTPUTDIR)\xcenter.h:          $(@B).idl

$(OUTPUTDIR)\xclslist.h:         $(@B).idl

$(OUTPUTDIR)\xdebug.h:           $(@B).idl

$(OUTPUTDIR)\xdebug_folder.h:    $(@B).idl

$(OUTPUTDIR)\xfdataf.h:          $(@B).idl

$(OUTPUTDIR)\xfdesk.h:           $(@B).idl

$(OUTPUTDIR)\xfdisk.h:           $(@B).idl

$(OUTPUTDIR)\xfldr.h:            $(@B).idl

$(OUTPUTDIR)\xfobj.h:            $(@B).idl

$(OUTPUTDIR)\xfont.h:            $(@B).idl

$(OUTPUTDIR)\xfontfile.h:        $(@B).idl

$(OUTPUTDIR)\xfontobj.h:         $(@B).idl

$(OUTPUTDIR)\xwppgmf.h:           $(@B).idl

$(OUTPUTDIR)\xfstart.h:          $(@B).idl

$(OUTPUTDIR)\xfsys.h:            $(@B).idl

$(OUTPUTDIR)\xfwps.h:            $(@B).idl

$(OUTPUTDIR)\xtrash.h:           $(@B).idl

$(OUTPUTDIR)\xtrashobj.h:        $(@B).idl

$(OUTPUTDIR)\xmmcdplay.h:        $(@B).idl

$(OUTPUTDIR)\xmmvolume.h:        $(@B).idl

$(OUTPUTDIR)\xwpfsys.h:          $(@B).idl

$(OUTPUTDIR)\xwpadmin.h:         $(@B).idl

$(OUTPUTDIR)\xwpkeybd.h:         $(@B).idl

$(OUTPUTDIR)\xwpmedia.h:         $(@B).idl

$(OUTPUTDIR)\xwpmouse.h:         $(@B).idl

$(OUTPUTDIR)\xwpnetwork.h:       $(@B).idl

$(OUTPUTDIR)\xwpnetsrv.h:       $(@B).idl

$(OUTPUTDIR)\xwppgm.h:           $(@B).idl

$(OUTPUTDIR)\xwpscreen.h:        $(@B).idl

$(OUTPUTDIR)\xwpsetup.h:         $(@B).idl

$(OUTPUTDIR)\xwpshadow.h:        $(@B).idl

$(OUTPUTDIR)\xwpsound.h:         $(@B).idl

$(OUTPUTDIR)\xwpstring.h:        $(@B).idl

$(OUTPUTDIR)\xwpvcard.h:         $(@B).idl


