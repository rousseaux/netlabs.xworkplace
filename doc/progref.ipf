.*----------------------------------------------------------------------------*
.*       Converted by HTML2IPF from progref.html at 5 Feb 2000, 10:34am       *
.*----------------------------------------------------------------------------*
:userdoc.
:docprof toc=12345.
:title.         XWorkplace Programmer's Guide and Reference     
.* Source filename: progref.html
:h1  res=1.         XWorkplace Programmer's Guide and Reference     
:font facename=default size=0x0.
:p.
:ul compact.
:li.
:link reftype=hd res=2.Notices:elink.
.br
:li.
:link reftype=hd res=3.Introduction -- start here:elink.
.br
:li.
:link reftype=hd res=4.National Language Support:elink.
.br
:li.
:link reftype=hd res=5.The XWorkplace source code:elink.
.br
:li.
:link reftype=hd res=6.Source code details:elink.
.br
:li.
:link reftype=hd res=7.Adding features to XWorkplace:elink.
:eul.
.br
.* Source filename: 020notices.html
:h1  x=right width=70% res=2.         Notices and Legalese     
:font facename=default size=0x0.
:p.
Please see the :hp1.XWorkplace User Guide:ehp1. for the "Notices" section&colon.
the GNU General Public Licence (GPL), the development team, credits, and
thank you's&per.
.* Source filename: 030intro.html
:h1  x=right width=70% res=3. Introduction -- Start Here 
:font facename=default size=0x0.
:p.
Welcome to the XWorkplace source code! This book has been written by
XFolder's and XWorkplace's original author, Ulrich M”ller, to introduce
you to the world of XWorkplace programming&per.
:p.:hp2.Note&colon.:ehp2. You must unzip this package on a HPFS drive
because many files in this package contain long file names&per.
:p.This INF book covers three different things&colon.
:ol compact.
:li.
Information about :hp2.XWorkplace National Language Support (NLS)&per.:ehp2. This is of
interest for people who would like to translate XWorkplace to a new language&per. No
knowledge of C programming is required for translations, but knowing HTML helps&per.
:p.See :link reftype=hd res=4."National Language Support":elink. for more information&per.
.br
:li.
Information about the :hp2.XWorkplace C source code&per.:ehp2.
This is mainly of interest
for programmers who would like to contribute to XWorkplace, find bugs, or who are just
interested in learning how the thing is functioning&per.
:p.See :link reftype=hd res=5."The XWorkplace Source Code":elink. for a general
introduction and how to compile&per.
:p.See :link reftype=hd res=6."Source Code Details":elink. for information about
specific XWorkplace features and their implementation&per. This is useful for contributors&per.
:p.See :link reftype=hd res=7."Adding Features to XWorkplace":elink. for information about
how to hook your own code into XWorkplace&per.
:eol.
.br
If you have questions, please post a message to
:font facename='Courier' size=18x12.xworkplace-dev@egroups&per.com:font facename=default size=0x0., which is the mailing list for XWorkplace
developers&per.
:p.But if you want to start writing WPS classes,
please don't start asking things like "What is an IDL file?"&per. If
you have never created a WPS class, please read the excellent courses at
EDM/2 (:link reftype=hd res=8.www&per.edm2&per.com:elink.) first,
which explain a lot of things to get you
started&per. As far as I know, XWorkplace is the most complex WPS source code
available, and it probably does not serve very well as a WPS beginner's
tutorial&per.
:p.Thanks!
.br
.* Source filename: 040nls.html
:h1  group=1 width=30% res=4.         National Language Support (NLS)     
:font facename=default size=0x0.
:p.
:link reftype=hd res=9 auto dependent.
:ul compact.
:li.
:link reftype=hd res=9.Overview:elink.
:li.
:link reftype=hd res=10.Requirements for translations:elink.
:li.
:link reftype=hd res=11.Preparations:elink.
:li.
:link reftype=hd res=12.The main NLS DLL (XFLDRxxx&per.DLL):elink.
:li.
:link reftype=hd res=13.The INF and HLP files:elink.
:li.
:link reftype=hd res=14.The HTML2IPF utility:elink.
:li.
:link reftype=hd res=15.Images and screenshots:elink.
:li.
:link reftype=hd res=16.Other files:elink.
:li.
:link reftype=hd res=17.Testing your NLS files:elink.
:eul.
.br
.* Source filename: nls_1overview.html
:h2  x=right width=70% res=9. Overview 
:font facename=default size=0x0.
:p.
:p.:hp2.Note&colon.:ehp2.
If you have already started translating XWorkplace, please do read the
:font facename='Courier' size=18x12.changelog&per.txt:font facename=default size=0x0. file&per. Please mind these important changes,
or the current XWorkplace version will not run with your NLS files properly&per.
:p.:hp2.Legal notes&per.:ehp2. XWorkplace is placed under the
GNU General Public License (GPL), including the National Language Support (NLS)&per.
If you translate the NLS part to your language, you create
"a work based on the Program" in the GPL sense&per. As a result, the
GPL automatically applies to your translation too&per.
:p.:hp2.This means that you have to publish your source code also&per.:ehp2.
:p.Now, to move away from the Legalese, I suggest that you do
not distribute XWorkplace NLS packages yourself&per. Note that this is
not a legal requirement (you may do so if you ship the source code
also), but for practical reasons, mainly the following two&colon.
:p.For one, all the NLS packages should be available from
my homepage for convience&per.
:p.Secondly, since
I might update XWorkplace after you have finished your work on your
NLS version, XWorkplace might have trouble cooperating with your
files&per. Internal IDs might change and/or new features might be
added&per.
So if you have finished creating your NLS package, please contact
me, and I will put in on my homepage and upload it to the usual
places where XWorkplace is normally available&per.
:p.This seems like the best solution
to be, because I have a list of places which carry XWorkplace, and I
can update all the NLS packages myself when anything changes with
XWorkplace&per. This way, my homepage will always be the place to look
for up-to-date NLS packages&per.
:p.
Thus, when you're done translating, please zip up the complete
directory tree again and mail the ZIP file back to me&per.
:p.
:hp2.How it works&per.:ehp2.
XWorkplace NLS packages identify themselves to the XWorkplace core via
:hp2.country codes,:ehp2. as described on the "COUNTRY" page in the
:hp1.OS/2 Command Reference:ehp1. (CMDREF&per.INF)&per.
:p.XWorkplace's language depends on exactly one setting in OS2&per.INI (application
"XFolder", key "LanguageCode")&per. If that setting is changed, XWorkplace assumes a
whole new set of language files to be present&per.
:p.This XWorkplace source package is prepared for creating the English NLS package, as
it comes with the English NLS package&per. The language code
for English is "001"&per.
:p.:hp2.So the first step you'll have to take is finding out your country
code&per.:ehp2. For example, Italian would be 039&per.
:p.
Unfortunately, XWorkplace's National Language Support (NLS) is spread across quite
a number of files, which have different file formats&per.
:p.
Basically, XWorkplace's NLS can be separated into three parts&colon.
:ol compact.
:li.
:hp2.Run-time NLS&colon.:ehp2. this is all the language-dependent
things that XWorkplace will ever display to the user, such as menus,
dialogs, messages, and notebook pages&per.
:p.Here we have&colon.
.br
:ul compact.
:li.
The :hp2.NLS DLL:ehp2. to be put into the XWorkplace :font facename='Courier' size=18x12.BIN:font facename=default size=0x0. subdirectory&per.
This file contains all language-dependent Presentation Manager (PM) resources,
mostly dialogs and menus&per. Also, a lot of strings are stored here&per.
:p.The NLS DLL is called :font facename='Courier' size=18x12.XFLDRxxx&per.DLL:font facename=default size=0x0., with "xxx" being your
three-digit language code&per.
The files neccessary to translate this DLL are in the :font facename='Courier' size=18x12.001:font facename=default size=0x0.
directory&per.
:p.See :link reftype=hd res=12."The main NLS DLL":elink. for details&per.
.br
:li.
The XWorkplace :hp2.message file:ehp2. introduced with V0&per.80 residing in the
HELP subdirectory&per. This file holds all kinds of messages which
are mostly displayed in XWorkplace's message boxes and are too large to be put into
the resources of the NLS DLL (because PM string resources are limited to 256
characters, unfortunately)&per.
:p.With V0&per.9&per.0, the message file format has changed&per. Previously, the file was
called :font facename='Courier' size=18x12.XFLDRxxx&per.MSG:font facename=default size=0x0., with "xxx" being your
three-digit language code, and an IBM utility called :font facename='Courier' size=18x12.MKMSGF&per.EXE:font facename=default size=0x0. was
required to compile the actual message file&per. Now, this file has the :font facename='Courier' size=18x12.&per.TMF:font facename=default size=0x0.
ending and is compiled at run-time into that file's extended attributes, so there's
no additional utilities needed&per.
:p.See :link reftype=hd res=12."The main NLS DLL":elink. for details&per.
.br
:li.
The :hp2.WPS class descriptions:ehp2. which are displayed on the
"WPS Classes"
settings page in the "Workplace Shell" object&per. This was introduced
with XFolder 0&per.80 also&per.
:p.This file is called :font facename='Courier' size=18x12.XFCLSxxx&per.TXT:font facename=default size=0x0., with "xxx" being your
three-digit language code&per. It also resides in the :font facename='Courier' size=18x12.001:font facename=default size=0x0. directory&per.
:p.See :link reftype=hd res=16."Other files":elink. for details&per.
.br
:li.
A number of REXX &per.CMD and text files for XWorkplace's installation&per.
The files to be translated are in the :font facename='Courier' size=18x12.MISC:font facename=default size=0x0. subdirectory&per.
:p.See :link reftype=hd res=16."Other files":elink. for details&per.
:eul.
.br
.br
:li.
:hp2.Documentation&colon.:ehp2. the XWorkplace User Guide (INF file) and online
help (HLP file)&per. This is not absolutely necessary and probably the larger
part of the translation work, since I have written so much text over time&per.
:p.Here we have&colon.
.br
:ul compact.
:li.
The :hp2.XWorkplace Online Reference (INF):ehp2. in the XWorkplace main directory&per.
:p.This file is called :font facename='Courier' size=18x12.XFLDRxxx&per.INF:font facename=default size=0x0., with "xxx" being your
three-digit language code&per.
The HTML source files neccessary to translate this file are in the :font facename='Courier' size=18x12.001/INF&per.001:font facename=default size=0x0.
subdirectory&per.
:p.See :link reftype=hd res=13."The INF and HLP files":elink. for details&per.
.br
:li.
The :hp2.XWorkplace HLP file:ehp2. in the HELP subdirectory&per. This file holds
all the help panels that are displayed when you press F1&per.
:p.This file is called :font facename='Courier' size=18x12.XFLDRxxx&per.HLP:font facename=default size=0x0., with "xxx" being your
three-digit language code&per.
The files neccessary to translate this file are in the :font facename='Courier' size=18x12.001/HELP&per.001:font facename=default size=0x0.
subdirectory&per.
:p.See :link reftype=hd res=13."The INF and HLP files":elink. for details&per.
:eul.
.br
.br
:li.
:hp2.Installation&colon.:ehp2. a number of files which are only used while
XWorkplace is being installed&per. These set up XWorkplace objects and INI keys
in a certain language&per. Please see the README in the :font facename='Courier' size=18x12.MISC:font facename=default size=0x0.
directory for details&per.
.br
:li.
A SmartGuide (Warp 4) script is used for the XWorkplace introduction, which is
displayed right after XWorkplace has been installed and the WPS has been restarted&per.
This (:font facename='Courier' size=18x12.XFLDRxxx&per.SGS:font facename=default size=0x0.) is also in the :font facename='Courier' size=18x12.MISC:font facename=default size=0x0. directory&per.
:p.See :link reftype=hd res=16."Other files":elink. for details&per.
:eol.
.br
The :font facename='Courier' size=18x12.TOOLS:font facename=default size=0x0. directory contains a valuable tool for converting
HTML files to the IBM (IPF) format&per. This will be described on a
:link reftype=hd res=14.separate page&per.:elink.
This directory also contains a few other needed tools&per. See the
separate README in that directory&per.
:p.
BTW&colon. I'm really curious how "Snap to grid" sounds in your language&per. :font facename='Courier' size=18x12.;-):font facename=default size=0x0.
.br
.* Source filename: nls_2requires.html
:h2  x=right width=70% res=10. Required Tools 
:font facename=default size=0x0.
:p.
Even though all
the source files in this package can be edited using any editor (because
they're all plain text files), you will need some extra utilities to create
the actual NLS files&per.
:p.
:hp2.In any case, a C compiler is:ehp2. :hp1.not:ehp1. :hp2.required:ehp2.&per.
:p.
To be more precise&colon.
:ul compact.
:li.
In order to create :font facename='Courier' size=18x12.&per.INF:font facename=default size=0x0. and :font facename='Courier' size=18x12.&per.HLP:font facename=default size=0x0.
files, you need IBM's "Information Presentation Facility Compiler" (:font facename='Courier' size=18x12.IPFC&per.EXE:font facename=default size=0x0.)&per.
This thing is part of the OS/2 Developer's Toolkit, which, unfortunately, still
isn't free&per. :hp1.However,:ehp1. I have now found that IPFC is contained in the
:hp2.Device Driver Development Kit (DDK):ehp2., which IBM has put online at
:link reftype=hd res=18.http&colon.//service&per.boulder&per.ibm&per.com/dl/ddk/priv/ddk-d:elink.&per.
You will need to register, but then downloads are free&per. Download :font facename='Courier' size=18x12.combase&per.zip:font facename=default size=0x0.,
which has IPFC plus a number of other tools&per. After unpacking, IPFC is in the
:font facename='Courier' size=18x12.DDK\base\tools:font facename=default size=0x0. directory, which you should include in your PATH&per.
.br
:li.
The makefiles in this package were written for :hp2.IBM NMAKE,:ehp2. which comes
with all the IBM compilers (and also with the DDK mentioned above)&per.
I'm not sure these will work with other MAKE utilities, such as
DMAKE or GNU make&per. I'd be grateful for feedback&per.
:p.The makefiles are only for conveniently updating the target NLS files&per.
If you don't have NMAKE, you can still assemble the files manually&per. See the
following pages for details&per.
.br
:li.
XWorkplace V0&per.9&per.0 no longer uses OS/2 message files (those :font facename='Courier' size=18x12.&per.MSG:font facename=default size=0x0.
things)&per. Instead, we have developed a new format which uses plain text ASCII
files (with a :font facename='Courier' size=18x12.&per.TMF:font facename=default size=0x0. extension) as input, so :font facename='Courier' size=18x12.MKMSGF&per.EXE:font facename=default size=0x0.
is no longer needed to compile message files&per.
:eul.
.br
.br
.* Source filename: nls_3prep.html
:h2  x=right width=70% res=11. Preparations 
:font facename=default size=0x0.
:p.
The files in the "001" directory contain everything neccessary to do a
complete translation of XWorkplace&per.
.br
Here's a list of things you need to do before you can begin translations&per.
:ol compact.
:li.
The first thing you need to do is find out your
:hp2.three-digit language code&per.:ehp2. Open the OS/2 Command Reference, "Country"
page, and find the code for your country&per.
.br
:li.
Make a copy of the entire :font facename='Courier' size=18x12.001:font facename=default size=0x0. directory tree within
the XWorkplace source tree, renaming it to your country code&per.
.br
:li.
Most files in the :font facename='Courier' size=18x12.001:font facename=default size=0x0. directory carry a three-digit language code in
their respective names&per. For your language, you need to change all the filenames
with "001" in their names to your country code (e&per.g&per. 039 for Italian)&per.
For example, rename :font facename='Courier' size=18x12.xfldr001&per.dlg:font facename=default size=0x0. to :font facename='Courier' size=18x12.xfldr039&per.dlg:font facename=default size=0x0.&per.
.br
:li.
You will also have to change the :font facename='Courier' size=18x12.&per.DEF:font facename=default size=0x0. file and :font facename='Courier' size=18x12.makefile:font facename=default size=0x0.,
which assume a country code of 001 at this point&per. Required changes are noted in
the files themselves&per.
.br
:li.
Open :font facename='Courier' size=18x12.xfldrXXX&per.rc:font facename=default size=0x0. (with "xxx" being your country code) and
find the strings "ID_XSSI_DLLLANGUAGE" and "ID_XSSI_NLS_AUTHOR"&per. Change those
two to match your language and name; this is the information that is displayed
to the user in the "Language" drop-down box in the "XWorkplace Setup" settings
object&per.
:eol.
.br
You should now be ready to :hp2.recompile the NLS DLL:ehp2. for the first time&per.
:p.
If you have IBM VAC++, you can simply use :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. to have the DLL
recreated&per. Open :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. and check somewhere in the middle where
the script checks for the presence of the :font facename='Courier' size=18x12.049:font facename=default size=0x0. directory, and change
the variables to point to your new directory instead&per.
Running :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. should then work fine, since all the neccessary
files are included&per.
:p.
Otherwise, things get a bit more complicated&per. Since the
resource compiler (:font facename='Courier' size=18x12.RC&per.EXE:font facename=default size=0x0.) is already included
with every OS/2 installation, you can try the following&colon.
:ol compact.
:li.
Copy an existing XWorkplace resource DLL (e&per.g&per. :font facename='Courier' size=18x12.xfldr001&per.dll:font facename=default size=0x0.)
into your new NLS directory;
rename it so that it contains your language code (e&per.g&per. :font facename='Courier' size=18x12.xfldr039&per.dll:font facename=default size=0x0.)&per.
.br
:li.
Open a command line in that directory&per.
.br
:li.
Type :font facename='Courier' size=18x12.rc xfldr039&per.rc xfldr039&per.dll:font facename=default size=0x0. (replace "039" with your language code),
which should create a new :font facename='Courier' size=18x12.&per.RES:font facename=default size=0x0. file and link it against the existing DLL&per.
:eol.
.br
After recompiling, you can test the DLL as described in
:link reftype=hd res=17."Testing the DLL":elink.&per.
.br
.* Source filename: nls_4dll.html
:h2  x=right width=70% res=12. The NLS DLL 
:font facename=default size=0x0.
:p.
:p.:hp2.Files that need to be changed&colon.:ehp2.
:ul compact.
:li.
:font facename='Courier' size=18x12.xfldr001&per.def&colon.:font facename=default size=0x0. The module definition file&per.
Required changes are noted in the file itself&per.
.br
:li.
:font facename='Courier' size=18x12.makefile&colon.:font facename=default size=0x0. Makefile for IBM NMAKE&per.
Required changes are noted in the file itself&per.
.br
:li.
:font facename='Courier' size=18x12.xfldr001&per.rc, xfldr001&per.dlg&colon.:font facename=default size=0x0.
These are the main resource files which need :hp1.lots:ehp1. of
changes&per.
:p.I've documented everything you need to change
in the :font facename='Courier' size=18x12.&per.RC:font facename=default size=0x0. file&per.
:p.The :font facename='Courier' size=18x12.&per.DLG:font facename=default size=0x0. file is included when the RC files is recompiled&per. It
contains all the XWorkplace dialogs (i&per.e&per. notebook pages and other dialog windows)&per.
Sorry, there are no comments in there because I'm using :font facename='Courier' size=18x12.DLGEDIT&per.EXE:font facename=default size=0x0.
to create the dialogs, which rewrites the DLG file at every change, so all comments
in there get lost&per.
:eul.
.br
:hp2.Using a dialog editor&per.:ehp2.
You can use :font facename='Courier' size=18x12.DLGEDIT&per.EXE:font facename=default size=0x0., the IBM dialog editor from the Developer's Toolkit,
to change the dialogs&per. (This will not affect the :font facename='Courier' size=18x12.&per.RC:font facename=default size=0x0. file&per.) For this,
you will need to create the :font facename='Courier' size=18x12.&per.RES:font facename=default size=0x0. file first&per. This is done by
:font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0.; alternatively, you can start :font facename='Courier' size=18x12.RC&per.EXE:font facename=default size=0x0. with the "-r" option,
which creates a :font facename='Courier' size=18x12.&per.RES:font facename=default size=0x0. instead of linking the resources to an
executable&per.
:p.If you then open the :font facename='Courier' size=18x12.&per.RES:font facename=default size=0x0. file with the dialog editor, you can
choose :font facename='Courier' size=18x12.src/main/dlgids&per.h:font facename=default size=0x0. as an include file so that all the numerical ID's
have a more meaningful name&per.
:p.
When you then save the file, the :font facename='Courier' size=18x12.&per.RES:font facename=default size=0x0. and :font facename='Courier' size=18x12.&per.DLG:font facename=default size=0x0. files will be
recreated by DLGEDIT&per.
The :font facename='Courier' size=18x12.&per.RC:font facename=default size=0x0. file remains untouched though&per.
:p.
The dialog editor has a helpful "Translate mode" in its "Options" menu
which disables a lot of menu items so you don't accidentally change dlg ID's
or other important stuff&per.
:p.
I don't know if you can use the URE editor also, I have not tried that&per.
.br
.* Source filename: nls_5infhlp.html
:h2  x=right width=70% res=13. The INF and HLP Files 
:font facename=default size=0x0.
:p.
The sources for the INF and HLP files are written using a slightly extended
HTML syntax&per. Creating INF and HLP files is therefore a two-step process&colon.
:ol compact.
:li.
The HTML sources are translated into an &per.IPF source file, using
the :link reftype=hd res=14.HTML2IPF:elink. utility&per.
.br
:li.
Next, the &per.IPF source is fed into IBM's :font facename='Courier' size=18x12.IPFC&per.EXE:font facename=default size=0x0. from the Developer's
Toolkit&per. See :link reftype=hd res=11."Preparations":elink. for where to get this utility&per.
:eol.
.br
Please use the :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. file on the top level of the XWorkplace source
files to have the doc files created&per. This might take a while&per. You will
first need to set a few environment variables on top of :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0., which
is documented in that file itself&per.
:p.
You then have two alternatives for the translations&colon.
:ul compact.
:li.
I recommend translating all the :font facename='Courier' size=18x12.&per.HTML:font facename=default size=0x0. files and
keep using :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0.
to have the HTML files converted into a single &per.IPF file
(using the HTML2IPF utility), which can then be fed into :font facename='Courier' size=18x12.IPFC&per.EXE:font facename=default size=0x0.&per.
:p.This has the advantage that you'll only have to change some panels
in future versions, of which I will keep track&per. Also, I always note changes
to the INF and HLP files in the HTML source files in HTML comments tags, so
you can easily search for what's changed&per.
:p.The disadvantage is that, being a REXX program, HTML2IPF is slow&per.
.br
:li.
You can also translate the :hp1.one:ehp1. :font facename='Courier' size=18x12.&per.IPF:font facename=default size=0x0.
file in each of the INF and
HELP directories directly, if you're familiar with the IPF source
language, which is a proprietary IBM format (and which I don't like
at all)&per.
:p.To do this, you'll have to create the two IPF source files first, using
:font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0.&per. You can then translate this file and
feed it into the :font facename='Courier' size=18x12.IPFC&per.EXE:font facename=default size=0x0. compiler
to have the &per.INF and &per.HLP files created&per.
:p.This has the disadvantage that if certain panels change in future
versions, you'll have no clue what parts changed in this single IPF
file, because the comments about changes are only visible in the HTML sources&per.
:p.If you still want to use this method, please change the text only&per.
Do not change any resources, IPF tags,
and especially not the resource ID's, or XWorkplace will not find its
help panels any more&per.
:eul.
.br
The "root" file in each of the two source directories
is called :font facename='Courier' size=18x12.XFLDRxxx&per.HTML:font facename=default size=0x0., respectively, with "xxx" being your
country code (which you should change in this one filename because HTML2IPF will use this
name for the target IPF file also)&per.
:p.To :hp2.use HTML2IPF manually:ehp2. (instead of through :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0.),
open a command line in the INF or HLP
directory (with XWorkplace :font facename='Courier' size=18x12.;-):font facename=default size=0x0. ), and type&colon.
:p.:font facename='Courier' size=18x12.<path>\html2ipf xfldr001&per.html:font facename=default size=0x0.
:p.This will create a :font facename='Courier' size=18x12.XFLDRxxx&per.IPF:font facename=default size=0x0. file (in the above example,
XFLDR001&per.IPF)&per. You can then run :font facename='Courier' size=18x12.ipfc XFLDRxxx&per.IPF:font facename=default size=0x0. to compile
a HLP file; add an :font facename='Courier' size=18x12./INF:font facename=default size=0x0. switch to produce an "INF" file&per.
You must have the INF or HLP directory as your current directory,
or HTML2IPF will tumble&per.
:p.
In any case, mind these important notes&colon.
:ul compact.
:li.
The (slightly awkward, I admit) structure of the files is the
following, in both directories&colon.
.br
:li.
:font facename='Courier' size=18x12.XFLDRxxx&per.HTML:font facename=default size=0x0. is the "root" file&per.
Files beginning with "0" appear in the contents tree of the
produced INF and HLP files&per.
All other files are somwhere below in the INF/HLP table-of-contents
hierarchy, depending on the :font facename='Courier' size=18x12.SUBLINKS:font facename=default size=0x0. tags in the HTML source files&per.
.br
:li.
Do not change filenames! HTML2IPF sorts the IPF pages internally
alphabetically according to the filenames, that's why I've
implemented these strange naming conventions&per. I admit that since
the structure has developed over time, it may not seem very
logical to you, but it works&per. Changing things here will result
in a lot of work, since all the links in the HTML files will
have to be renamed also&per.
:p.Also, if you change the linkage of the HTML files, the resource
IDs of the help panels will be altered, and XWorkplace will then
get confused displaying the proper help panels&per.
.br
:li.
Within the HTML files, do not change :hp1.anything:ehp1. within angle
brackets ("<xxx>")&per. Translate :hp1.only:ehp1. the text outside of these&per.
Even though some tags might not be good HTML style and the
HTML files might not look pretty when viewed with Netscape,
certain tag combinations (especially the <BR><LI> combinations)
are neccessary to make the pages look good for IPF&per.
.br
:li.
Don't forget to translate the page titles (in between the
<TITLE> tags)&per. This is what appears in the title of a panel window
and in the "Table of contents" tree&per. I've forgotten this many times&per.&per.&per.
.br
:li.
There are a number of files in the HLP directory which seem
pretty useless&per. These are neccessary however in order to
guarantee a certain sequence of the help panels in the
resulting HLP file, on which XWorkplace relies for calling
a certain help panel&per. I don't quite understand myself anymore
how this order works, but it does, so I don't care&per. :font facename='Courier' size=18x12.;-):font facename=default size=0x0.
.br
:li.
I do not recommend translating :hp1.everything:ehp1. for the INF book&per.
I have done the German translation myself,
and I think that the "Revision history" section makes no
sense translating, because it is frequently updated and
NLS versions didn't exist for the older versions anyway&per.
Also, the "XWorkplace Internals" pages apply to programmers,
which need to know English anyway, because otherwise they
won't find their way through the required Toolkit docs
either&per. So I think you can save yourself some work there&per.
:eul.
.br
.* Source filename: nls_6html2ipf.html
:h2  x=right width=70% res=14. The HTML2IPF Utility 
:font facename=default size=0x0.
:p.
:p.
The :font facename='Courier' size=18x12.TOOLS:font facename=default size=0x0. directory contains the very valuable HTML2IPF tool by
Andrew Pavel Zabolotny&per. This REXX script is capable of creating a single
:font facename='Courier' size=18x12.&per.IPF:font facename=default size=0x0. file from a list of linked HTML files, which can then be passed to
IBM's :font facename='Courier' size=18x12.IPFC&per.EXE:font facename=default size=0x0. for creating :font facename='Courier' size=18x12.&per.HLP:font facename=default size=0x0. and :font facename='Courier' size=18x12.&per.INF
:font facename=default size=0x0. files&per.
:p.
I have slightly modified this tool (see notes below)&per.
:p.
Even if you don't know what I'm talking about, in this directory, you
need to open :font facename='Courier' size=18x12.HTML2IPF&per.CMD:font facename=default size=0x0. and change three, maybe four things&colon.
:ul compact.
:li.
In line 27, translate the :font facename='Courier' size=18x12.Resources on the Internet:font facename=default size=0x0. string&per.
:li.
In lines 28-31, translate the string beginning with :font facename='Courier' size=18x12.This chapter contains&per.&per.&per.:font facename=default size=0x0.
translate every line, but do not mess with the :font facename='Courier' size=18x12.'||'0d0a'x:font facename=default size=0x0. codes
and such&per.
:li.
In lines 34/35 translate the :font facename='Courier' size=18x12.Click below to launch [code] with this URL&colon.:font facename=default size=0x0.
string, but do not change the code in the middle&per.
:li.
In line 20, change the image converter; this is thoroughly explained in
the :link reftype=hd res=15.Images:elink. section&per.
:eul.
.br
:hp2.Additional information (not obligatory):ehp2.
:p.
I have included HTML2IPF&per.INF, which describes how the tool works&per. You
need not bother with this though, because everything is already set up
properly&per.
:p.
I have changed HTML2IPF&per.CMD in a few places to allow for IPF window
positioning and certain extra character formatting&per. These changes are
only documented here, in case you're interested -- you will NOT find
these changes in the INF file (you don't have to know this, just if
you're curious)&colon.
:ul compact.
:li.
The <HTML> tag has new attributes&colon. XPOS= and YPOS= work just like
IPF's "x" and "y" tags; WIDTH= and HEIGHT are the same as in IPF&per.
:li.
Added the strings described above to global variables for NLS&per.
:li.
The <A> tag accepts a new "AUTO=" attribute, which works just like
"HREF=", but automatically opens and closes the window (this is, for
example, used on the "Introduction" page of the :hp1.XWorkplace Online Reference:ehp1.)&per.
:li.
The <CITE> and </CITE> formatting tags are now set to use a
non-proportional font, which is used extensively&per.
:li.
Some formatting changes (<UL>, <BR>, <B> etc&per.)&per.
:li.
HTML2IPF now removes indenting spaces at the beginning of lines
because these would all appear in the INF/HLP files, while HTML
ignores them&per. (New with V0&per.81)&per.
:eul.
.br
.* Source filename: nls_6images.html
:h2  x=right width=70% res=15. Images and Screenshots 
:font facename=default size=0x0.
:p.
:p.
:p.
The images in both the INF and HELP directory are always available
in both GIF and uncompressed OS/2 1&per.3 BMP format&per. The reason for
this is that HTML does not support BMP, and IPF does not support
GIF&per. :font facename='Courier' size=18x12.&colon.-(:font facename=default size=0x0.
:p.HTML2IPF is very comfortable in this respect&colon. it will
automatically convert all GIF images to BMP format every time it
finds an <IMG> tag, but only if no BMP file of the same filestem
was found (see :font facename='Courier' size=18x12.HTML2IPF&per.INF:font facename=default size=0x0. for details)&per.
:p.
The image conversion program which HTML2IPF requires is specified
in line 20 of :font facename='Courier' size=18x12.HTML2IPF&per.CMD:font facename=default size=0x0.&per. The original had Image Alchemy in here,
I have changed it to "GBMSIZE", which is part of the freeware
"Generalized Bitmap Module" (GBM) package available at Hobbes&per.
:ul compact.
:li.
If you don't have GBMSIZE on your PATH, you can add the path to
this line&per.
.br
:li.
If you wish to use a different converter, specify it here&per. Be
careful though&colon. :hp2.IPFC only supports uncompressed OS/2 1&per.3 bitmaps:ehp2.
(see :font facename='Courier' size=18x12.HTML2IPF&per.INF:font facename=default size=0x0.)&per. That is, neither compressed bitmaps nor
Windows or OS/2 2&per.0 bitmaps work&per. This is annoying, because
IPFC's own image compression is totally outdated, but there's nothing we
can do about this&per.
:eul.
.br
I'd be very grateful if you could create your own screenshots
for the online documentation, because I own German versions of OS/2
and for your NLS package, screenshots of your language are
preferrable, at least for those pictures which have language-dependent
stuff in them&per.
:p.
I have used the following settings for the screenshots (just for your
information, you don't have to use these)&colon.
:cgraphic.
Fonts used&colon.
Titlebars&colon. Humanist 521, 13 points (available on the
           CorelDraw 4 CD in Type 1 format)
All the other fonts are set to 9&per.WarpSans&per.
CandyBarZ installed, colors&colon.
Active&colon.     top 191/0/0, bottom 52/0/0
Inactive&colon.   top 160/160/130, bottom 40/40/40
Oh yes, XWorkplace installed&per. ;-)
:ecgraphic.
:p.
Now, if you create your own screenshots, save them as GIFs with the
exact filenames of the originals (e&per.g&per. :font facename='Courier' size=18x12.trunc&per.gif:font facename=default size=0x0.); you must then
DELETE the respective BMP file, because HTML2IPF will only call the
image converter for BMP if no BMP file of the same name exists&per.
:p.
:hp2.Tricks to reduce file sizes&colon.:ehp2.
:p.
Keep in mind that HLP/INF files have their own compression scheme which
works best when much redundant data is in the bitmap files&per. That is,
large areas of a plain color can be compressed best&per.
:p.So taking screenshots of folders with background bitmaps is a no-no, because
this will really blow up the files&per.
:p.And use as few colors as possible&per. You can use PMView to reduce the number
of colors to, say, 12 colors, which usually still looks alright&per. Make
sure you don't have "dithering" or "error diffusion" enabled, because
this will add lots of extra pixels which will make compression less
efficient&per.
:p.Also, if you need to scale images to a smaller size, make sure not to
enable any "interpolation", i&per.e&per. introducing extra pixels to make colors smoother&per.
This adds lots more colors to the file, which cannot be compressed well&per.
.br
.* Source filename: nls_7misc.html
:h2  x=right width=70% res=16. Directory "MISC" 
:font facename=default size=0x0.
:p.
:p.
The :font facename='Courier' size=18x12.MISC:font facename=default size=0x0. directory contains files used by XWorkplace's installation script
(INSTALL&per.CMD) plus the SmartGuide Script used for the XWorkplace
introduction&per.
:ul compact.
:li.
:font facename='Courier' size=18x12.INSTxxx&per.MSG:font facename=default size=0x0. (with "xxx" being your country code, which you need to
change) is -- I'm sorry -- in a proprietary format&per. It's not difficult
to understand though&colon. The file is used by :font facename='Courier' size=18x12.XHELP&per.CMD:font facename=default size=0x0. (in the XWorkplace
package), which is capable of extracting single text messages in
between the :font facename='Courier' size=18x12.<TOPIC>:font facename=default size=0x0.; and :font facename='Courier' size=18x12.</TOPIC>:font facename=default size=0x0.
tags in this file&per. The text
between these tags is then displayed on the screen&per.
:p.
:hp2.Please note that this file will disappear in a future release of XWorkplace, as soon
as the WarpIn installer is functioning, which I am currently working on&per. See my WWW
homepage for details&per.:ehp2.
:p.
If you still wish to translate this file,
what you need to do here is simply translate all the text which follows
a :font facename='Courier' size=18x12.<TOPIC>:font facename=default size=0x0. tag&per. The text is displayed "as is",
and no formatting is
performed; as a result, you must take care that no more than 80
characters are contained in a line&per.
:p.
You also should take care of the line breaks&colon. it makes a difference
in output whether a :font facename='Courier' size=18x12.</TOPIC>:font facename=default size=0x0. end tag is found at the end of a line
or at the beginning of a new line, because in the latter case, the
line break is still printed to the screen&per.
:p.
Just one more note&colon. Do not change the keys mentioned in this file
("X", "Y", "N"), even if your language
does not use "Y" for saying
"Yes"&per. Unfortunately, :font facename='Courier' size=18x12.INSTALL&per.CMD:font facename=default size=0x0. relies on these keys&per. :font facename='Courier' size=18x12.&colon.-(:font facename=default size=0x0.
.br
:li.
:font facename='Courier' size=18x12.CROBJxxx&per.CMD:font facename=default size=0x0. is a straightforward REXX script which creates the
default XWorkplace Configuration Folder&per. Even if you don't know REXX,
don't worry&colon. you only have to change the strings on top of the file,
which contain all the language-dependent things&per. Be careful with the quotes&per.
Do not change anything else, because XWorkplace relies on it&per.
.br
:li.
:font facename='Courier' size=18x12.SOUNDxxx&per.CMD:font facename=default size=0x0. is the REXX script which creates the neccessary INI
entries for having the new XWorkplace system sounds in your "Sound"
object&per. Only change the strings at the top of the file to your language&per.
.br
:li.
:font facename='Courier' size=18x12.XFLDRxxx&per.SGS:font facename=default size=0x0. is a Warp 4 SmartGuide script to display the "Welcome"
window after XWorkplace has been installed and the WPS has been restarted&per.
:p.I have no idea what the precise syntax for these files is (once again
has IBM developed good software, but then lets it rot), but it seems
to be some HTML-like syntax, except that :hp2.the tags MUST be in
lower case,:ehp2. or they won't work&per.
:eul.
.br
.* Source filename: nls_8testing.html
:h2  x=right width=70% res=17. Testing Your NLS Files 
:font facename=default size=0x0.
:p.
XWorkplace's NLS depends entirely on a single entry in :font facename='Courier' size=18x12.OS2&per.INI:font facename=default size=0x0.
("XFolder"&colon.&colon."LanguageCode")&per. This entry defaults to a "001" string and can
be changed either at installation time (if the install script does it) or
by using the "Language" setting in the "XWorkplace Setup" object&per.
:p.All NLS resources are loaded depending on this one setting&per. That is,
if you change the language, XWorkplace expects not only your NLS DLL to be present
in :font facename='Courier' size=18x12./BIN:font facename=default size=0x0., but also the TMF, HLP, and WPS class description
files in :font facename='Courier' size=18x12./HELP:font facename=default size=0x0.&per. So if you have not translated these yet, you might
want to create a copy of the English ones with your language code in the filename&per.
:p.To test a new version of your NLS files&colon.
:ul compact.
:li.
Make sure you do not currently have your new :hp2.NLS DLL:ehp2. selected on the
"XWorkplace Internals" settings page, because if you do, the DLL is locked
by XWorkplace
and cannot be replaced&per.
:p.Select "US English" instead, which will unlock the
previously used DLL, which can then be deleted from the XWorkplace directory&per.
:p.Then put your new DLL into the :font facename='Courier' size=18x12.BIN:font facename=default size=0x0. subdirectory and open
the settings again; select your DLL and see if things work&per.
.br
:li.
Additional caveat for :hp2.INF/HLP files&colon.:ehp2.
Before compiling the &per.IPF to the &per.HLP/&per.INF files, you should make
sure that the target file is not currently in use&per.
:p.With INF files, that's easy&colon. simply close it if it's open&per.
:p.With HLP files, you have to keep in mind that the WPS keeps these
files locked even after you've closed a help panel window&per. However,
the WPS only ever keeps a single HLP file locked at a time, so in order to
unlock the XWorkplace HLP file, simply open a default WPS help panel,
e&per.g&per. by selecting "Extended Help" for the Desktop window&per.
:p.You can then copy the new XWorkplace &per.HLP file to the XWorkplace HELP
directory&per. (:font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. will do this automatically&per.)
:eul.
.br
.* Source filename: 050source1.html
:h1  group=1 width=30% res=5.         The XWorkplace Source Code     
:font facename=default size=0x0.
:p.
:link reftype=hd res=19 auto dependent.
:ul compact.
:li.
:link reftype=hd res=19.Overview:elink.
:li.
:link reftype=hd res=20.Requirements for compiling:elink.
:li.
:link reftype=hd res=21.Making (building) XWorkplace:elink.
:li.
:link reftype=hd res=22.Makefile environment variables:elink.
:li.
:link reftype=hd res=23.Troubleshooting:elink.
:li.
:link reftype=hd res=24.Creating the code documentation:elink.
:li.
:link reftype=hd res=25.The SRC\CLASSES\ directory:elink.
:li.
:link reftype=hd res=26.The SRC\HELPERS\ directory:elink.
:li.
:link reftype=hd res=27.XWorkplace function prefixes:elink.
:li.
:link reftype=hd res=28.Debugging XWorkplace:elink.
:eul.
.br
.* Source filename: src1_1overview.html
:h2  x=right width=70% res=19. Overview 
:font facename=default size=0x0.
:p.
Before going into the details of source code, which is complex enough to get
even the more experienced programmer confused,
:hp2.please read the chapters in the "XWorkplace internals" chapter
of the XWorkplace Online reference:ehp2. for a first introduction to XWorkplace's basic
inner workings&per.
:p.This will give you a vague idea of what XWorkplace is doing where
and deals with the most important concepts only, so you don't get
overwhelmed by all the details which come in this file&per.
:p.
The different :hp2.subdirectories:ehp2. contain the different parts of XWorkplace&per.
The layout of the subdirectories has been changed with XWorkplace V0&per.9&per.0 to
allow for easier addition of new features by people other than me&per. Also,
since XWorkplace will be on the Netlabs CVS server, many changes had to be
made to the makefiles and code layout&per.
:ul compact.
:li.
The :font facename='Courier' size=18x12.001\:font facename=default size=0x0. directory tree still contains all the
:link reftype=hd res=4.National Language Support (NLS):elink.
for English&per.
.br
:li.
The :font facename='Courier' size=18x12.BIN\:font facename=default size=0x0. directory contains binary object files which
are created from the sources (below) by the various makefiles&per. If that
directory doesn't exist (it's not part of the CVS tree, for many reasons),
it will be created automatically by the makefiles&per.
.br
:li.
The :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. directory contains SOM IDL source files for
the various XWorkplace classes&per. The :font facename='Courier' size=18x12.&per.DEF:font facename=default size=0x0. files in this
directory are created automatically by the SOM compiler (:font facename='Courier' size=18x12.SC&per.EXE:font facename=default size=0x0.),
but not used in the build process (see below)&per. When invoked by the
makefile, the SOM compiler is told to update the sources in :font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0.
and write the class header files to :font facename='Courier' size=18x12.INCLUDE\CLASSES:font facename=default size=0x0.&per.
:p.In general, all SOM code must be in the :font facename='Courier' size=18x12.CLASSES:font facename=default size=0x0. subdirectories&per.
Since several people might be working on the same classes (one example is the
XWPMouse class, which is used for both the XWorkplace hook and the animated
mouse pointers), the code in :font facename='Courier' size=18x12.CLASSES:font facename=default size=0x0. should only call implementation
code in other directories of :font facename='Courier' size=18x12.SRC:font facename=default size=0x0., which should be prototyped in
a corresponding subdirectory of :font facename='Courier' size=18x12.INCLUDE:font facename=default size=0x0.&per.
.br
:li.
The :font facename='Courier' size=18x12.INCLUDE\:font facename=default size=0x0. directory tree has all the headers for
XWorkplace&per.
:p.In that tree, :font facename='Courier' size=18x12.INCLUDE\CLASSES\:font facename=default size=0x0. has the headers which are
generated by the SOM compiler from the IDL files in the :font facename='Courier' size=18x12.IDL:font facename=default size=0x0.
directory (above)&per.
:p.:font facename='Courier' size=18x12.INCLUDE\HELPERS\:font facename=default size=0x0. has the headers for the files in :font facename='Courier' size=18x12.SRC\HELPERS\:font facename=default size=0x0.
(see below)&per.
:p.:font facename='Courier' size=18x12.INCLUDE\FILESYS\:font facename=default size=0x0. has the headers for the files in :font facename='Courier' size=18x12.SRC\FILESYS\:font facename=default size=0x0.
(see below)&per.
:p.:font facename='Courier' size=18x12.INCLUDE\SHARED\:font facename=default size=0x0. has headers for code which might be used by several
parts of XWorkplace, developed by several people, and corresponds to
:font facename='Courier' size=18x12.SRC\SHARED:font facename=default size=0x0.&per. I guess you get the idea now&per.
.br
:li.
The :font facename='Courier' size=18x12.SRC\:font facename=default size=0x0. directory tree contains the actual C/CPP source
files for XWorkplace&per.
:p.In that tree, :font facename='Courier' size=18x12.SRC\CLASSES\:font facename=default size=0x0. has
the SOM code for the WPS classes which will be compiled into :font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0.&per.
It is this DLL which loads the NLS DLL
(:font facename='Courier' size=18x12.XFLDRxxx&per.DLL:font facename=default size=0x0., with xxx being a language code) at WPS bootup
or when the language is changed using the "XWorkplace Internals"
settings page&per. See :link reftype=hd res=25.this page:elink. for more&per.
:p.The :font facename='Courier' size=18x12.SRC\HELPERS\:font facename=default size=0x0. subdirectory contains a lot of C files with
helper functions which are independent of XWorkplace&per. That is, these can be used with
any OS/2 VIO and/or PM program&per. The functions are grouped into categories&per.
See :link reftype=hd res=26.this page:elink. for more&per.
:p.The :font facename='Courier' size=18x12.SRC\HELPERS\:font facename=default size=0x0. dir also contains files from Dennis Bareis' PMPRINTF package,
which is available in full from his homepage&per. Refer to the
:link reftype=hd res=28."Debugging XWorkplace":elink. section
for details&per.
:p.The other subdirectories in :font facename='Courier' size=18x12.SRC\:font facename=default size=0x0. contain implementation code for
:font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0. (called by the SOM code in :font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0.)
or external programs which are part of XWorkplace, such as NetscapeDDE and Treesize&per.
.br
:li.
:font facename='Courier' size=18x12.TOOLS\:font facename=default size=0x0. contains some tools used by the makefiles&per.
See :link reftype=hd res=21.Making XWorkplace:elink.&per.
:eul.
.br
.* Source filename: src1_2requires.html
:h2  x=right width=70% res=20. Required Tools 
:font facename=default size=0x0.
:p.
:ul compact.
:li.
:hp2.Compiler&per.:ehp2. With V0&per.80, I have switched to IBM VisualAge C++ 3&per.0
to develop XWorkplace&per.
I have not tried EMX/GCC because I'm too lazy to get used to it, even
though it might compile faster&per.
:p.For VAC++ 3&per.0, fixpak 8 is strongly recommended, because the original crashes
frequently with the complex SOM header files&per.
:p.I'm not perfectly sure whether I'm using IBM-specific language extensions&per.
I had to switch the language level to "extended" to use some none-POSIX
C stuff, and I'm not sure whether this is working with GCC&per. I have looked up some
functions in the EMX docs, such as :font facename='Courier' size=18x12._beginthread:font facename=default size=0x0., and these appear to
be the same, even though they're not POSIX C&per.
:p.If you get these sources compiled with a different compiler, please let me
know or supply patches&per.
.br
:li.
Whatever compiler you use, you need some :hp2.OS/2 Developer's Toolkit:ehp2. for
all the SOM header files&per.
:p.The Warp 3 toolkit is sufficient&per. XWorkplace does some tricks for Warp 4 too,
but does it even though I don't have the Warp 4 toolkit&per. :font facename='Courier' size=18x12.;-):font facename=default size=0x0.
The Toolkit is also needed for changing
:link reftype=hd res=4.NLS files,:elink. even though you won't
need a compiler for that&per.
.br
:li.
You need an "installation directory" of XWorkplace because the makefiles
copy all output files to that directory tree&per. This directory tree must be specified
using the :font facename='Courier' size=18x12.XWPRUNNING:font facename=default size=0x0. :link reftype=hd res=22.environment variable:elink.&per.
.br
:li.
All the &per.C files have lots of debugging code which can conditionally be
compiled using certain #define's in :font facename='Courier' size=18x12.include\setup&per.h:font facename=default size=0x0.&per. That file
is :font facename='Courier' size=18x12.#include:font facename=default size=0x0.'d from all XWorkplace source files&per.
:p.Only if you enable the debugging code,
you need the full :hp2.PMPRINTF package by Dennis Bareis&per.:ehp2.
You do :hp1.not:ehp1. need that package if you don't uncomment the debugging #define's
in that file&per.
:p.Per default, those #define's are commented out, so don't worry&per.
Check the :link reftype=hd res=28."Debugging":elink. section&per.
:eul.
.br
Also see the :link reftype=hd res=21."Making XWorkplace":elink. section for how to
rebuild XWorkplace&per.
.br
.* Source filename: src1_31make.html
:h2  x=right width=70% res=21. Making (Building) XWorkplace 
:font facename=default size=0x0.
:p.
With V0&per.9&per.0, the make process has been completely redesigned to allow
for easier extension of XWorkplace and for the Netlabs CVS server&per.
:p.If you wish to build the whole XWorkplace thing including the NLS DLLs
and the INF and HLP files, do the following&colon.
:ol compact.
:li.
Put the :font facename='Courier' size=18x12.&per.EXE:font facename=default size=0x0. files in :font facename='Courier' size=18x12./TOOLS:font facename=default size=0x0. directory
in a directory on your PATH&per. These tools are required by the makefiles&per.
.br
:li.
In :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. in the main directory, you'll have to set
a few variables&per. This is explained in that file&per.
.br
:li.
Edit :font facename='Courier' size=18x12.SETUP&per.IN:font facename=default size=0x0., if you have any wicked compiler setup
on your system&per. This file is included from the makefiles in the source
subdirectories&per. For most cases, the default setup should be OK&per.
Instead of changing :font facename='Courier' size=18x12.SETUP&per.IN:font facename=default size=0x0., I strongly recommend to use
:link reftype=hd res=22.environment variables:elink.&per.
.br
:li.
Call :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0.,
which will recurse into the directories and possibly take a long
time&per. This will build the main DLL, the NLS DLL, and all the INF and
HLP files&per. Alternatively, execute :font facename='Courier' size=18x12.nmake:font facename=default size=0x0. or :font facename='Courier' size=18x12.nmake really_all:font facename=default size=0x0.
from the command line&per. (See the top of the main :font facename='Courier' size=18x12.makefile:font facename=default size=0x0. for
possible targets&per.)
:p.A complete build (:font facename='Courier' size=18x12.nmake -a really_all:font facename=default size=0x0.)
currently (99-11-29) takes 240 seconds on my PII-400, 128 MB RAM&per.
:eol.
.br
:hp2.Debug code&per.:ehp2. Depending on whether the :font facename='Courier' size=18x12.XWPDEBUG:font facename=default size=0x0. environment
variable is set (in :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. or :font facename='Courier' size=18x12.SETUP&per.IN:font facename=default size=0x0. or somewhere else),
XWorkplace will be compiled with debug code or not&per. See the
:link reftype=hd res=22.next page:elink. for details&per.
:p.:hp2.Makefiles&per.:ehp2. Each directory now has its own makefile&per. All makefiles were
written for IBM NMAKE&per. I don't know if these will work with other make utilities&per.
:p.:hp2.Replacing in-use files&per.:ehp2. You can rebuild XWorkplace while the WPS is up
and XWorkplace is installed&per. The makefiles will automatically unlock DLLs which
are currently in use&per. After the rebuild, restart the WPS to make it use the newly
built DLLs&per.
The exception to this is XWPHOOK&per.DLL, which is not unlocked&per. This is intentional&per.
Unlocking a system hook DLL (which becomes part of every PM process on the system)
will _never_ release the DLL again, so you'd have to reboot to use the new DLL&per.
So if you get an error during the build process which says "XWPHOOK&per.DLL is in use",
kill :font facename='Courier' size=18x12.XWPDAEMON&per.EXE:font facename=default size=0x0., which will properly unload the hooks&per.
:p.:hp2.Rebuilding the whole thing&per.:ehp2. If you want to make sure that all C files
are :hp1.re:ehp1.compiled, use :font facename='Courier' size=18x12.NMAKE -A:font facename=default size=0x0. on the main makefile&per.
Alternatively, you can open and save :font facename='Courier' size=18x12.include/setup&per.h:font facename=default size=0x0. to make
it newer than the target files&per. This is
#include'd in all XWorkplace C files, and the makefiles' inference
rules will then enforce recompilation of :hp1.all:ehp1. C files&per.
.br
.* Source filename: src1_32env.html
:h2  x=right width=70% res=22. Makefile Environment Variables 
:font facename=default size=0x0.
:p.
This section describes the environment variables which should be set
externally before executing the makefiles&per.
:p.You can modify :font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. to set up these things, because
:font facename='Courier' size=18x12.MAKE&per.CMD:font facename=default size=0x0. calls the main :font facename='Courier' size=18x12.makefile:font facename=default size=0x0.&per.
:p.Alternatively, create your own "XWorkplace make" command file, which
sets up these variables, or define these variables in CONFIG&per.SYS&per.
:p.If these are not set, you'll either get errors from the makefiles
(certain checks are built into those), or :font facename='Courier' size=18x12.setup&per.in:font facename=default size=0x0. will
use some dumb default, which is most probably not matching your system&per.
:ol compact.
:li.
:font facename='Courier' size=18x12.PROJECT_BASE_DIR:font facename=default size=0x0. points to the base directory of
the XWorkplace source tree&per. This must be the parent directory of :font facename='Courier' size=18x12.src\:font facename=default size=0x0.,
:font facename='Courier' size=18x12.include\,:font facename=default size=0x0. etc&per. This is required for the makefiles to work, because
otherwise they'll never know where to recurse to&per.
.br
:li.
If :font facename='Courier' size=18x12.XWPDEBUG:font facename=default size=0x0. is defined (to anything), XWorkplace will
be compiled and linked with debug code enabled&per. Also, this will pass the
:font facename='Courier' size=18x12.__XWPDEBUG__:font facename=default size=0x0. define to the compiler, to which some code parts
react&per. For example, only if that flag is defined, PmPrintf calls will be
compiled&per. See :link reftype=hd res=28."Debugging XWorkplace":elink. for details&per.
.br
:li.
:font facename='Courier' size=18x12.XWPRUNNING:font facename=default size=0x0. must point to an exiting XFolder/XWorkplace
installation&per. Since the makefiles first create all the files in :font facename='Courier' size=18x12.bin\:font facename=default size=0x0.,
this is not suitable for testing your code, because XWorkplace expects a lot
of files at fixed locations relative to the directory where :font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0.
resides&per. Most makefiles will copy the target files to the directory pointed to
by this environment variable (after unlocking executables), so after rebuilding,
you can simply restart the WPS, and your new code will be running in the WPS&per.
:p.This must point to the parent directory of :font facename='Courier' size=18x12.bin\:font facename=default size=0x0., :font facename='Courier' size=18x12.help\:font facename=default size=0x0. etc&per.
in the XFolder/XWorkplace installation tree&per.
.br
:li.
:font facename='Courier' size=18x12.XWPRELEASE:font facename=default size=0x0. is only used with :font facename='Courier' size=18x12.nmake release:font facename=default size=0x0.&per.
This specifies the base directory where to build a complete XWorkplace release&per. If
this directory does not exist, it is created&per.
.br
:li.
:font facename='Courier' size=18x12.HELPERS_BASE:font facename=default size=0x0. is required&per. This must point to the WarpIN CVS source
tree, from which XWorkplace steals the :link reftype=hd res=26.helpers code:elink.&per.
This must point to the parent directory of :font facename='Courier' size=18x12.src\:font facename=default size=0x0. in the WarpIN tree&per.
:eol.
.br
.* Source filename: src1_33trouble.html
:h2  x=right width=70% res=23. Troubleshooting 
:font facename=default size=0x0.
:p.
:ol compact.
:li.
If you get errors from the :hp2.makefiles,:ehp2. make sure you have set up the
:link reftype=hd res=22.environment variables:elink. right&per.
.br
:li.
Also, you need to check out the WarpIN source tree for the
:link reftype=hd res=26.helpers:elink. code&per.
.br
:li.
If you get something like :hp2."File XWPHOOK&per.DLL is in use",:ehp2. the
XWorkplace hook DLL is currently loaded and running&per. While all other executables
are automatically unlocked and replaced by the makefiles even while they're running,
this cannot be done with the hook DLL, because then you'd only be able to reload
that DLL by rebooting (it will _never_ get released otherwise)&per.
:p.Use WatchCat or some other process killer to kill :font facename='Courier' size=18x12.XWPDAEMN&per.EXE:font facename=default size=0x0., which
will automatically unload the hook DLL&per. Then, after the build, restart the daemon
by executing :font facename='Courier' size=18x12.XWPDAEMN -D:font facename=default size=0x0. (to circumvent the error message box which
comes up otherwise)&per.
:p.Warning&colon. If you don't restart the daemon before restarting the WPS, the XWorkplace
startup folder will get processed again after the WPS restart&per.
.br
:li.
If you :hp2.cannot install XWorkplace:ehp2. after the build, you have probably built
the debug version, but the PMPRINTF DLLs are not on your LIBPATH&per. See
:link reftype=hd res=28."Debugging":elink. for details&per.
:eol.
.br
.* Source filename: src1_4docs.html
:h2  x=right width=70% res=24. Creating the Code Documentation 
:font facename=default size=0x0.
:p.
Starting with V0&per.9&per.0, you can have HTML documentation created for the whole
XWorkplace code&per. This done using my own :font facename='Courier' size=18x12.xdoc:font facename=default size=0x0. utility, which
can be found in the :font facename='Courier' size=18x12.TOOLS\:font facename=default size=0x0. directory after XWorkplace has
been built for the first time&per.
:p.:font facename='Courier' size=18x12.xdoc:font facename=default size=0x0. is capable of parsing C/CPP source code and header files
to some extent&per. When certain codes are found in a comment in the sources
(e&per.g&per. :font facename='Courier' size=18x12.@@:font facename=default size=0x0.), that comment is assumed to document some function
and will be considered for HTML output&per.
:p.After XWorkplace has been built for the first time, call :font facename='Courier' size=18x12.createdoc&per.cmd:font facename=default size=0x0.
in the main directory, which will create the HTML documentation in a new
subdirectory of the code tree :font facename='Courier' size=18x12.HTML\:font facename=default size=0x0.&per. Open
:font facename='Courier' size=18x12.index&per.html:font facename=default size=0x0. then to get an index of all XWorkplace source files&per.
.br
.* Source filename: src1_4maindll.html
:h2  x=right width=70% res=25. The SRC\CLASSES\ directory 
:font facename=default size=0x0.
:p.
:font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0. contains only C code which was originally generated
by the SOM compiler from the IDL files in the :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. directory&per. Of
course, that code has been extended so that the new classes do anything meaningful&per.
:p.As opposed to versions before 0&per.9&per.0, this directory contains SOM code :hp1.only&per.:ehp1.
The idea is that (except for fairly small method code) code in one of these class
files should call some implementation function in a directory other than
:font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0.&per. Only in doing so it is possible that several developers
can implement features within the same WPS class&per.
:p.For example, :font facename='Courier' size=18x12.src\classes\xfldr&per.c:font facename=default size=0x0. branches over to
:font facename='Courier' size=18x12.src\filesys\folder&per.c:font facename=default size=0x0., which is the "folder implementation" code and has
most of the folder features&per.
:p.
The XWorkplace package contains many classes, but puts them all into a single DLL&per.
This is possible by specifying the same target DLL in the IDL files&per.
(XWorkplace's IDL files are now in the separate :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. directory&per.)
One can put several classes into one &per.IDL file (which happens in
:font facename='Courier' size=18x12.xtrash&per.idl:font facename=default size=0x0. for example) or spread them across several IDL files too&per.
:p.Note that the &per.DEF files in :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. are not used&per.
These files are created automatically by the
SOM compiler, which cannot however create a single DEF file if you
use several IDL files for the same DLL&per. For finally linking the whole main DLL,
the makefiles instead use :font facename='Courier' size=18x12.\src\shared\xwp&per.def:font facename=default size=0x0., which I manually
created from all the DEF files which were created by the SOM compiler&per.
This file exports lots of mysterious SOM structures which allow the SOM kernel see the
classes in the DLL&per.
.br
.* Source filename: src1_5helpers.html
:h2  x=right width=70% res=26. The SRC\HELPERS\ directory 
:font facename=default size=0x0.
:p.
:font facename='Courier' size=18x12.SRC\HELPERS:font facename=default size=0x0. contains lots of useful code which I have developed
over time&per. This code is :hp2.independent of XWorkplace:ehp2. and could be used
with any OS/2 program&per. Some of the code is for PM programs, some can be used
with text-mode programs also&per. Consider this a general OS/2 programming
library&per.
:p.:hp2.Note&colon.:ehp2. XWorkplace's source code does not contain the helpers source code&per.
Instead, during the build process, the XWorkplace makefiles branch over to the
WarpIN source tree, which is also on the Netlabs CVS server&per. You will need to
set the :font facename='Courier' size=18x12.HELPERS_BASE:font facename=default size=0x0. :link reftype=hd res=22.environment variable:elink.
to point to the WarpIN source tree for this to work&per.
:p. In detail, we have&colon.
:ul compact.
:li.
:font facename='Courier' size=18x12.animate&per.*:font facename=default size=0x0. contains some animation code&per.
:li.
:font facename='Courier' size=18x12.cnrh&per.*:font facename=default size=0x0. Container helper functions (new with V0&per.9&per.0,
partly moved from winh&per.*)&per.
:li.
:font facename='Courier' size=18x12.comctl&per.*:font facename=default size=0x0. Various window procedures&per.
:li.
:font facename='Courier' size=18x12.debug&per.*:font facename=default size=0x0. parses executables and SYM files for debugging
information; used by except&per.c (below)&per. Introduced with V0&per.84,
moved to :font facename='Courier' size=18x12.SRC\HELPERS\:font facename=default size=0x0. with V0&per.9&per.0&per.
:li.
:font facename='Courier' size=18x12.dosh&per.*:font facename=default size=0x0. Control Program helper functions&per.
:li.
:font facename='Courier' size=18x12.eas&per.*:font facename=default size=0x0. contains helper functions to handle
extended attributes&per.
.br
:li.
:font facename='Courier' size=18x12.except&per.*:font facename=default size=0x0. contains generic code for implementing powerful
exception handlers&per. This was introduced with V0&per.84 and has been moved
to :font facename='Courier' size=18x12.SRC\HELPERS\:font facename=default size=0x0. with V0&per.9&per.0&per. The code has been straightened
out to be independent of XWorkplace with the use of exception "plug-ins"&per.
See :link reftype=hd res=29."XWorkplace exception handling":elink. for details&per.
:li.
:font facename='Courier' size=18x12.gpih&per.*:font facename=default size=0x0. GPI (graphics) helper functions&per.
:li.
:font facename='Courier' size=18x12.linklist&per.*:font facename=default size=0x0. contains helper functions to handle linked lists&per.
:li.
:font facename='Courier' size=18x12.prfh&per.*:font facename=default size=0x0. is new with V0&per.82 and contains Profile (INI)
helper functions&per.
:li.
:font facename='Courier' size=18x12.procstat&per.*:font facename=default size=0x0. modified Kai Uwe Rommel's DosQProc() functions&per.
:li.
:font facename='Courier' size=18x12.shapewin&per.*:font facename=default size=0x0. contains the ShapeWindows library&per. See the top of
shapewin&per.c for details (new with V0&per.85)&per.
:li.
:font facename='Courier' size=18x12.stringh&per.*:font facename=default size=0x0. contains helper functions to handle strings/texts
:li.
:font facename='Courier' size=18x12.syssound&per.*:font facename=default size=0x0. has code for managing system sound data and
sound schemes&per.
:li.
:font facename='Courier' size=18x12.threads&per.*:font facename=default size=0x0. contains helper functions to synchronize threads&per.
:li.
:font facename='Courier' size=18x12.tmsgfile&per.*:font facename=default size=0x0. has Christian Langanke's new &per.TMF
(text message file) handling to get rid of those ugly &per.MSG files&per.
:li.
:font facename='Courier' size=18x12.winh&per.*:font facename=default size=0x0. Win (PM) helper functions&per.
:li.
:font facename='Courier' size=18x12.wphandle&per.*:font facename=default size=0x0. Henk Kelder's WPS handles functions&per.
:eul.
.br
.* Source filename: src1_801prefixes.html
:h2  x=right width=70% res=27. XWorkplace Function Prefixes 
:font facename=default size=0x0.
:p.
XWorkplace uses function prefixes to indicate in which source file
a certain function resides&per. XWorkplace function prefixes are in
:hp2.lower case:ehp2. to distinguish them from the OS/2 API function
prefixes&per. So if you encounter some lower-case function prefix,
you can be pretty sure it's some function in :font facename='Courier' size=18x12.MAIN:font facename=default size=0x0. or :font facename='Courier' size=18x12.HELPERS:font facename=default size=0x0.
and has been prototyped in the corresponding header file&per.
:p.
If a function has no prefix, it's probably in the same source file and
not prototyped in the headers&per.
:p.
However, there are two general function prefixes which are used to indicate
certain function :hp1.types:ehp1.&colon.
:ul compact.
:li.
:font facename='Courier' size=18x12.fnwp*:font facename=default size=0x0. is my general prefix
for window procedures (the :font facename='Courier' size=18x12.MRESULT EXPENTRY&per.&per.&per.:font facename=default size=0x0. type), which
may or may not be exported&per.
.br
:li.
:font facename='Courier' size=18x12.fncb:font facename=default size=0x0. is my general prototype for callback functions&per. Most of
these are callbacks for the notebook helpers in :font facename='Courier' size=18x12.shared\notebook&per.c:font facename=default size=0x0.,
but there are some for functions in :font facename='Courier' size=18x12.HELPERS:font facename=default size=0x0. also&per. See the
respective function header then&per.
:eul.
.br
Here are the other function headers and the files where their code resides&colon.
:ul compact.
:li.
:font facename='Courier' size=18x12.anm*     :font facename=default size=0x0.helpers\animate&per.*
:li.
:font facename='Courier' size=18x12.apm*     :font facename=default size=0x0.startshut\apm&per.*
:li.
:font facename='Courier' size=18x12.arc*     :font facename=default size=0x0.startshut\archives&per.*
:li.
:font facename='Courier' size=18x12.cfg*     :font facename=default size=0x0.config\cfgsys&per.*
:li.
:font facename='Courier' size=18x12.cls*     :font facename=default size=0x0.config\classlst&per.*
:li.
:font facename='Courier' size=18x12.cmn*     :font facename=default size=0x0.shared\common&per.*
:li.
:font facename='Courier' size=18x12.cnrh*    :font facename=default size=0x0.helpers\cnrh&per.*
:li.
:font facename='Courier' size=18x12.ctl*     :font facename=default size=0x0.helpers\comctl&per.*
:li.
:font facename='Courier' size=18x12.dtp*     :font facename=default size=0x0.filesys\desktop&per.*
:li.
:font facename='Courier' size=18x12.dosh*    :font facename=default size=0x0.helpers\dosh&per.*
:li.
:font facename='Courier' size=18x12.dsk*     :font facename=default size=0x0.filesys\disk&per.*
:li.
:font facename='Courier' size=18x12.ea*      :font facename=default size=0x0.helpers\eas&per.*
:li.
:font facename='Courier' size=18x12.exc*     :font facename=default size=0x0.helpers\except&per.*
:li.
:font facename='Courier' size=18x12.fdr*     :font facename=default size=0x0.filesys\folder&per.* or filesys\fdrhoty&per.*
:li.
:font facename='Courier' size=18x12.fops*    :font facename=default size=0x0.filesys\fileops&per.*
:li.
:font facename='Courier' size=18x12.fsys*    :font facename=default size=0x0.filesys\filesys&per.*
:li.
:font facename='Courier' size=18x12.ftyp*    :font facename=default size=0x0.filesys\filetype&per.*
:li.
:font facename='Courier' size=18x12.gpih*    :font facename=default size=0x0.helpers\gpih&per.*
:li.
:font facename='Courier' size=18x12.hif*     :font facename=default size=0x0.config\hookintf&per.*
:li.
:font facename='Courier' size=18x12.krn*     :font facename=default size=0x0.shared\kernel&per.*
:li.
:font facename='Courier' size=18x12.lst*     :font facename=default size=0x0.helpers\linklist&per.*
:li.
:font facename='Courier' size=18x12.mnu*     :font facename=default size=0x0.filesys\menus&per.*
:li.
:font facename='Courier' size=18x12.ntb*     :font facename=default size=0x0.shared\notebook&per.*
:li.
:font facename='Courier' size=18x12.obj*     :font facename=default size=0x0.filesys\object&per.*
:li.
:font facename='Courier' size=18x12.prfh*    :font facename=default size=0x0.helpers\prfh&per.*
:li.
:font facename='Courier' size=18x12.prc*     :font facename=default size=0x0.helpers\procstat&per.*
:li.
:font facename='Courier' size=18x12.shp*     :font facename=default size=0x0.helpers\shapewin&per.*
:li.
:font facename='Courier' size=18x12.snd*     :font facename=default size=0x0.helpers\syssound&per.* or filesys\sound&per.*
:li.
:font facename='Courier' size=18x12.stb*     :font facename=default size=0x0.filesys\statbars&per.*
:li.
:font facename='Courier' size=18x12.strh*    :font facename=default size=0x0.helpers\stringh&per.*
:li.
:font facename='Courier' size=18x12.thr*     :font facename=default size=0x0.helpers\threads&per.*
:li.
:font facename='Courier' size=18x12.tmf*     :font facename=default size=0x0.helpers\tmsgfile&per.*
:li.
:font facename='Courier' size=18x12.wph*     :font facename=default size=0x0.helpers\wphandle&per.*
:li.
:font facename='Courier' size=18x12.wpsh*    :font facename=default size=0x0.shared\wpsh&per.*
:li.
:font facename='Courier' size=18x12.xsd*     :font facename=default size=0x0.startshut\shutdown&per.*
:li.
:font facename='Courier' size=18x12.xthr*    :font facename=default size=0x0.filesys\xthreads&per.*
.br
:li.
:font facename='Courier' size=18x12.xwp* :font facename=default size=0x0.newly introduced SOM instance methods in SRC\CLASSES
:li.
:font facename='Courier' size=18x12.xfcls*, xwpcls* :font facename=default size=0x0.newly introduced SOM class methods in SRC\CLASSES
:eul.
.br
.* Source filename: src1_880debug.html
:h2  x=right width=70% res=28. Debugging XWorkplace 
:font facename=default size=0x0.
:p.
Debugging WPS applications can be really tiresome, because you have to restart
the WPS for every tiny change you made to the source codes to take effect&per. And,
as with any PM program, you can't just :font facename='Courier' size=18x12.printf():font facename=default size=0x0. stuff to the screen&per.
Even worse, it's hard to use the PM debugger, because you have to start the whole
WPS (:font facename='Courier' size=18x12.PMSHELL&per.EXE:font facename=default size=0x0.) with it, since :font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0. is no standalone
application&per.
:p.So I had to look for something else&per.
:p.Those :font facename='Courier' size=18x12._Pmpf(("xxx")):font facename=default size=0x0. thingies are for the
magnificent PMPRINTF package by Dennis Bareis&per.
These only display anything if the proper :font facename='Courier' size=18x12.DEBUG_xxx:font facename=default size=0x0. #define's are set in
:font facename='Courier' size=18x12.include\setup&per.h:font facename=default size=0x0. (changed with V0&per.9&per.0)&per.
For the release version of XWorkplace, all these flags
have been disabled, so no additional code is produced at all&per. You
thus don't have to remove the commands to speed up XWorkplace, because this
wouldn't make any difference&per.
:p.Some files from the PMPRINTF package are included so that you can
compile&per. The PM interface which actually displays the messages is not
however&per. Check Dennis Bareis' homepage at
:link reftype=hd res=30.http&colon.//www&per.ozemail&per.com&per.au/~dbareis/:elink. where you'll find tons of other
useful utilities too&per.
:p.However, to really use PMPRINTF, you'll have to put some DLLs
on your LIBPATH&per. See the PMPRINTF docs for details&per. I strongly
recommend using this utility&per.
:p.:hp2.IMPORTANT NOTE&colon.:ehp2. If you #define any of the flags at the top of
:font facename='Courier' size=18x12.include\setup&per.h:font facename=default size=0x0., you :hp1.must:ehp1. have the PMPRINTF DLLs somewhere on your
LIBPATH, or otherwise you'll spend days figuring out why XWorkplace is
simply not working any more&per. (I had this recently after a reinstall
of OS/2, after which the DLLs where gone&per.) That is, XWorkplace classes
will not load at WPS startup, because the PMPRINTF imports cannot be
resolved&per. Neither will registering XWorkplace classes succeed&per. And don't
expect to get an error message other than FALSE&per. :font facename='Courier' size=18x12.&colon.-(:font facename=default size=0x0.
:p.:font facename='Courier' size=18x12._Pmpf(("xxx")):font facename=default size=0x0. uses regular printf syntax, except for those
strange double brackets, which are needed because macros don't accept
variable parameter lists otherwise&per.
.br
.* Source filename: 051source2.html
:h1  group=1 width=30% res=6.         Source Code Details     
:font facename=default size=0x0.
:p.
:link reftype=hd res=19 auto dependent.
:ul compact.
:li.
:link reftype=hd res=31.Some SOM tricks:elink.
:li.
:link reftype=hd res=32.XWorkplace settings:elink.
:li.
:link reftype=hd res=33.Folder window subclassing:elink.
:li.
:link reftype=hd res=34.Menu manipulation:elink.
:li.
:link reftype=hd res=35.Extended sort functions:elink.
:li.
:link reftype=hd res=36.XWorkplace threads:elink.
:li.
:link reftype=hd res=37.Playing Sounds:elink.
:li.
:link reftype=hd res=38.XShutdown:elink.
:li.
:link reftype=hd res=29.XWorkplace Exception Handling:elink.
:eul.
.br
.* Source filename: src2_803som.html
:h2  x=right width=70% res=31. Some SOM Tricks 
:font facename=default size=0x0.
:p.
In general, XWorkplace is pretty straightforward SOM/WPS code&per. It
overrides a lot of :font facename='Courier' size=18x12.wp*:font facename=default size=0x0. and :font facename='Courier' size=18x12.wpcls*:font facename=default size=0x0. methods&per.
:p.In order to avoid too much
confusion, the methods which XWorkplace :hp1.adds:ehp1. do not carry
the usual :font facename='Courier' size=18x12.wp*:font facename=default size=0x0. and
:font facename='Courier' size=18x12.wpcls*:font facename=default size=0x0. prefixes, but
:font facename='Courier' size=18x12.xwp*:font facename=default size=0x0. and :font facename='Courier' size=18x12.xwpcls*:font facename=default size=0x0. instead&per.
Only "real" XWorkplace SOM methods have this prefix; "normal" functions have other
prefixes (see the :link reftype=hd res=27.previous page:elink. for a list)&per.
:p.As XWorkplace became more complex over time, I have delved quite deeply
into SOM&per. While I still don't fully understand what is going on in
the SOM kernel (from what I hear, I guess nobody does) and some bugs
in there keep puzzling me, I've found out some interesting stuff&per.
:p.:hp2.Note&colon.:ehp2. The following is not required to write WPS classes&per. This
is additional information for those who have written WPS classes already
and might be interested in some SOM internals&per.
:p.
Most of this is related to the :hp2."WPS Classes":ehp2. object
(:font facename='Courier' size=18x12.xclslist&per.c:font facename=default size=0x0.)&per.
The SOM logic for this is in
:font facename='Courier' size=18x12.classlst&per.c:font facename=default size=0x0. in :font facename='Courier' size=18x12.clsWPSClasses2Cnr:font facename=default size=0x0.,
which can analyze the current SOM runtime environment (which is that
of the WPS), i&per.e&per. all the classes that have been loaded and query their
inheritance hierarchy at runtime&per.
:p.This inserts the WPS class tree as
recordcores into a given cnr control&per. This works with any containers
in tree views, so that some XWorkplace dialogs can use this too&per. Check
the sources, there's some interesting stuff&per.
:p.
Note that there are quite a number of functions in XWorkplace which take
SOM (WPS) objects as a parameter, even though they are not :hp2.SOM methods&per.:ehp2.
See :font facename='Courier' size=18x12.src\shared\wpsh&per.c:font facename=default size=0x0. for examples&per. The reason for this is that
resolving SOM methods takes quite a bit of time, and calling regular
functions will work just as well, but faster&per.
:p.
If you look into the :font facename='Courier' size=18x12.&per.IH:font facename=default size=0x0. header files created by the SOM
compiler, you see that the C bindings for method calls are really all
macros #define'd in the header files which translate into more complex
C code&per.
:p.Here's an example&colon. if you call
:font facename='Courier' size=18x12._wpQueryStyle(somSelf):font facename=default size=0x0. in :font facename='Courier' size=18x12.xfldr&per.c:font facename=default size=0x0., where this
method has not been overridden, the WPObject version of this method should
get called&per. Here's the #define in :font facename='Courier' size=18x12.xfldr&per.ih:font facename=default size=0x0. for this&colon.
:p.:font facename='Courier' size=18x12.#define XFolder_wpQueryStyle WPObject_wpQueryStyle:font facename=default size=0x0.
:p.And :font facename='Courier' size=18x12.WPObject_wpQueryStyle:font facename=default size=0x0. is #define'd in :font facename='Courier' size=18x12.wpobject&per.h:font facename=default size=0x0. from
the toolkit headers as follows&colon.
:cgraphic.
#define WPObject_wpQueryStyle(somSelf) \
    (SOM_Resolve(somSelf, WPObject, wpQueryStyle) \
    (somSelf))
:ecgraphic.
:p.
Actually, there are more macros related to this, but this is the important one&per.
:font facename='Courier' size=18x12.SOM_Resolve:font facename=default size=0x0. in turn is a macro #define'd in :font facename='Courier' size=18x12.somcdev&per.h:font facename=default size=0x0.,
which calls the SOM kernel function :font facename='Courier' size=18x12.somResolve:font facename=default size=0x0.&per. That function
finally goes through the class method tables to find the actual function
address and call it&per.
:p.
As as result, not only does compiling of SOM code take ages (because of all the
nested macros), but also :hp1.calling:ehp1. SOM methods does, because as opposed
to "static" OO languages such as C++, method resolution is occuring at
run-time&per. IBM says in the SOM docs that
:hp2.calling a SOM method takes at least three times as long as calling a normal C
function&per.:ehp2.
:p.
Since there is no real need to write functions as SOM methods, except when you
want to design a method which can be overriden in subclasses,
I have only done so
in rare occasions to speed up processing&per.
:p.
Here are some :hp2.other SOM tricks and functions:ehp2. which are not mentioned directly
in the WPS reference (but only in the chaotic SOM docs), but these are
very useful&per. Some of this is pretty basic, some might be new to WPS programmers,
so I'll list this here&colon.
:ul compact.
:li.
Since all the SOM stuff is declared in those huge header files, you
need to :hp2.#include a header file:ehp2. if you need access to certain class-specific
features&per.
:p.For example, if you write a subclass of WPDataFile and need some
program-object method call (e&per.g&per. :font facename='Courier' size=18x12._wpQueryProgDetails:font facename=default size=0x0.), you need
to put :font facename='Courier' size=18x12.#include <wppgm&per.h>:font facename=default size=0x0. on top of your code, or the
method binding will not be found&per.
:p.This is not neccessary if the method is defined for a superclass of your
class, because the SOM headers automatically #include all the parent classes&per.
That is, for example,
you don't need to #include :font facename='Courier' size=18x12.wpfsys&per.h:font facename=default size=0x0. (for WPFileSystem)
for your WPDataFile subclass&per.
.br
:li.
The most important thing to keep in mind is that all :hp2.SOM classes are
objects:ehp2. too&per. They are instances of their respective metaclasses&per. This takes some
getting used to, but it is this concept only which allows WPS classes to
be created at runtime and for class replacements in the first place&per.
:p.That is, for example, any WPS object is an instance of WPObject (really
one of its subclasses)&per. The WPObject class in turn is an instance of
its metaclass, M_WPObject&per.
:p.In SOM, the default metaclass for a class is SOMClass&per. However, the WPS
overrides this behavior to create a unique metaclass for each WPS class,
which is prefixed with :font facename='Courier' size=18x12.M_:font facename=default size=0x0.&per. Since the metaclass is a class as well,
it has methods too (the so-called "class methods", which operate on class
objects, as opposed to the "instance methods" of the class itself, which operate
on instances of the class -- the WPS objects)&per.
.br
:li.
To :hp2.access a class object,:ehp2. for any existing class you always have a
corresponding macro which is
an underscore plus the class name&per. That is, the class object of WPObject
can always be obtained with :font facename='Courier' size=18x12._WPObject:font facename=default size=0x0.&per. This is useful for calling
class methods, which always need to be invoked on a class object&per. So, to call
a folder class method, you do something like
:font facename='Courier' size=18x12._wpclsQueryOpenFolders(_WPFolder):font facename=default size=0x0.&per.
:p.
Note&colon. If a class has been replaced, the macro will not return the original, but
the replacement class&per. That is, if XWorkplace is installed, the above example
would actually return the XFldObject class object (see notes below), and methods
invoked on the WPObject class object would actually be resolved for XFldObject&per.
That's how class replacements work in the first place&per. See the notes below for more&per.
.br
:li.
:font facename='Courier' size=18x12.BOOL _somIsA(pObject, pClassObject):font facename=default size=0x0. checks for whether
pObject is an :hp2.instance of the class:ehp2. specified by pClassObject or one of
its subclasses&per.
This is extensively used in :font facename='Courier' size=18x12.statbars&per.c:font facename=default size=0x0. to
adjust status bar display depending on the class of a single object which
is currently selected&per.
:p.Example&colon. :font facename='Courier' size=18x12._somIsA(somSelf, _WPObject):font facename=default size=0x0. should always be true within
the WPS context, since all WPS classes are subclasses of WPObject&per.
By contrast, :font facename='Courier' size=18x12._somIsA(somSelf, _WPFolder):font facename=default size=0x0. would only return TRUE if somSelf
is a WPS folder&per. (This would work with :font facename='Courier' size=18x12._XFolder:font facename=default size=0x0. too, but this would
require that you have access to the XFolder header files and that XFolder replaces
WPFolder&per. :font facename='Courier' size=18x12._WPFolder:font facename=default size=0x0. always works, even if WPFolder is replaced,
because :font facename='Courier' size=18x12._WPFolder:font facename=default size=0x0. would be resolved to point to the replacement class object
then -- e&per.g&per. :font facename='Courier' size=18x12._XFolder:font facename=default size=0x0.&per.)
.br
:li.
:font facename='Courier' size=18x12.PSZ _somGetName(pClassObject):font facename=default size=0x0. returns the
:hp2.class name:ehp2. specified by pClassObject, as specified in the IDL file&per.
(That is different from :font facename='Courier' size=18x12.wpclsQueryTitle:font facename=default size=0x0.&per.)
:p.Example&colon. :font facename='Courier' size=18x12._somGetName(_XFldObject):font facename=default size=0x0. will return "XFldObject"&per.
.br
:li.
:font facename='Courier' size=18x12.SOMClass* _somGetParent(pClassObject):font facename=default size=0x0. returns a
:hp2.parent class&per.:ehp2.
:p.Examples&colon. :font facename='Courier' size=18x12._somGetParent(_WPProgram):font facename=default size=0x0. returns the WPAbstract
class object (:font facename='Courier' size=18x12._WPAbstract:font facename=default size=0x0.)&per.
:font facename='Courier' size=18x12._somGetParent(_XFolder):font facename=default size=0x0. should return :font facename='Courier' size=18x12._WPFolder:font facename=default size=0x0.,
unless there's some other WPFolder replacement class above XWorkplace in the
replacement list&per.
.br
:li.
:font facename='Courier' size=18x12.BOOL _somDescendedFrom(pClass, pClassParent):font facename=default size=0x0. returns TRUE
if pClass is :hp2.descended:ehp2. from pClassParent&per.
:p.Examples&colon. :font facename='Courier' size=18x12._somDescendedFrom(_WPFolder, _WPObject):font facename=default size=0x0. should return
TRUE&per. :font facename='Courier' size=18x12._somDescendedFrom(_WPFolder, _WPProgram):font facename=default size=0x0. would return FALSE&per.
.br
:li.
:font facename='Courier' size=18x12.somResolveByName(pClassObject, "method"):font facename=default size=0x0. gives you
a :hp2.function pointer:ehp2. as implemented by the specified class&per. You should assign
this to a function pointer variable which matches the function prototype,
or otherwise you'll get crashes&per. This can be useful if you invoke a SOM method
frequently, for example in a loop on many objects, because then SOM method resolution
has to be done only once&per. If you used the macros, resolution would take place
for each iteration in the loop&per.
.br
:li.
:hp2.Class replacements&per.:ehp2. Nowhere is really documented how class
replacements actually work, except that replacement classes must be direct
descendants of the class which is to be replaced&per. I assume that class
replacements are a feature of the SOM class manager, of which the WPS
class manager is a descendant (WPClassManager is half documented in the
WPS Reference&per.) At least there is a documented SOMClassMgr
method, :font facename='Courier' size=18x12.somSubstituteClass:font facename=default size=0x0., which appears to be doing exactly
this job&per.
:p.From what I've seen, class replacements seem to work this way&colon. Any time a class object
is queried, the class manager does not return the class object of the
specified class, but the object of the replacement class instead&per. As a result, all
the method calls are not resolved for the original class, but for the replacement
class instead&per.
:p.Examples&colon. If XFolder replaces WPFolder,
:font facename='Courier' size=18x12.wpModifyPopupMenu:font facename=default size=0x0. is not called for WPFolder,
but for XFolder instead, even though the WPS had originally called it for a folder object&per.
By contrast, for :font facename='Courier' size=18x12.wpQueryIcon:font facename=default size=0x0., which is not overridden by XFolder, method
resolution leads to the method as implemented by the parent class, which should
be WPFolder, unless some other class replacements is present&per.
:p.The class replacement mechanism
is most obvious with the class object macros described above&colon. if
you have a :font facename='Courier' size=18x12._WPFolder:font facename=default size=0x0. in your code, this returns the class object
of XFolder, i&per.e&per. :font facename='Courier' size=18x12._WPFolder == _XWorkplace:font facename=default size=0x0.&per. So if you absolutely need
the WPFolder class object (which is normally not necessary, since this would
circumvent XFolder's functionality), you use :font facename='Courier' size=18x12._WPFolder:font facename=default size=0x0. (which
returns the XFolder class object) and then
climb up the class object's inheritance tree using
:font facename='Courier' size=18x12._somGetParent:font facename=default size=0x0. until :font facename='Courier' size=18x12._somGetName:font facename=default size=0x0. returns "WPFolder"&per.
.br
:li.
To get the class object of any class without having access to its
header files, do the following&colon.
:cgraphic.
somId    somidThis = somIdFromString("WPObject");
SOMClass *pClassObject = _somFindClass(SOMClassMgrObject, somidThis, 0, 0);
:ecgraphic.
:p.
Note again that :font facename='Courier' size=18x12._somFindClass:font facename=default size=0x0. will return replacement classes
instead of the originals&per. (That's how the class object macros work, BTW&per.)
.br
:li.
:font facename='Courier' size=18x12.__get_somRegisteredClasses(SOMClassMgrObject):font facename=default size=0x0. returns a
:font facename='Courier' size=18x12.SOMClassSequence:font facename=default size=0x0. of all currently registered class objects&per. This
is used by the "WPS classes" page in the "Workplace Shell" object&per. See
:font facename='Courier' size=18x12.classlst&per.c:font facename=default size=0x0. for details&per.
:eul.
.br
.* Source filename: src2_806settings.html
:h2  x=right width=70% res=32. XWorkplace Settings 
:font facename=default size=0x0.
:p.
XWorkplace uses two kinds of settings&colon. global and instance settings&per.
:p.
The :hp2.global settings:ehp2. are set both in the "Workplace Shell" object and
on the "XDesktop" notebook page and are stored in the "XWorkplace" app
in :font facename='Courier' size=18x12.OS2&per.INI:font facename=default size=0x0.&per. All the global notebook logic is in :font facename='Courier' size=18x12.xfsys&per.c:font facename=default size=0x0.&per.
:p.
XWorkplace's code accesses these settings via a large structure called
:font facename='Courier' size=18x12.GLOBALSETTINGS:font facename=default size=0x0. (suprise!) defined in :font facename='Courier' size=18x12.common&per.h:font facename=default size=0x0., to which a pointer be
obtained using :font facename='Courier' size=18x12.cmnQueryGlobalSettings:font facename=default size=0x0. (:font facename='Courier' size=18x12.common&per.c:font facename=default size=0x0.)&per.
:p.
By contrast, the :hp2.individual object settings:ehp2. are stored in
instance data, which is declared in the :font facename='Courier' size=18x12.&per.IDL:font facename=default size=0x0. files&per. Most of these are
stored and retrieved using the normal WPS mechanism
(:font facename='Courier' size=18x12.wpSaveDeferred:font facename=default size=0x0./:font facename='Courier' size=18x12.wpRestoreData:font facename=default size=0x0.) and can have
a certain "transparent"
value, which means that the global setting is to be used instead&per.
.br
.* Source filename: src2_808subclass.html
:h2  x=right width=70% res=33. Subclassing Folder Windows 
:font facename=default size=0x0.
:p.
XWorkplace subclasses all WPFolder frame windows to intercept a large
number of messages&per. This is needed for the majority of XWorkplace's features,
which are not directly SOM/WPS-related, but are rather straight PM programming&per.
:p.Most of the standard WPS methods are really encapsulations of PM messages&per.
When the WPS opens a folder window, it creates a container control
as a child of the folder frame window, which is subclassed to get all the
container WM_CONTROL messages&per. Then, for example, if the user opens a context
menu on an object in the container, the frame gets WM_CONTROL with CN_CONTEXTMENU&per.
The WPS then finds the WPS (SOM) object which corresponds to the container record core
on which the context menu was opened and invokes the WPS menu methods on it,
that is, :font facename='Courier' size=18x12.wpFilterPopupMenu:font facename=default size=0x0. and :font facename='Courier' size=18x12.wpModifyPopupMenu:font facename=default size=0x0.&per.
Similar things happen with WM_COMMAND and :font facename='Courier' size=18x12.wpMenuItemSelected:font facename=default size=0x0.&per.
:p.The trick is just how to get "past" the WPS, which does a lot of hidden
stuff in its method code&per. By subclassing the folder frames ourselves, we get all
the messages which are being sent or posted to the folder
:hp1.before:ehp1. the WPS gets them and can thus
modify the WPS's behavior even if no methods have been defined for a certain
event&per. An example of this is that the contents of an XFolder status bar changes
when object selections have changed in the folder&colon. this reacts to WM_CONTROL
with CN_EMPHASIS&per.
:p.Subclassing is done in :font facename='Courier' size=18x12.XFolder&colon.&colon.wpOpen:font facename=default size=0x0. after
having called the parent method (WPFolder's), which returns the handle of the
new folder frame which the WPS has created&per.
:p.
XFolder's subclassed frame window proc is called :font facename='Courier' size=18x12.fnwpSubclassedFolderFrame:font facename=default size=0x0.
(in :font facename='Courier' size=18x12.src\filesys\folder&per.c:font facename=default size=0x0.)&per. Take a look at it, it's one of the most
interesting parts of XWorkplace&per. It handles status bars (which are frame
controls), tree view auto-rolling, special menu features, the "folder
content" menus and more&per.
:p.This gives us the following hierarchy of window procedures&colon.
:ol compact.
:li.
our :font facename='Courier' size=18x12.fnwpSubclassedFolderFrame:font facename=default size=0x0., first, followed by
:li.
the WPS folder frame window subclass, followed by
:li.
WC_FRAME's default frame window procedure, followed by
:li.
:font facename='Courier' size=18x12.WinDefWindowProc:font facename=default size=0x0. last&per.
:eol.
.br
If additional WPS enhancers are installed (e&per.g&per. Object Desktop), they will
appear at the top of the chain also&per. I guess it depends on the hierarchy of replacement
classes in the WPS class list which one sits on top&per.
:p.
In order to be able to relate the original frame window proc to a
folder frame (which can be different for each WPFolder window),
XWorkplace maintains a global linked list of :font facename='Courier' size=18x12.PSUBCLASSEDLISTITEM:font facename=default size=0x0.
structures (declared in :font facename='Courier' size=18x12.include\filesys\folder&per.h:font facename=default size=0x0.)&per. This might be a bit slow, but I
have found no other way to store the original window proc&per. Using
window words does not work (believe me&colon. it :hp1.does:ehp1. crash the WPS),
because either the WPFolder frame window class does not have window words
or the window words seem to be used by the WPS already, and (of
course) this is not documented&per. If you have a better idea for this,
let me know&per.
:p.As with all linked lists, XWorkplace uses the :font facename='Courier' size=18x12.lst*:font facename=default size=0x0. functions in
:font facename='Courier' size=18x12.src\helpers\linklist&per.c:font facename=default size=0x0. for adding/removing/searching etc&per. list items&per.
.br
.* Source filename: src2_810menus.html
:h2  x=right width=70% res=34. Menu Manipulation 
:font facename=default size=0x0.
:p.
This is perhaps the most complex part of XWorkplace&per. After all, this is
what I've started with, so some code parts may well be unchanged
since December '97, when I neither knew much about C nor about WPS
programming&per.
:p.
With V0&per.81, all the logic for manipulating and evaluating context
menus has been moved to a new file, :font facename='Courier' size=18x12.menus&per.c:font facename=default size=0x0.&per. All these functions have
been given the :font facename='Courier' size=18x12.mnu*:font facename=default size=0x0. prefix to make this a bit clearer&per.
:p.
The XWorkplace classes override :font facename='Courier' size=18x12.wpModifyPopupMenu:font facename=default size=0x0. and
:font facename='Courier' size=18x12.wpMenuItem[Help]Selected:font facename=default size=0x0. for almost every replacement class&per. These
overrides either handle the new context menu items in that method
code or, for XFolder and XFldDisk, call the functions in :font facename='Courier' size=18x12.menus&per.c:font facename=default size=0x0.,
because these two classes share many common menu items, so we can
share most of the code also&per.
:p.
All newly inserted menu items use :hp1.variable:ehp1. menu IDs&per. You can tell
this from their #define names, which have an :font facename='Courier' size=18x12._OFS_:font facename=default size=0x0. in their name
(check :font facename='Courier' size=18x12.dlgids&per.h:font facename=default size=0x0.)&per. To all these values, XWorkplace adds the offset that
is specified on the "Paranoia" notebook page&per.
:p.
For an introduction to how the config folder menu items work, see the
"XWorkplace Internals" section in the
:hp1.XWorkplace User Guide:ehp1.&per.
XWorkplace uses a fairly obscure system of global
variables to be able to relate menu items to their intended
functions&per. All the variable menu items are stored in a global linked
list when the context menu is opened for a folder&per. In each list item,
XWorkplace also stores what kind of object (template, program object,
folder content item etc&per.) the menu item represents&per. This list is then
examined by :font facename='Courier' size=18x12.wpMenuItemSelected:font facename=default size=0x0. to perform the corresponding action
(create a new object, start program etc&per.)&per. After that, the list is
destroyed, so it only eats up memory while the context menu is open&per.
All this code is now (V0&per.81) in :font facename='Courier' size=18x12.menus&per.c:font facename=default size=0x0.&per.
:p.
As opposed to the "regular" new menu items,
the :hp2.folder content menus:ehp2. are initially only inserted as empty submenu stubs&per.
They are only filled with items when they're opened&colon. XWorkplace
intercepts :font facename='Courier' size=18x12.WM_INITMENU:font facename=default size=0x0. in the subclassed folder frame proc
(:font facename='Courier' size=18x12.fnwpSubclassedFolderFrame:font facename=default size=0x0., :font facename='Courier' size=18x12.src\filesys\folder&per.c:font facename=default size=0x0.) and then
populates the menu&per.
These menu items are stored in the global list too and marked as
folder content menu items so that :font facename='Courier' size=18x12.wpMenuItemSelected:font facename=default size=0x0. will then simply
open the corresponding WPS object&per. The folder content code has now
been moved to :font facename='Courier' size=18x12.menus&per.c:font facename=default size=0x0. also&per.
:p.
Painting the icons is then done using owner-draw menu items; the
messages which are necessary for this are also intercepted in
:font facename='Courier' size=18x12.fnwpSubclassedFolderFrame:font facename=default size=0x0. and then call corresponding functions in
:font facename='Courier' size=18x12.menus&per.c:font facename=default size=0x0.&per.
:p.
The folder content :hp1.menu windows:ehp1. are also subclassed to be able to intercept
right-mouse button clicks&per. The new window proc for this is called
(suprise!) :font facename='Courier' size=18x12.fnwpFolderContentMenu:font facename=default size=0x0.&per.
:p.
The :font facename='Courier' size=18x12.xfSelectingMenuItem:font facename=default size=0x0. method introduced in V0&per.80 has been removed
again with V0&per.81 due to the problems with SOM multiple inheritance&per.
.br
.* Source filename: src2_811sort.html
:h2  x=right width=70% res=35. Extended Sort Functions 
:font facename=default size=0x0.
:p.
The default WPS sort functions are a mess, and I suppose IBM knows that, since
they have hardly documented them at all&per.
:p.So here's what I found out&per.
:p.Normally, the WPS relates sort criteria to :hp2.object details&per.:ehp2. That is, to
implement a sort criterion, you have to have a corresponding object detail, as
defined in the :font facename='Courier' size=18x12.wpclsQueryDetailsInfo:font facename=default size=0x0. and :font facename='Courier' size=18x12.wpQueryDetailsData:font facename=default size=0x0.
methods&per. This is superficially explained in the "WPS Programming Guide", in
"Object Criteria&colon. Details methods"&per.
:p.There are many more object details than those shown in folder Details
views&per. I have counted 23 here (but your results may vary), and since there
are only 13 columns in Details view, there should be
about 10 invisible details&per. (Side note&colon. You can view all of them by turning
to the "Include" page of any folder and pressing the "Add&per.&per.&per." button&per.)
:p.Most of the details
are defined in WPFileSystem's override of :font facename='Courier' size=18x12.wpclsQueryDetailsInfo:font facename=default size=0x0.,
and if certain sort flags are specified there (even when the details column is
never visible), the WPS will make this detail available as a sort criterion&per.
That is, if such a sort flag is set for an object detail,
the WPS inserts the column title in the "Sort" submenu,
shows it on the "Sort" page, and sets some corresponding comparison function
for the folder containers automatically&per.
:p.This works beautifully for "real" subclasses of default WPS classes&per. For example,
you can create some subclass of WPDataFile, e&per.g&per. "WPAddress" for storing addresses,
define new object details for that class, set the "folder sort class" of a folder to
WPAddress, and you can then sort the folder according to addresses automatically&per.
:p.So, in theory, the whole details concept is great&per. You add new details to
a class, and the WPS can automatically sort, display the details, and even
set the "Include" options for folder accordingly&per.
:p.But unfortunately, all the details/include/sort stuff is yet another example
of another great IBM concept which was only half implemented&per.
I have found out the hard way that :hp2.XWorkplace cannot use this approch:ehp2.
to extend the default sort criteria&per. This is for the following reasons (believe
me, I've tried all of the following)&colon.
:ol compact.
:li.
It is impossible to add new default details to a replacement class of
WPFileSystem&per.
Even if WPFileSystem is replaced with another class, :font facename='Courier' size=18x12.wpQueryFldrSortClass:font facename=default size=0x0. and
:font facename='Courier' size=18x12.wpQueryFldrDetailsClass:font facename=default size=0x0. do not return the replacement class object,
but the original WPFileSystem class object&per. I have then tried to replace these two methods
too, and details/sorting would still not always work&per. There seem to be some ugly
kludges in the WPS internally&per.
.br
:li.
Apparently, two sort criteria ("Name" and "Type") do :hp1.not:ehp1. correspond to
any details columns&per. Normally, sort criteria are specified by passing the corresponding
details column to a sort function (see :font facename='Courier' size=18x12.wpIsSortAttribAvailable:font facename=default size=0x0., for
example)&per. However, these two criteria have indices of "-2" and "-1", so there seems
to be some hard-coded special case check in the WPS again&per. The "Type" criterion is
even available when WPObject is selected as the folder sort class, but WPObject
does not define that detail&per. So who knows&per.
.br
:li.
The documentation for :font facename='Courier' size=18x12.CLASSFIELDINFO:font facename=default size=0x0. is incomplete and partly
incorrect&per. I have tried to specify new sort functions in there, and the WPS would
just hang&per. Apparently the function prototypes are not correct, because my
own comparison function
would never reach its first instruction before the hang&per.
.br
:li.
The :font facename='Courier' size=18x12.wpQueryFldrSort:font facename=default size=0x0. and :font facename='Courier' size=18x12.wpSetFldrSort:font facename=default size=0x0. methods
are completely obscure&per. They are prototyped using a :font facename='Courier' size=18x12.PVOID:font facename=default size=0x0. parameter, and the
documentation says this should point to a :font facename='Courier' size=18x12.SORTFASTINFO:font facename=default size=0x0. structure&per. But
for one, this structure is defined nowhere in the headers, and even more importantly,
the data passed to these methods does not correspond to the description in the
WPS reference&per. The first field in the structure is definitely no sort function&per.
So we have one more mystery there&per.
.br
:li.
The most important limitation was however that there are no default settings
for sorting&per. The default sort criterion is always set to "Name", and "Always sort"
is always off&per. This is hard-coded into WPFolder and cannot be changed&per. There is not
even an instance method for querying or setting the "Always sort" flag, let alone
a class method for the default values&per. The only documented thing is the
:font facename='Courier' size=18x12.ALWAYSSORT:font facename=default size=0x0. setup string&per.
:eol.
.br
Besides, the whole concept is so complicated that I only understood it now after
having debugged all the related methods for several days&per. I don't think many users
appreciate the flexibility of that concept or even know what "sort classes" or
"details classes" are about,
even though there may be certain WPS classes that use this concept&per.
:p.:hp2.The XWorkplace approach&per.:ehp2. It's basically a "brute force" method&per. XWorkplace lets
the WPS add the default sort features to context menus as usual,
then adds its own, and just
intercepts :hp1.all:ehp1. the menu ID's in :font facename='Courier' size=18x12.wpMenuItemSelected:font facename=default size=0x0.&per.
:p.I have then
written a complete new set of sort comparison functions (in :font facename='Courier' size=18x12.cnrsort&per.*:font facename=default size=0x0.)
which do the job for the various sort criteria&per. These are regular :hp1.container:ehp1.
comparison functions, as used with the CM_SORTRECORD message, thus bypassing
all the information in the WPS's :font facename='Courier' size=18x12.CLASSFIELDINFO:font facename=default size=0x0. structures&per.
So it's not the WPS at all any more which does the sorting, but
XWorkplace, and XWorkplace uses its own set of global and instance settings
for all this&per. See
:font facename='Courier' size=18x12.fdrSetFldrCnrSort:font facename=default size=0x0. in :font facename='Courier' size=18x12.src\filesys\folder&per.c:font facename=default size=0x0. for how this is done&per.
:p.As you know, XWorkplace replaces the "Sort" page in folder settings notebooks
altogether too so we can set the new instance settings&per.
:p.In addition, XWorkplace overrides the :font facename='Courier' size=18x12.wpSetFldrSort:font facename=default size=0x0. method, which
apparently gets called every time the WPS tries to sort a folder&per. That is, normally,
if one
of the "Sort" menu items would get called, but these are intercepted already by
XWorkplace&per. The override is still needed though because the WPS calls that method frequently
when "Always sort" is on also, e&per.g&per. when you rename a file&per.
:p.Finally, there's a real neat hack to get access to the internal folder sort settings,
i&per.e&per. the settings that reside in the memory allocated by the original WPFolder class&per.
In :font facename='Courier' size=18x12.wpRestoreData:font facename=default size=0x0., which gets called when an object is awakened, WPFolder
always attempts to restore its folder sort settings from the instance settings&per.
:p.Since
the caller always passes a block of memory to which :font facename='Courier' size=18x12.wpRestoreData:font facename=default size=0x0. should
write if the data was found, we can intercept that pointer and store it in XWorkplace's
instance data&per. We can therefore manipulate the "Always sort" flag also&per.
:p.BTW, this can be used as a general approach to get access to WPS-internal data, if
a corresponding :font facename='Courier' size=18x12.ulKey:font facename=default size=0x0. exists&per. Just override :font facename='Courier' size=18x12.wpRestoreData:font facename=default size=0x0.
(or the other restore methods) and check for what keys they get called&per. Most of them
are declared in :font facename='Courier' size=18x12.wpfolder&per.h:font facename=default size=0x0. in the Toolkit headers&per.
.br
.* Source filename: src2_812threads.html
:h2  x=right width=70% res=36. XWorkplace Threads 
:font facename=default size=0x0.
:p.
XWorkplace is quite heavily multi-threaded and offloads most tasks which
are probable to take some time to several threads which are always running in
the background&per. Those threads create object windows and are thus normally
blocked, unless there's real work to do&per.
XWorkplace uses mutex semaphores all over the place to serialize access
to global data structures&per.
:p.With V0&per.9&per.0, all the thread code has been put into a new file, called
:font facename='Courier' size=18x12.src\filesys\xthreads&per.c:font facename=default size=0x0.&per.
:p.All threads are created in :font facename='Courier' size=18x12.krnInitializeXWorkplace:font facename=default size=0x0.
(:font facename='Courier' size=18x12.src\shared\kernel&per.c:font facename=default size=0x0.),
which gets called from :font facename='Courier' size=18x12.M_XFldObject&colon.&colon.wpclsInitData:font facename=default size=0x0., which is probably
the first SOM method called when the WPS is initializing at startup&per.
:p.All threads have some :font facename='Courier' size=18x12.fntXXXThread:font facename=default size=0x0. function, which is
the main thread function passed to the :font facename='Courier' size=18x12.thr*:font facename=default size=0x0. functions in
:font facename='Courier' size=18x12./helpers/threads&per.c:font facename=default size=0x0. when the thread is created&per.
These in turn create an object window using some :font facename='Courier' size=18x12.fnwpXXXObject:font facename=default size=0x0.
function as their window procedure&per.
:p.For each thread, we then have some :font facename='Courier' size=18x12.krnPostXXXMsg:font facename=default size=0x0. function
which posts a message to the corresponding object window&per. All messages are
defined and explained in :font facename='Courier' size=18x12.include\filesys\xthreads&per.h:font facename=default size=0x0.&per.
:p.The following additional threads are available&colon.
:ul compact.
:li.
The :hp2.Worker thread:ehp2. is running with idle priority and does
mainly maintenance which is not time-critical&per.
:p.For example, the Worker thread maintains the global linked list of
currently awake WPS objects&per. :font facename='Courier' size=18x12.XFldObject&colon.&colon.wpObjectReady:font facename=default size=0x0. post
:font facename='Courier' size=18x12.WOM_ADDAWAKEOBJECT:font facename=default size=0x0. to the Worker thread, which then adds the object
to that list&per. (This list is needed by XShutdown to store all the awake
WPS objects; see the :hp1.XWorkplace User Guide:ehp1. for details)&per.
:p.The Worker thread runs at "Idle" priority, unless more than 300
messages have piled up in its message queue&per. In this case, the
priority is temporarily raised to "Regular" and set back if the
message count goes below 10 again&per. This can happen when opening
folders with a very large number of objects&per.
.br
:li.
The :hp2."Speedy" thread:ehp2. is running at a high
"Regular" priority for things which won't take long but should not
block the main WPS (Workplace) thread&per. This thread creates an object
window also&per.  For example, the :link reftype=hd res=37.new system sounds:elink.
are played here, and the WPS boot logo is shown&per.
.br
:li.
The :hp2."File" thread:ehp2. is new with V0&per.9&per.0 and runs with regular priority,
that is, concurrently with the WPS user interface&per. This now does file operations,
such as moving objects to and emptying the trash can and such&per.
This thread will probably get more jobs in the future&per.
:eul.
.br
.br
.* Source filename: src2_814sounds.html
:h2  x=right width=70% res=37. Playing Sounds 
:font facename=default size=0x0.
:p.
The new system sounds are played in the "Speedy" thread with high
regular priority&per. Check :font facename='Courier' size=18x12.fntSpeedyThread:font facename=default size=0x0. and :font facename='Courier' size=18x12.fnwpSpeedyObject:font facename=default size=0x0.
for details about how this is implemented using MMOS/2&per.
:p.There are two above-average programming tricks related to this&colon.
:ul compact.
:li.
For one, nowhere is documented how to access and modify the
system sounds that appear in the :hp2."Sound" object&per.:ehp2. I have accidently
discovered that
this is actually fairly easy&colon. these are stored in :font facename='Courier' size=18x12.MMPM&per.INI:font facename=default size=0x0. in the
MMOS/2 directory&per. Each sound has an index in that file, which I have
declared in :font facename='Courier' size=18x12.common&per.h:font facename=default size=0x0. (those :font facename='Courier' size=18x12.MMSOUND_*:font facename=default size=0x0. #define's)&per.
These should be the
same on every system&per. The INI data then contains a
:font facename='Courier' size=18x12."<soundfile>#<description>#<volume>":font facename=default size=0x0. string&per.
:p.
Sound data manipulation has been moved into a separate file with V0&per.9&per.0&colon.
check :font facename='Courier' size=18x12./helpers/syssound&per.c:font facename=default size=0x0. for details&per. This now also supports
sound scheme manipulation&per.
.br
:li.
With V0&per.82, the actual playing of the sounds has been moved to a
separate :font facename='Courier' size=18x12.SOUND&per.DLL:font facename=default size=0x0. because previously XWorkplace would refuse to install on
systems where MMPM/2 was not installed&per. This was due to the fact that
if you link a DLL against the MMPM/2 library functions, loading of the
DLL fails if these imports cannot be resolved&per. (Naturally, you don't get an
error message other than FALSE from the SOM kernel&per. This has taken me days
to find out&per.)
:p.Now, if loading of :font facename='Courier' size=18x12.SOUND&per.DLL:font facename=default size=0x0. fails (either because it's not there or
because the MMPM/2 imports of :hp1.that:ehp1. DLL cannot be resolved), the new system
sounds are silently disabled&per. This happens in :font facename='Courier' size=18x12.xthrInitializeThreads:font facename=default size=0x0.&per.
:p.Also, I've added much more
sophisticated MMPM/2 messaging to check for whether the waveform
device is actually available&per. So playing sounds is a process of
communication between the Speedy thread and :font facename='Courier' size=18x12.SOUND&per.DLL:font facename=default size=0x0. now&per. See the
top of :font facename='Courier' size=18x12./main/sounddll&per.c:font facename=default size=0x0. for more explanations&per.
:eul.
.br
.* Source filename: src2_820shutdown.html
:h2  x=right width=70% res=38. XShutdown 
:font facename=default size=0x0.
:p.
XShutdown (i&per.e&per. the "eXtended Shutdown" and "Restart WPS" features)
resides entirely in the :font facename='Courier' size=18x12.shutdown&per.c:font facename=default size=0x0. file&per.
:p.
XShutdown starts two additional threads to close all the windows&per. This is described
in detail in the "XWorkplace Internals" section of the :hp1.XWorkplace User Guide:ehp1.&per.
I have also tried to put plenty of comments into the sources&per.
.br
.* Source filename: src2_840except.html
:h2  x=right width=70% res=29. XWorkplace Exception Handling 
:font facename=default size=0x0.
:p.
XWorkplace registers additional exception handlers in certain parts
where I considered worth it&per.
:p.With "worth it" I mean the following situations&colon.
:ol compact.
:li.
Certain code parts
crashed on my system and these parts seemed error-prone enough to me
to outweigh the performance loss of registering and deregistering
exception handlers&per.
.br
:li.
Exception handlers :hp1.must:ehp1. be registered for all additional
threads&per. If no exception handling was registered there, crashes would take
the whole WPS down, because the default WPS exception handler only
deals with the default WPS threads&per.
.br
:li.
Exception handlers :hp1.must:ehp1. also be registered every time
mutex semaphores are used&per. If a code part crashes while a mutex
semaphore has been requested by a function, all other threads
waiting for that semaphore to be released will be blocked forever&per.
And I mean forever, because even :font facename='Courier' size=18x12.DosKillProcess:font facename=default size=0x0. won't be
able to terminate that thread&per. You'd have to reboot to get out of this&per.
So the exception handler must always check for whether a mutex semaphore
is currently owned by the thread and, if so, release it&per.
:eol.
.br
XWorkplace does not use the VAC++ library funcs for all this, but installs its
own set of quite complex exception handlers
(using :font facename='Courier' size=18x12.DosSetExceptionHandler():font facename=default size=0x0.)&per.
The code for exception handlers has been made independent of XWorkplace
and moved to :font facename='Courier' size=18x12./helpers/except&per.c:font facename=default size=0x0. with V0&per.9&per.0&per.
:p.Also, I have created a few handy macros which automatically register and
deregister the XWorkplace exception handlers&per. This avoids the frequent problem
that one forgets to deregister an exception handler, which leads to
really awkward problems which are almost impossible to debug&per.
Those macros are called :font facename='Courier' size=18x12.TRY_xxx:font facename=default size=0x0. and :font facename='Courier' size=18x12.CATCH:font facename=default size=0x0. to
mimic at least some C++ syntax&per.
See the top of :font facename='Courier' size=18x12.src\helpers\except&per.c:font facename=default size=0x0. for detailed instructions how
to use these&per.
:p.
The XWorkplace exception handlers are the following&colon.
:ul compact.
:li.
:font facename='Courier' size=18x12.excHandlerLoud:font facename=default size=0x0. is the one that makes the loud sounds
and writes the :font facename='Courier' size=18x12.XFLDTRAP&per.LOG:font facename=default size=0x0.
file which is well-known to many XWorkplace users (grin)&per. It uses
a :font facename='Courier' size=18x12.longjmp():font facename=default size=0x0. to get back to the function, which might then
react to that exception by trying to restore some safe
state of the thread&per. See the func header for details about
how this works&per.
:p.
This handler is used by all the additional XWorkplace threads
and also by the subclassed folder frame and folder content
menu window procs&per. This slows down the system a bit because
the handler must be registered and deregistered for each
message that comes in, but there's no other way to do this&per.
(I think&per.)
:p.With V0&per.84, I have added lots of debugging code which I found in
the EXCEPTQ&per.ZIP package at Hobbes&per. The exception handlers are now capable
of finding symbols either from debug code (if present) or from a SYM file
in order to write more meaningful trap logs&per.
See the top of :font facename='Courier' size=18x12.except&per.c:font facename=default size=0x0. for details&per.
.br
:li.
:font facename='Courier' size=18x12.excHandlerQuiet:font facename=default size=0x0. is similar to :font facename='Courier' size=18x12.excHandlerPlus:font facename=default size=0x0. in that it
uses a :font facename='Courier' size=18x12.longjmp():font facename=default size=0x0. also, but neither is this reported to the
user nor is the logfile written to (that's why it's "quiet")&per.
:p.
This is used in places where exceptions have ben known
to occur and there's no way to get around them&per. I created
this handler for V0&per.80 because I found out that :font facename='Courier' size=18x12.somIsObj():font facename=default size=0x0.
is :hp1.not:ehp1. a fail-safe routine to find out if a SOM pointer
is valid, even though IBM claims it is&per.
:p.
(These were the strange errors in the Worker thread which
appeared in V0&per.71 when folders with subfolders were
deleted, because the Worker thread then tried to access
these objects when the folder was populated right before
deletion&per.)
:p.
So I created the :font facename='Courier' size=18x12.wpshCheckObject:font facename=default size=0x0. func which can
return FALSE, using this handler, if access to the object
fails&per.
:eul.
.br
.br
.* Source filename: 055extend.html
:h1  group=1 width=30% res=7.         Adding Features to XWorkplace     
:font facename=default size=0x0.
:p.
:link reftype=hd res=39 auto dependent.
:ul compact.
:li.
:link reftype=hd res=39.Overview:elink.
.br
:li.
:link reftype=hd res=40.Adding a new class:elink.
.br
:li.
:link reftype=hd res=41.Extending existing XWorkplace classes:elink.
.br
:li.
:link reftype=hd res=42.The "XWorkplace Setup" object:elink.
.br
:li.
:link reftype=hd res=43.XWorkplace code you might find useful:elink.
:eul.
.br
.* Source filename: ext_1overview.html
:h2  x=right width=70% res=39. Overview 
:font facename=default size=0x0.
:p.
This section describes how you can integrate your own code into XWorkplace&per.
Since XWorkplace is now a Netlabs project on the Netlabs CVS server, certain
rules must be followed so that the whole thing still works even though several
developers might be adding stuff&per.
:p.With XWorkplace V0&per.9&per.0, I have restructured the whole directory and
makefile system to allow for extending XWorkplace&per. In any case, the idea is
that all XWorkplace classes will reside in one single DLL, :font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0.,
but should be capable of being registered independently&per.
:p.Basically, there are two situations which you need to tell apart&colon.
:ol compact.
:li.
You can :link reftype=hd res=40.add a new class to XWorkplace:elink.&per. That is,
you wish to hook your code into the WPS, but XWorkplace doesn't have a class for
this yet&per. This situation applies both if you wish to replace a standard WPS class
(example&colon. the spooler class, WPSpool) which XWorkplace doesn't replace yet, or if
you wish to create an all new class&per. (This only makes a difference at install time,
whether the install program will replace a class&per.)
.br
:li.
You may choose to :link reftype=hd res=41.extend an existing XWorkplace class:elink.
(either an XWorkplace replacement for a default WPS class, e&per.g&per. XFolder, or a newly
introduced XWorkplace class, e&per.g&per. XFldWPS -- the "Workplace Shell" object)&per.
This is much simpler, because you can build on existing code&per.
:eol.
.br
.* Source filename: ext_2addclass.html
:h2  x=right width=70% res=40. Adding a New Class 
:font facename=default size=0x0.
:p.
This section describes what to do if you want to write some all new SOM WPS class,
which should be integrated into the main XWorkplace DLL, and describe which files
need to be changed to have your stuff compiled into the whole thing&per.
:ol compact.
:li.
Most importantly, I suggest that you put all your code into a
:hp2.separate source directory:ehp2.
in the XWorkplace :font facename='Courier' size=18x12.SRC\:font facename=default size=0x0. and :font facename='Courier' size=18x12.INCLUDE\:font facename=default size=0x0. directory trees&per.
Please do not put your stuff into :font facename='Courier' size=18x12.SRC\FILESYS:font facename=default size=0x0.,
because this will make it pretty difficult to maintain order&per.
For the purpose of explanations here, I will assume that your directory is
called :font facename='Courier' size=18x12.SRC\YOURDIR\:font facename=default size=0x0. and your class is called :font facename='Courier' size=18x12.XWPYourClass:font facename=default size=0x0.&per.
:p.So first of all, create your two subdirectories (:font facename='Courier' size=18x12.YOURDIR:font facename=default size=0x0.) in
:font facename='Courier' size=18x12.INCLUDE:font facename=default size=0x0. and :font facename='Courier' size=18x12.SRC:font facename=default size=0x0.&per.
.br
:li.
Put your new IDL file into the :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. directory and modify the
makefile in there to recognize your IDL file&per. That is, add your header file to
the :font facename='Courier' size=18x12.all&colon.:font facename=default size=0x0. statement and add a line to the bottom&per. And please, add a
comment that you did so&per.
:p.For the purpose of clarity, I suggest that with your IDL file, you take one
of the existing IDL files as a template&per. My IDL coding style has evolved during
the last two years, and I now consider the comments etc&per. in there pretty lucid
in order not to forget anything&per. Of course, if you have something better, go ahead&per.
.br
:li.
In :font facename='Courier' size=18x12.SRC\YOURDIR\:font facename=default size=0x0., write your own makefile which compiles your
sources&per. You can take the makefile in :font facename='Courier' size=18x12.SRC\FILESYS\:font facename=default size=0x0. as a template&per. This
makefile is pretty smart because it automatically recognizes whether it is called
from the main makefile, and if not, it invokes the main makefile, which in turn
will call the sub-makefiles later&per. Also, that makefile uses the general makefile
include :font facename='Courier' size=18x12.setup&per.in:font facename=default size=0x0. in the main directory for compiler setup etc&per.
:p.Make sure that your makefile writes all :font facename='Courier' size=18x12.&per.OBJ:font facename=default size=0x0. files into the
:font facename='Courier' size=18x12.BIN:font facename=default size=0x0. directory, which your makefile must create if it doesn't exist yet&per.
Again, see how the makefile in :font facename='Courier' size=18x12.SRC\FILESYS:font facename=default size=0x0. does this&per.
:p.Other than that, in :font facename='Courier' size=18x12.SRC\YOURDIR:font facename=default size=0x0., do whatever you want&per.
.br
:li.
:hp2.Coding&per.:ehp2. For your C code, make sure that you get the :font facename='Courier' size=18x12.#include:font facename=default size=0x0.'s
right&per.
Take a look at any C file in :font facename='Courier' size=18x12.SRC\FILESYS:font facename=default size=0x0. for examples (:font facename='Courier' size=18x12.folder&per.c:font facename=default size=0x0.
is a good candidate, because it's fairly complex)&per. If you use any headers from
:font facename='Courier' size=18x12.include\shared\:font facename=default size=0x0., there are certain rules that you must follow, because
these headers require other headers to be included already&per.
:p.Also, I strongly recommend to :hp1.always:ehp1. include :font facename='Courier' size=18x12.include\setup&per.h:font facename=default size=0x0.,
because this will automatically make your code PMPRINTF-enabled&per.
:p.I don't care about your :hp2.coding style,:ehp2. but if you want your code to be
documented automatically, you should follow mine, because otherwise
:font facename='Courier' size=18x12.xdoc&per.exe:font facename=default size=0x0. won't work&per.
See :link reftype=hd res=24.Code documentation:elink. for details&per.
.br
:li.
Note that the SOM compiler is unable to recognize that several
classes should be put into the same DLL and create a common &per.DEF file for this
purpose, so you have to do this manually&per.
:p.So have the SOM compiler create a &per.DEF file for your class from your &per.IDL
source file&per. (The makefile in :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. will do this automatically if you
have added your header to the :font facename='Courier' size=18x12.all&colon.:font facename=default size=0x0. target&per.)
:p.Then take the block below the :font facename='Courier' size=18x12.EXPORTS:font facename=default size=0x0. line from the &per.DEF file
and add it to the bottom of :font facename='Courier' size=18x12.src\shared\xwp&per.def:font facename=default size=0x0. (which is the module
definition file used for linking the whole XWorkplace DLL)&per. These structures make
the SOM kernel see your class in the DLL&per. If you don't do this, your class
cannot be registered&per.
.br
:li.
Finally, take a look at :font facename='Courier' size=18x12.\makefile:font facename=default size=0x0.&per. This is the "master makefile"
which links all &per.OBJ modules into the main XWorkplace DLL (:font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0.)&per.
In that makefile, there is an :font facename='Courier' size=18x12.OBJS:font facename=default size=0x0. macro which lists all the &per.OBJ files
which are to be linked together&per.
:p.Add your &per.OBJ file(s) to the end of that variable (and please, add a comment that
you did so)&per.
:eol.
.br
This should work&per. If you have any questions, feel free to contact me&per.
.br
.* Source filename: ext_3extend.html
:h2  x=right width=70% res=41. Extending Existing XWorkplace Classes 
:font facename=default size=0x0.
:p.
This section describes what to do if the class that you need already exists in
XWorkplace&per. Of course, this requires less setup work, because much of the work
has already been done&per.
:ol compact.
:li.
If you need a new WPS method which isn't overridden yet, modify the
IDL file in :font facename='Courier' size=18x12.IDL\:font facename=default size=0x0. to suit your needs&per. Please, please, if you modify
my IDL files, follow the coding style which exists, especially those "@@" flags etc&per.,
which are needed for :link reftype=hd res=24.code documentation:elink. to work&per.
:p.If the method you need is already overridden by XWorkplace, go to :hp2.3&per.:ehp2.
.br
:li.
If you then re-make XWorkplace, the SOM compiler will automatically get invoked
and modify the sources in :font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0. and the headers in
:font facename='Courier' size=18x12.INCLUDE\CLASSES:font facename=default size=0x0. accordingly&per.
.br
:li.
Again, as said on the :link reftype=hd res=40.previous page:elink., add your own
directory to :font facename='Courier' size=18x12.INCLUDE:font facename=default size=0x0. and :font facename='Courier' size=18x12.SRC:font facename=default size=0x0.&per. Modify the SOM code of the
class you need in :font facename='Courier' size=18x12.SRC\CLASSES:font facename=default size=0x0. to call your implementation in your
:font facename='Courier' size=18x12.SRC\YOURDIR:font facename=default size=0x0. directory&per.
.br
:li.
You will need to add your header from :font facename='Courier' size=18x12.INCLUDE\YOURDIR:font facename=default size=0x0. to the
SOM class code file that you modified&per. Please think of some useful
:link reftype=hd res=27.function prefix:elink. for your exported functions so
that other programmers (including me) can find your code more easily&per.
:p.Don't forget to update :font facename='Courier' size=18x12.src\classes\makefile:font facename=default size=0x0. so that the class code
file will be made dependent on your header&per.
.br
:li.
In :font facename='Courier' size=18x12.SRC\YOURDIR\:font facename=default size=0x0., write your own makefile which compiles your
sources&per. You can take the makefile in :font facename='Courier' size=18x12.SRC\FILESYS\:font facename=default size=0x0. as a template&per. This
makefile is pretty smart because it automatically recognizes whether it is called
from the main makefile, and if not, it invokes the main makefile, which in turn
will call the sub-makefiles later&per. Also, that makefile uses the general makefile
include :font facename='Courier' size=18x12.setup&per.in:font facename=default size=0x0. in the main directory for compiler setup etc&per.
:p.Make sure that your makefile writes all :font facename='Courier' size=18x12.&per.OBJ:font facename=default size=0x0. files into the
:font facename='Courier' size=18x12.BIN:font facename=default size=0x0. directory, which your makefile must create if it doesn't exist yet&per.
Again, see how the makefile in :font facename='Courier' size=18x12.SRC\FILESYS:font facename=default size=0x0. does this&per.
:p.Other than that, in :font facename='Courier' size=18x12.SRC\YOURDIR:font facename=default size=0x0., do whatever you want&per.
.br
:li.
Finally, take a look at :font facename='Courier' size=18x12.\makefile:font facename=default size=0x0.&per. This is the "master makefile"
which links all &per.OBJ modules into the main XWorkplace DLL (:font facename='Courier' size=18x12.XFLDR&per.DLL:font facename=default size=0x0.)&per.
In that makefile, there is an :font facename='Courier' size=18x12.OBJS:font facename=default size=0x0. macro which lists all the &per.OBJ files
which are to be linked together&per.
:p.Add your &per.OBJ file(s) to the end of that variable (and please, add a comment that
you did so)&per.
:eol.
.br
.* Source filename: ext_4settings.html
:h2  x=right width=70% res=42. The "XWorkplace Setup" Object 
:font facename=default size=0x0.
:p.
If you have added new code to XWorkplace, you might wonder how to integrate
your code into the :hp2."XWorkplace Setup" object,:ehp2. most notably, how to
add stuff to the "Features" container&per.
:p.This is pretty easy&per. The "Features" page uses a subclassed container
(the code for those checkboxes is in :font facename='Courier' size=18x12.helpers\comctl&per.c:font facename=default size=0x0., but
this you need not worry about)&per. All you have to do is take a look at
:font facename='Courier' size=18x12.src\shared\xsetup&per.c:font facename=default size=0x0.&per. The :font facename='Courier' size=18x12.setFeaturesInitPage:font facename=default size=0x0.
function is responsible for initializing the container with the features,
while the :font facename='Courier' size=18x12.setFeaturesItemChanged:font facename=default size=0x0. function reacts to selection
changes in the container&per.
:p.To add something, perform the following steps&colon.
:ol compact.
:li.
Add a dialog item ID to :font facename='Courier' size=18x12.include\dlgids&per.h:font facename=default size=0x0., where all the
other items of this kind are (search for :font facename='Courier' size=18x12.ID_XCSI_GENERALFEATURES:font facename=default size=0x0.
to find them)&per.
.br
:li.
Add a string for your setting to the NLS strings (e&per.g&per. for English, to
:font facename='Courier' size=18x12.001\xfldr001&per.rc:font facename=default size=0x0.), using that ID&per.
.br
:li.
Add your ID to the :font facename='Courier' size=18x12.FeatureItemsList:font facename=default size=0x0. array in
:font facename='Courier' size=18x12.src\shared\xsetup&per.c:font facename=default size=0x0.&per. Your item will then automatically get
inserted into the container by :font facename='Courier' size=18x12.setFeaturesInitPage:font facename=default size=0x0.&per.
.br
:li.
Still, in :font facename='Courier' size=18x12.setFeaturesInitPage:font facename=default size=0x0., you need to add a line
which checks the container checkbox according to the setting (those
:font facename='Courier' size=18x12.ctlSetRecordChecked(hwndFeaturesCnr &per.&per.&per.:font facename=default size=0x0. lines)&per.
.br
:li.
In :font facename='Courier' size=18x12.setFeaturesItemChanged:font facename=default size=0x0., add a case/switch which
reacts to user changes&per.
:eol.
.br
:hp2.Important note&colon.:ehp2. Please do not use the :font facename='Courier' size=18x12.GLOBALSETTINGS:font facename=default size=0x0.
structure (in :font facename='Courier' size=18x12.common&per.h:font facename=default size=0x0.) to store your setting&per. Instead, call
some other code which stores your setting in a separate key in OS2&per.INI or
wherever you want&per. If several programmers start messing with the
:font facename='Courier' size=18x12.GLOBALSETTINGS:font facename=default size=0x0. structure, we'll soon get complete chaos&per.
Thank you&per.
.br
.* Source filename: ext_5useful.html
:h2  x=right width=70% res=43. XWorkplace Code You Might Find Useful 
:font facename=default size=0x0.
:p.
This section should help you identify XWorkplace code sections which might
also be useful to you&per.
:ol compact.
:li.
This applies mostly to the code in the :font facename='Courier' size=18x12.HELPERS:font facename=default size=0x0. directory&per.
All of that code is not dependent on XWorkplace and could be used in any
program&per. There are lots of Control Program, Presentation Manager and GPI
helper functions which might solve problems that you are having&per.
:p.To use the helper funcs, simply add :font facename='Courier' size=18x12.#include "helpers\xxx&per.h":font facename=default size=0x0.
at the top of your code&per. See the top of the respective header for additional
:font facename='Courier' size=18x12.#include:font facename=default size=0x0.'s which are required by the helper&per.
.br
:li.
There are some functions in :font facename='Courier' size=18x12.src\shared\common&per.c:font facename=default size=0x0. which might be
useful to you, most notably to query some XWorkplace settings and NLS stuff&per.
:font facename='Courier' size=18x12.cmnQueryGlobalSettings:font facename=default size=0x0. will return the global settings structure,
for example, or :font facename='Courier' size=18x12.cmnMessageBox:font facename=default size=0x0. will display one of the pretty
XWorkplace message boxes&per.
Since your code should be NLS enabled, you should always use
:font facename='Courier' size=18x12.cmnQueryLanguageCode:font facename=default size=0x0. to find out what language is currently used&per.
.br
:li.
To find out how to create a :font facename='Courier' size=18x12.WPAbstract:font facename=default size=0x0. class from scratch,
including a completely new view defined by your class (like the "Class list view"),
you can take a look at :font facename='Courier' size=18x12.xclslist&per.idl:font facename=default size=0x0. and :font facename='Courier' size=18x12.xclslist&per.c:font facename=default size=0x0.,
which do exactly this&per. See the comments in that file for instructions&per.
.br
:li.
To create a new settings object (which is not derived from WPSystem), take
a look at :hp2.XWPSetup:ehp2., a direct subclass of WPAbstract, which is implemented
this way&per.
.br
:li.
For easier maintenance of notebook dialog procs, I have created
:font facename='Courier' size=18x12.src\shared\notebook&per.c:font facename=default size=0x0.,
which does just this using callbacks, so you don't have to rewrite the same
stupid window procedures for each notebook page&per. This is used throughout XWorkplace's
source code whenever settings pages are inserted, and has proven to be extremely
useful&per.
.br
:li.
Finally, :font facename='Courier' size=18x12.src\shared\kernel&per.c:font facename=default size=0x0. contains code which you can
extend for certain tricky situations&per. For one, you can extend
:font facename='Courier' size=18x12.krnInitializeXWorkplace:font facename=default size=0x0. to have code executed upon WPS startup;
secondly, you can add messages to :font facename='Courier' size=18x12.krn_fnwpThread1Object:font facename=default size=0x0. if you
need code to absolutely always execute on thread 1 of PMSHELL&per.EXE, which cannot
be guaranteed with open views of any kind (especially folder views)&per.
:eol.
.br
.br
:h1 group=99 x=right width=30%.Resources on the Internet
:font facename=default size=0x0.
:p.This chapter contains all external links referenced in this book.
 Each link contained herein is an Unified Resource Locator (URL) to a certain location
 on the Internet. Simply double-click on one of them to launch Netscape
 with the respective URL.
:h2 res=18 group=98 x=right y=bottom width=60% height=40%.http&colon.//service&per.boulder&per.ibm&per.com/dl/ddk/priv/ddk-d
:font facename=default size=0x0.
:p.:lines align=center.
Click below to launch Netscape with this URL&colon.
:font facename='System VIO' size=14x8.
:p.:link reftype=launch object='netscape.exe ' data='http://service.boulder.ibm.com/dl/ddk/priv/ddk-d'.
http&colon.//service&per.boulder&per.ibm&per.com/dl/ddk/priv/ddk-d
:elink.:elines.
:h2 res=8 group=98 x=right y=bottom width=60% height=40%.http&colon.//www&per.edm2&per.com
:font facename=default size=0x0.
:p.:lines align=center.
Click below to launch Netscape with this URL&colon.
:font facename='System VIO' size=14x8.
:p.:link reftype=launch object='netscape.exe ' data='http://www.edm2.com'.
http&colon.//www&per.edm2&per.com
:elink.:elines.
:h2 res=30 group=98 x=right y=bottom width=60% height=40%.http&colon.//www&per.ozemail&per.com&per.au/~dbareis/
:font facename=default size=0x0.
:p.:lines align=center.
Click below to launch Netscape with this URL&colon.
:font facename='System VIO' size=14x8.
:p.:link reftype=launch object='netscape.exe ' data='http://www.ozemail.com.au/~dbareis/'.
http&colon.//www&per.ozemail&per.com&per.au/~dbareis/
:elink.:elines.
:euserdoc.
