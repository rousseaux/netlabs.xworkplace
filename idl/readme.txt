
The IDL directory contains all the IDL source files for the
various XWorkplace classes.

When any of the IDL sources is changed, the makefiles automatically
update the headers in include\classes and the C sources in src\classes
by invoking the SOM compiler upon them.

Also, the .DEF files will be created in this directory. Note that
these are not used, but only bin\xwp.def, which must be updated
manually when new classes are introduced.

See PROGREF.INF for details.


