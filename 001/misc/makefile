
#
# makefile:
#       makefile for 001\misc directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    001
#
#       This copies the files in the current directory to
#       XWPRUNNING subdirectories, if necessary.

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

PROJECT_BASE_DIR = $(MAKEDIR)\..\..

# include setup (compiler options etc.)
!include ..\..\setup.in

# This is the language code for the language that the NLS DLL
# will support. Change this to a different language code, and
# the makefile will support your langauge.
LANGUAGE = 001

#
# DO NOT CHANGE THE FOLLOWING
#

#
# Define the suffixes for files which NMAKE will work on.
# .SUFFIXES is a reserved NMAKE keyword ("pseudotarget") for
# defining file extensions that NMAKE will recognize.
#

.SUFFIXES: .tmf

FILES_TO_COPY = \
    $(XWPRUNNING)\help\xfldr$(LANGUAGE).tmf \
    $(XWPRUNNING)\bin\crobj$(LANGUAGE).cmd \
    $(XWPRUNNING)\bin\instl$(LANGUAGE).cmd \
    $(XWPRUNNING)\bin\sound$(LANGUAGE).cmd \
    $(XWPRUNNING)\help\drvrs$(LANGUAGE).txt \
    $(XWPRUNNING)\help\xfcls$(LANGUAGE).txt

all: $(FILES_TO_COPY)
    @echo ----- Leaving $(MAKEDIR)

$(XWPRUNNING)\help\xfldr$(LANGUAGE).tmf: xfldr$(LANGUAGE).tmf
    $(COPY) $(@B).tmf $(XWPRUNNING)\help

$(XWPRUNNING)\bin\crobj$(LANGUAGE).cmd: $(@B).cmd
    $(COPY) $(@B).cmd $(XWPRUNNING)\bin

$(XWPRUNNING)\bin\instl$(LANGUAGE).cmd: $(@B).cmd
    $(COPY) $(@B).cmd $(XWPRUNNING)\bin

$(XWPRUNNING)\bin\sound$(LANGUAGE).cmd: $(@B).cmd
    $(COPY) $(@B).cmd $(XWPRUNNING)\bin

$(XWPRUNNING)\help\drvrs$(LANGUAGE).txt: $(@B).txt
    $(COPY) $(@B).txt $(XWPRUNNING)\help

$(XWPRUNNING)\help\xfcls$(LANGUAGE).txt: $(@B).txt
    $(COPY) $(@B).txt $(XWPRUNNING)\help

