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

# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize in inference
# rules.
.SUFFIXES: .idl .h .ih .def

# OUTPUTDIR specifies the directory where we will put the
# files created by this makefile.
OUTPUTDIR = ..\include\classes

# Some SOM variables. You won't have to change these.
SOMTEMP = $(TEMP)
!if [set SOMTMP=$(TEMP)] || \
    [set SMTMP=$(SOMTEMP)] || \
    [set SMEMIT=ih;h;c] || \
    [set SMADDSTAR=1] || \
    [set SMNOTC=1]
!endif

# Specify the main target.

all: $(OUTPUTDIR)\xclslist.h \
     $(OUTPUTDIR)\xdebug.h \
     $(OUTPUTDIR)\xfdataf.h \
     $(OUTPUTDIR)\xfdesk.h \
     $(OUTPUTDIR)\xfdisk.h \
     $(OUTPUTDIR)\xfldr.h \
     $(OUTPUTDIR)\xfobj.h \
     $(OUTPUTDIR)\xfpgmf.h \
     $(OUTPUTDIR)\xfstart.h \
     $(OUTPUTDIR)\xfsys.h \
     $(OUTPUTDIR)\xfwps.h \
     $(OUTPUTDIR)\xtrash.h \
     $(OUTPUTDIR)\xwpkeybd.h \
     $(OUTPUTDIR)\xwpmouse.h \
     $(OUTPUTDIR)\xwpsetup.h \
     $(OUTPUTDIR)\xwpscreen.h \
     $(OUTPUTDIR)\xwpsound.h
    @echo ----- Leaving $(MAKEDIR)

# Now define inference rules for what to do with certain file
# types, based on their file extension.
# The syntax we need here is ".fromext.toext".
# So whenever NMAKE encounters a .toext file, it
# executes what we specify here.
# The ugly {} brackets are some awkward syntax for specifying
# files in other directories.

# -- emit .IH, .DEF files when .IDL has changed
.idl{$(OUTPUTDIR)}.h:
# emit H, IH (include\classes):
        sc -p -v -r -d$(OUTPUTDIR) -maddstar -mnoint -S128000 -C128000 -sh;ih $(@B).idl
# emit C (src\classes):
        sc -p -v -r -d..\src\classes -maddstar -mnoint -S128000 -C128000 -sc $(@B).idl
# emit DEF (this dir):
        sc -p -v -r -maddstar -mnoint -S128000 -C128000 -s def $(@B).idl
# cleanup IH: this removes the ugly #pragma's for VAC++ 3.0
        strrpl $(OUTPUTDIR)\$(@B).ih "#pragma checkout(suspend)" " "
        strrpl $(OUTPUTDIR)\$(@B).ih "#pragma checkout(resume)" " "

# Now define dependencies to recreate the .H, .IH etc. files
# using the SOM inference rule above.

# Special macros used here: $(@B) is the current target w/out ext.

$(OUTPUTDIR)\xclslist.h:         $(@B).idl

$(OUTPUTDIR)\xdebug.h:           $(@B).idl

$(OUTPUTDIR)\xfdataf.h:          $(@B).idl

$(OUTPUTDIR)\xfdesk.h:           $(@B).idl

$(OUTPUTDIR)\xfdisk.h:           $(@B).idl

$(OUTPUTDIR)\xfldr.h:            $(@B).idl

$(OUTPUTDIR)\xfobj.h:            $(@B).idl

$(OUTPUTDIR)\xfpgmf.h:           $(@B).idl

$(OUTPUTDIR)\xfstart.h:          $(@B).idl

$(OUTPUTDIR)\xfsys.h:            $(@B).idl

$(OUTPUTDIR)\xfwps.h:            $(@B).idl

$(OUTPUTDIR)\xtrash.h:           $(@B).idl

$(OUTPUTDIR)\xwpkeybd.h:         $(@B).idl

$(OUTPUTDIR)\xwpmouse.h:         $(@B).idl

$(OUTPUTDIR)\xwpscreen.h:        $(@B).idl

$(OUTPUTDIR)\xwpsetup.h:         $(@B).idl

$(OUTPUTDIR)\xwpsound.h:         $(@B).idl


