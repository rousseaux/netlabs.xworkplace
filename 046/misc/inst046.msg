<!-- English message file for install.cmd
    DO NOT CHANGE, or the install program will not work properly. -->

<TOPIC NAME="IdentifyMsg">Please enter 001 to install in English.
</TOPIC>

<TOPIC NAME="welcomeMsg">
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³ Welcome to XWorkplace!                              ³
ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
XWorkplace replaces a whole number of WPS classes to improve your Workplace
Shell.

This script will NOT copy any files to a different location.
It will ONLY register or de-register the XWorkplace classess with the
Workplace Shell. For this, XFLDR.DLL in the ./BIN subdirectory will be used.
If you wish to install from a different directory, please abort the install
program now by pressing Ctrl+C and copy the folder containing this install
script to a different location first. XWorkplace neither changes CONFIG.SYS
nor does it copy any files into the OS/2 system directories.

WARNING: This is a temporary release for testing and debugging.
DO NOT USE this version on production machines. There are definitely bugs
in this release, so beware.

Now press any key to continue or Ctrl+C to stop. ></TOPIC>

<TOPIC NAME="licenceMsg">
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³ Terms And Conditions                                ³
ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ

XWorkplace is placed under the terms and conditions set by the GNU Public
Licence agreement. By installing XWorkplace on your system, you agree to the
terms and conditions set by this agreement.

There is NO WARRANTY whatsoever.

Please read through the "Notices" section of the User Guide (XFLDR001.INF)
to find out more about this.

It is strongly recommended that you also read through the "Installation"
section of the same document, especially if you are updating from a
previous XFolder installation.

Now press any key to begin or Ctrl+C to stop. ></TOPIC>

<TOPIC NAME="queryActionMsg">
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³ Select desired action                               ³
ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
This install script can do the following:

    (R) Register the XWorkplace WPS classes from the current directory.
        This will enhance your system after you restart the WPS.

        Please read through the "Installation" section of
        the XWorkplace User Guide (XFLDR001.INF) first before
        registering the classes.

    (D) De-register XWorkplace . This will restore the original WPS
        behavior. No files will be deleted.

    (X) Exit without doing anything.

Press "R", "D", or "X" now. ></TOPIC>

<TOPIC NAME="shutdownMsg">
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³ Restart WPS                                         ³
ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
If everything has worked out fine, we can now restart the WPS
automatically. This is neccessary to apply the changes made.
Doing this will ONLY restart the WPS without restarting OS/2
completely.

Press "Y" if you wish to restart the WPS. No data should be lost.
Press "N" if you don't wish to restart the WPS. You will then
have to shutdown and reboot yourself.

NOTE: Some users have reported that the WPS restart is not
working on their systems. Although it does most of the time,
if problems occur, you can always press Ctrl+Alt+Del, reinstall
XFolder and then shut down your system.

Restart the WPS now? ></TOPIC>

<TOPIC NAME="unregisteringOldMsg">
We will now check the WPS class list for previous versions of XFolder.
If any error messages occur, don't worry. The install script is
simply going for any classes that XFolder might have installed in
previous versions, and if an error is reported, the class was not
installed.

Press any key to continue or Ctrl+C to stop. ></TOPIC>

<TOPIC NAME="registeringNewMsg">
We will now register the classes of this XWorkplace version with the
WPS class list. This time, there should be no errors.

Press any key to continue or Ctrl+C to stop. ></TOPIC>

<TOPIC NAME="creatingObjMsg">
Now creating default configuration folder...
</TOPIC>

<TOPIC NAME="exitMsg">
Press any key to exit the install script now.></TOPIC>

<TOPIC NAME="howtoMsg">
You have chosen not to restart the Workplace Shell.
Before XWorkplace will function, you must shut down and reboot your
computer.
Now press any key to exit this installation script. ></TOPIC>

<TOPIC NAME="QueryODMsg">
We have found the WPS class "TSEnhFolder" on your system, which is
installed by Object Desktop to allow certain folder enhancements.
According to reports of XFolder users, XFolder has certain problems
cooperate well with Object Desktop.
We can now change the order of the "TSEnhFolder" class in the WPS
class list. This will ensure that XFolder is loaded before this class
so that it will be able to access certain WPS resources properly.
This should not limit any of the Object Desktop features.
Change the order of the "TSEnhFolder" class? (Y/N) ></TOPIC>

<TOPIC NAME="ODDeregdMsg">
The order of the "TSEnhFolder" class was changed. If something
went wrong, you may re-enable this class by executing "OD.CMD"
in the "BIN" subdirectory of XFolder.
Press any key to continue. ></TOPIC>

<TOPIC NAME="removeIniMsg">
Do you wish to also remove the XWorkplace settings from the system's
INI files? This will affect all the XWorkplace Global Settings you
have specified in the different settings objects.
If you do not plan to re-install XWorkplace, this is recommended to
save space in your INI files. However, in doing so, all of your
XWorkplace settings will be lost.
Remove XWorkplace settings? (Y/N) ></TOPIC>


