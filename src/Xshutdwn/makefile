!if [@echo ***** $(MAKEDIR)\makefile: Entering]
!endif


CC      = icc /c /Ge /Gd- /Se /Re /ss /Gm+ /Ti+ /O- /Oi-
LINK    = ilink
INCLUDE = $(INCLUDE);..\main;..\helpers

all: xshutdwn.exe

xshutdwn.obj: xshutdwn.c $(HEADERS)
          @ echo $(INCLUDE)
          $(CC) xshutdwn.c

xshutdwn.res: xshutdwn.rc xshut.ico

xshutdwn.def: ..\bldlevel.h
          cmd.exe /c BuildLevel.cmd $*.def ..\bldlevel.h "XWorkplace XShutdown Interface"

xshutdwn.exe: xshutdwn.obj xshutdwn.def xshutdwn.res
          $(LINK) xshutdwn.obj xshutdwn.def
          rc -p -x xshutdwn.res xshutdwn.exe




