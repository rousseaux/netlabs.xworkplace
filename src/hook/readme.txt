
src\hook has code related to the XWorkplace PM hook.
This is linked into several modules:

XWPHOOK.DLL contains the actual hook code. Its source
code is in xwphook.c only.

XWPDAEMN.EXE is the XWorkplace Daemon, which installs
and configures the hook by dynamically linking to
XWPHOOK.DLL. This also has the PageMage window code.
Its source code is in the xwpdaemn.c and pgmg*.c
files.


