
/*
 *@@setup.h:
 *      common include file for _all_ XWorkplace code
 *      files (src\main and src\helpers) which defines
 *      common flags for debugging etc.
 *
 *      If this file is changed, this will cause the
 *      makefiles to recompile _all_ XWorkplace sources,
 *      because this is included will all source files.
 *
 *      Include this _after_ os2.h and standard C includes
 *      (stdlib.h et al), but _before_ any project includes,
 *      because this modifies some standard definitions.
 */

#ifdef SETUP_HEADER_INCLUDED
    #error setup.h included twice.
#else
    #define SETUP_HEADER_INCLUDED

    /*************************************************************
     *
     *   API wrappers
     *
     *************************************************************/

    // All these have been added with V0.9.12 (2001-05-18) [umoeller].
    // If you run into trouble with these in one of your code files,
    // after including setup.h, #undef the respective flag.

    // enable wrappers in include\helpers\winh.h
    #define WINH_STANDARDWRAPPERS

    /*************************************************************
     *
     *   Debug info setup
     *
     *************************************************************/

    /*
     *  The following #define's determine whether additional debugging
     *  info will be compiled into xfldr.dll.
     *
     *  1)  The general DONTDEBUGATALL flag will disable all other
     *      debugging flags, if defined. DONTDEBUGATALL gets set
     *      automatically if __DEBUG__ is not defined (that is,
     *      if DEBUG is disabled in setup.in).
     *
     *      Alternatively, set DONTDEBUGATALL explicitly here to
     *      disable the debugging flags completely. Of course, that
     *      does not affect compiler options.
     *
     *  2)  If DONTDEBUGATALL is not set (i.e. if we're in debug mode),
     *      the various flags below are taken into account. Note that
     *      many of these are from very old XFolder versions, and
     *      I cannot guarantee that these will compile any more.
     */

    // disable debugging if debug code is off
    #ifndef __DEBUG__
        #define DONTDEBUGATALL
    #endif

    // or set it here explicitly even though debugging is on:
        // #define DONTDEBUGATALL

    #ifndef DONTDEBUGATALL

        // If the following is commented out, no PMPRINTF will be
        // used at all. XWorkplace uses Dennis Bareis' PMPRINTF
        // package to do this.

        // **** IMPORTANT NOTE: if you use this flag, you _must_
        // have the PMPRINTF DLLs somewhere on your LIBPATH, or
        // otherwise XFLDR.DLL cannot be loaded, because the imports
        // will fail. That is, XWorkplace classes can neither be registered
        // nor loaded at WPS startup. This has cost me a lot of thought
        // once, and you'll get no error message, so be warned.
            #define _PMPRINTF_

        // The following replaces the SOMMethodDebug macros with
        // a PMPRINTF version. This leads to a LOT of output for
        // each SOM method called from all the XWorkplace files and
        // slows down the system _very_ much if the PMPRINTF output
        // wnd is open.
            // #define DEBUG_SOMMETHODS

        // the following will show a dumb message box when XWPSetup
        // is opened to check whether all classes have properly
        // registered themselves in THREADGLOBALS
            // #define DEBUG_XWPSETUP_CLASSES

        // the following writes xfldtrap.log even for "quiet"
        // exceptions, i.e. those handled by excHandlerQuiet
            // #define DEBUG_WRITEQUIETEXCPT

        // The following printfs about folder context menus.
            // #define DEBUG_CONTEXT

        // the following allows debug mode for XShutdown, which
        // will be enabled by holding down SHIFT while selecting
        // "Shutdown..." from the desktop context menu. In addition,
        // you'll get some PMPRINTF info and beeps
            // #define DEBUG_SHUTDOWN

        // the following beeps when thread priorities change
            // #define DEBUG_PRIORITY

        // the following printf's each added awake object
            // #define DEBUG_AWAKEOBJECTS

        // the following gives information on ordered folder content
        // (sorting by .ICONPOS etc.)
            // #define DEBUG_ORDEREDLIST

        // the following displays internal status bar data
            // #define DEBUG_STATUSBARS

        // the following will printf all kinds of settings
        // notebook information
            // #define DEBUG_SETTINGS

        // the following will printf WM_CONTROL for WPS cnrs
            // #define DEBUG_CNRCNTRL

        // the following will printf wpAddToContent
            // #define DEBUG_CNRCONTENT

        // the following will printf lots of sort info
            #define DEBUG_SORT

        // the following will printf language code queries and
        // NLS DLL evaluation
            // #define DEBUG_LANGCODES

        // the following will printf folder/global hotkey info
            // #define DEBUG_KEYS

        // the following displays XWorkplace memory usage in the
        // "Object internals" of the Desktop settings; this
        // will produce additional debug code for malloc(),
        // so this better only be used for debugging
            // #define DEBUG_MEMORY

        // the following beeps when the Worker thread cleans up
        // the default heap
            // #define DEBUG_MEMORYBEEP

        // the following displays a lot of infos about menu
        // processing (msgs), esp. for folder content menus
            // #define DEBUG_MENUS

        // debug startup (folder, archives) processing
            // #define DEBUG_STARTUP

        // debug title clash dialog
            // #define DEBUG_TITLECLASH

        // debug new system sounds
            // #define DEBUG_SOUNDS

        // debug data/program file associations/icons
            // #define DEBUG_ASSOCS

        // debug folder icon replacements
            // #define DEBUG_FLDRICONS

        // debug wpRestoreData and such
            // #define DEBUG_RESTOREDATA

        // debug notebook.c callbacks
            // #define DEBUG_NOTEBOOKS

        // debug trashcan
            // #define DEBUG_TRASHCAN
    #endif

    /*************************************************************
     *
     *   Common helpers declarations
     *
     *************************************************************/

    // XWPEXPORT defines the standard linkage for the
    // XWorkplace helpers.

    #ifdef __EMX__
        #define XWPENTRY
    #else
        // VAC:
        #if defined(__IBMC__) || defined(__IBMCPP__)
            #define XWPENTRY _Optlink
        #endif
    #endif

    /********************************************************************
     *
     *   Global #include's
     *
     ********************************************************************/

    #ifdef OS2_INCLUDED
        // the following reacts to the _PMPRINTF_ macro def'd above;
        // if that's #define'd, _Pmpf(()) commands will produce output,
        // if not, no code will be produced.
        #include "helpers\pmprintf.h"

        // SOMMethodDebug is the macro defined for all those
        // xxxMethodDebug thingies created by the SOM compiler.
        // If you have uncommended DEBUG_SOMMETHODS above, this
        // will make sure that _Pmpf is used for that. In order
        // for this to work, you'll also need _PMPRINTF_.
        #ifdef SOMMethodDebug
            #undef  SOMMethodDebug
        #endif

        #ifdef DEBUG_SOMMETHODS
            #define  SOMMethodDebug(c,m) _Pmpf(("%s::%s", c,m))
        #else
            #define  SOMMethodDebug(c,m) ;
        #endif
    #endif

    #define _min(a,b) ( ((a) > (b)) ? b : a )
    #define _max(a,b) ( ((a) > (b)) ? a : b )

    // all this added V0.9.2 (2000-03-10) [umoeller]
    #if ( defined (  __IBMCPP__ ) && (  __IBMCPP__ < 400 ) )
        typedef int bool;
        #define true 1
        #define false 0
        #define _BooleanConst    // needed for some VAC headers, which define bool also
    #endif

    #ifndef __stdlib_h          // <stdlib.h>
        #include <stdlib.h>
    #endif
    #ifndef __string_h          // <string.h>
        #include <string.h>
    #endif

    #ifdef __DEBUG__
        // enable memory debugging; comment out this line
        // if you don't want it
        // #define __XWPMEMDEBUG__

        #include "helpers\memdebug.h"
    #endif
#endif

