@echo off

REM This will rename lots of HTML source files for the
REM XFolder Online Reference and update the links in the
REM HTML sources also.
REM For this to work, /helpers/strrpl.exe must be on your PATH.

REM change the following command to the directory where this
REM script should change to (containing all the INF HTML sources)
SET SUBDIR=inf.001

echo Warning: This script will rename a lot of files in the %SUBDIR% directory.
echo Please read the comments in this script to find out more.
echo Press Ctrl+C to abort now or any other key to continue.
PAUSE > NUL

CD %SUBDIR%

REM This command is for deleting all .LONGNAME EAs. If you
REM don't have my CommandPak utilities, you can delete this
REM line. But make sure the .LONGNAMEs are delete in some
REM other way, or the WPS might still display the old filenames.
call xren.cmd -dL *

del 065configure.html

ren config_2menu.html menu_3config.html
ren config_2wpprogram.html menu_4wpprogram.html
ren config_3order.html menu_5order.html
ren config_61statusbars.html fldr_32statusbars.html
ren config_6settings.html sys_2global.html
ren config_7settings.html fldr_7settings.html
ren config_8sounds.html 064intro_sounds.html

ren intro_10menu.html menu_1default.html
ren intro_21template.html menu_2template.html
ren intro_22favorite.html menu_6favorite.html
ren intro_23sort.html fldr_38sort.html
ren intro_24selectsome.html menu_7selectsome.html
ren intro_25snaptogrid.html menu_8snaptogrid.html
ren intro_26copyfilename.html menu_91copyfilename.html
ren intro_3icon.html fldr_1icon.html
ren intro_4fullpath.html fldr_2fullpath.html
ren intro_4statusbars.html fldr_31statusbars.html
ren intro_4treeviews.html fldr_4treeviews.html
ren intro_4xelerators.html fldr_5xelerators.html
ren intro_5quickopen.html fldr_5quickopen.html

ren intro_6restartwps.html xsd_1restartwps.html
ren intro_6xshutdown.html xsd_2xshutdown.html
ren intro_7folders.html xsd_3folders.html
ren intro_8cmdline.html xsd_4cmdline.html

ren intro_900system.html sys_1intro.html
ren intro_911wpsclasses.html sys_4wpsclasses.html
ren intro_921kernel1.html sys_5scheduler.html
ren intro_922kernel2.html sys_6memory.html
ren intro_923kernel3.html sys_7filesys.html

strrpl *.html config_2menu.html menu_3config.html
strrpl *.html config_2wpprogram.html menu_4wpprogram.html
strrpl *.html config_3order.html menu_5order.html
strrpl *.html config_61statusbars.html fldr_32statusbars.html
strrpl *.html config_6settings.html sys_2global.html
strrpl *.html config_7settings.html fldr_7settings.html
strrpl *.html config_8sounds.html 064intro_sounds.html

strrpl *.html intro_10menu.html menu_1default.html
strrpl *.html intro_21template.html menu_2template.html
strrpl *.html intro_22favorite.html menu_6favorite.html
strrpl *.html intro_23sort.html fldr_38sort.html
strrpl *.html intro_24selectsome.html menu_7selectsome.html
strrpl *.html intro_25snaptogrid.html menu_8snaptogrid.html
strrpl *.html intro_26copyfilename.html menu_91copyfilename.html
strrpl *.html intro_3icon.html fldr_1icon.html
strrpl *.html intro_4fullpath.html fldr_2fullpath.html
strrpl *.html intro_4statusbars.html fldr_31statusbars.html
strrpl *.html intro_4treeviews.html fldr_4treeviews.html
strrpl *.html intro_4xelerators.html fldr_5xelerators.html
strrpl *.html intro_5quickopen.html fldr_5quickopen.html

strrpl *.html intro_6restartwps.html xsd_1restartwps.html
strrpl *.html intro_6xshutdown.html xsd_2xshutdown.html
strrpl *.html intro_7folders.html xsd_3folders.html
strrpl *.html intro_8cmdline.html xsd_4cmdline.html

strrpl *.html intro_900system.html sys_1intro.html
strrpl *.html intro_911wpsclasses.html sys_4wpsclasses.html
strrpl *.html intro_921kernel1.html sys_5scheduler.html
strrpl *.html intro_922kernel2.html sys_6memory.html
strrpl *.html intro_923kernel3.html sys_7filesys.html








