#
# makefile:
#       makefile for TOOLS\ directory.
#       For use with IBM NMAKE, which comes with the IBM compilers,
#       the Developer's Toolkit, and the DDK.
#
#       All the makefiles have been restructured with V0.9.0.
#
#       Called from:    main makefile
#
#       Input:          none
#
#       Output:         all XWorkplace tools as EXEs, in this directory.
#
#       Edit "setup.in" to set up the make process.
#

# Say hello to yourself.
!if [@echo +++++ Entering $(MAKEDIR)]
!endif

all: repclass wpsreset
    @echo ----- Leaving $(MAKEDIR)

repclass:
    @cd repclass
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..

wpsreset:
    @cd wpsreset
    @nmake -nologo all "MAINMAKERUNNING=YES"
    @cd ..


