
#ifndef DEBUG_H
#define DEBUG_H

#define NEWLINE "\n"

#define ISTRUE(b)  (b ? "TRUE" : "FALSE")
#define ISSTR(s)   (s ? s : "(null)")

#ifdef DEBUG
#define _c_ ,
#define DEBUGMSG(str, parms) printf(str,parms)
#define FUNCENTER()          printf("%s() ->" NEWLINE,__FUNCTION__)
#define FUNCEXIT(rc)         printf("%s() <- rc=%u" NEWLINE, __FUNCTION__, rc)
#define DEBUGLABEL()         printf("%s() - line %u" NEWLINE, __FUNCTION__, __LINE__)
#else
#define _c_
#define DEBUGMSG(str, parms)
#define FUNCENTER()
#define FUNCEXIT(rc)
#define DEBUGLABEL()
#endif

#define SET_APPNAME               "WPAMPTR."
#define SET_INACTIVEANIMATETHREAD SET_APPNAME"ANIMATETHREAD"

#define SET_ANIMTHREADPRIORITY    SET_APPNAME"ANIMTHREADPRIORITY"
#define SET_ANIMCMDASYNC          SET_APPNAME"ANIMCMDASYNC"
#define SET_ANIM9TIMER            SET_APPNAME"ANIM9TIMER"
#define SET_ANIMINITDELAY         SET_APPNAME"ANIMINITDELAY"
#define SET_ANIMINITPRIORITY      SET_APPNAME"ANIMINITPRIORITY"

#define DEBUGSETTING(s)      (getenv( s) != NULL)
#define DEBUGSETTINGCMP(s,v) ((getenv( s) != NULL) && (!stricmp(getenv( s), v)))
#define DEBUGSETTINGVAL(s)   (getenv( s))


#define BEEP_END DosSleep(10);
#define BEEP  DosBeep(500, 50); BEEP_END
#define BEEP1 BEEP
#define BEEP2 BEEP BEEP
#define BEEP3 BEEP BEEP BEEP
#define BEEP4 BEEP BEEP BEEP BEEP

#endif // DEBUG_H
