#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INCL_ERRORS
#define INCL_PM
#define INCL_DOS
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options


#include "pointers\wpamptr.h"
#include "pointers\anwcur.h"
#include "pointers\debug.h"


#ifdef __IBMC__
#pragma linkage (SOMInitModule, system)
#endif

SOMEXTERN void SOMLINK SOMInitModule(long majorVersion,
                                     long minorVersion, string className)
{
    FUNCENTER();

    SOM_IgnoreWarning(majorVersion);    /* This function makes */
    SOM_IgnoreWarning(minorVersion);    /* no use of the passed */
    SOM_IgnoreWarning(className);   /* arguments.   */
    XWPWinCursorNewClass(XWPWinCursor_MajorVersion, XWPWinCursor_MinorVersion);
    WPAnimatedMousePointerNewClass(WPAnimatedMousePointer_MajorVersion, WPAnimatedMousePointer_MinorVersion);
}
