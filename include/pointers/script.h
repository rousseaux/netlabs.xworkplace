
#ifndef SCRIPT_H
#define SCRIPT_H

#include "pointers\info.h"

BOOL LoadPointerFromAnimouseScriptFile(PSZ pszName,
                                       ULONG ulPointerIndex,
                                       PHPOINTER pahptr,
                                       PICONINFO paiconinfo,
                                       PHPOINTER phptrStatic,
                                       PICONINFO piconinfoStatic,
                                       PULONG paulTimeout,
                                       PULONG pulEntries);

#endif // SCRIPT_H

